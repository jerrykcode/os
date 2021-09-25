#include "fork.h"
#include "thread.h"
#include "memory.h"
#include "global.h"
#include "process.h"
#include "file.h"
#include "debug.h"

extern void intr_exit(void);

static pid_t update_stack0(struct task_st *child_thread) {
    child_thread->parent_pid = child_thread->pid; // child_thread的内容是父进程复制过来的
    child_thread->pid = fork_pid();
    child_thread->status = TASK_READY;
    child_thread->ticks = child_thread->priority; // 时间片充满
    child_thread->thread_node.pre = child_thread->thread_node.next = NULL;
    child_thread->thread_ready_node.pre = child_thread->thread_ready_node.next = NULL;
    return child_thread->pid;
}

/* 更新子进程的页表，为每个虚拟地址分配一页物理地址并安装到页表，
   并将父进程这个虚拟地址指向的内容复制到子进程这个虚拟地址指向的内容
   使用内核内存buf_page来做传输中介 */
static void update_page_table(struct task_st *child_thread, struct task_st *parent_thread, void *buf_page) {
    uint32_t btmp_bytes_len = parent_thread->usrprog_vaddr.vaddr_btmp.btmp_bytes_len;
    uint8_t *bits = parent_thread->usrprog_vaddr.vaddr_btmp.bits;
    uint32_t vaddr_start = parent_thread->usrprog_vaddr.vaddr_start;

    void *vaddr;
    // 通过位图枚举父进程使用的每一个虚拟地址
    for (int i = 0; i < btmp_bytes_len; i++) {
        if (bits[i]) {
            for (int j = 0; j < 8; j++) {
                if (bits[i] & (1 << j)) {
                    vaddr = (i << 3 + (7 - j)) * PAGE_SIZE + vaddr_start; // 使用的一个虚拟地址
                    memcpy(buf_page, vaddr, PAGE_SIZE);
                    process_active(child_thread); // 改动，让cr3寄存器指向子进程的页表
                    // 为子进程的虚拟地址vaddr申请一页物理地址并安装到子进程的页表
                    malloc_page_for_vaddr_in_fork(PF_USER, vaddr);
                    memcpy(vaddr, buf_page, PAGE_SIZE);
                    // cr3寄存器重新指向父进程页表
                    process_active(parent_thread);
                }
            }
        }
    }
}

static void update_intr_stack(struct task_st *child_thread) {
    struct intr_stack *stack = (struct intr_stack *)((uint32_t)child_thread + PAGE_SIZE - sizeof(struct intr_stack)); // 中断栈
    stack->eax = 0; // 退出中断后eax为0，也就是函数返回值为0，对fork来说表示子进程

    *((uint32_t *)stack - 1) = intr_exit; // ret
    *((uint32_t *)stack - 2) = 0;         // esi
    *((uint32_t *)stack - 3) = 0;         // edi
    *((uint32_t *)stack - 4) = 0;         // ebx
    *((uint32_t *)stack - 5) = 0;         // ebp

    child_thread->self_stack = (uint32_t *)stack - 5; // 栈指针

    // 加入就绪队列
    list_push_back(&threads_all, &child_thread->thread_node);
    list_push_back(&threads_ready, &child_thread->thread_ready_node);
}

static void update_inode_open_cnts(struct task_st *thread) {
    int32_t local_fd, global_fd;
    for (local_fd = 0; local_fd < MAX_FILES_OPEN_PER_PROC; local_fd++) {
        global_fd = thread->fd_table[local_fd];
        ASSERT(global_fd < MAX_FILE_OPEN);
        file_table[global_fd].fd_inode->i_open_cnts++;
    }
}

pid_t sys_fork(void) {
    struct task_st *parent_thread = current_thread(); // 当前运行的作为父进程
    struct task_st *child_thread = malloc_kernel_page(1); // 申请一页内核内存作为子进程的内核栈
    if (child_thread == NULL)
        return -1;
    uint32_t rollback = 0;
    memcpy(child_thread, parent_thread, PAGE_SIZE); // 父进程的内核栈内容复制给子进程的内核栈
    pid_t child_pid = update_stack0(child_thread); // 更新子进程内核栈的属性

    uint32_t btmp_pages_num = DIV_ROUND_UP((0xc0000000 - USR_VADDR_START) / PAGE_SIZE / 8, PAGE_SIZE); // 虚拟地址位图需要占用的页数
    child_thread->usrprog_vaddr.vaddr_btmp.bits = malloc_kernel_page(btmp_pages_num); // 为子进程申请虚拟地址位图
    if (child_thread->usrprog_vaddr.vaddr_btmp.bits == NULL) {
        rollback = 1;
        goto ROLLBACK;
    }
    // 父进程的虚拟地址位图复制给子进程
    child_thread->usrprog_vaddr.vaddr_start = parent_thread->usrprog_vaddr.vaddr_start;
    memcpy(child_thread->usrprog_vaddr.vaddr_btmp.bits, parent_thread->usrprog_vaddr.vaddr_btmp.bits, btmp_pages_num * PAGE_SIZE);
    child_thread->usrprog_vaddr.vaddr_btmp.btmp_bytes_len = parent_thread->usrprog_vaddr.vaddr_btmp.btmp_bytes_len;

    child_thread->page_table = create_usr_page_table(); // 为子进程创建页表
    void *buf = malloc_kernel_page(1);
    if (buf == NULL) {
        k_printf("ERROR sys_fork: alloc memory failed!!!\n");
        rollback = 2;
        goto ROLLBACK;
    }
    update_page_table(child_thread, parent_thread, buf); // 为子进程更新页表
    pages_free(buf, 1);

    update_intr_stack(child_thread); // 为子进程更新其内核栈顶部的中断内容，并加入就绪队列

    update_inode_open_cnts(child_thread); // 更新inode打开计数
    return child_pid;

    // 出现错误才会执行回滚操作
ROLLBACK:
    switch (rollback) {
        case 2:
            pages_free(child_thread->usrprog_vaddr.vaddr_btmp.bits, btmp_pages_num);
            delete_usr_page_table(child_thread->page_table);
        case 1:
            pages_free(child_thread, 1);
    }
    return -1;
}
