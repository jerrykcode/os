#include "wait_exit.h"
#include "bitmap.h"
#include "global.h"
#include "stdbool.h"
#include "stddef.h"
#include "list.h"
#include "interrupt.h"

// 删除用户进程占有的资源
static void release_prog_resources() {
    struct task_st *cur = current_thread();
    void *page_table = cur->page_table;
    if (page_table == NULL)
        return;

    // 遍历页表，删除本进程占用的物理内存
    uint32_t *page_table_vaddr = (uint32_t *)page_table;
    uint32_t pde_phyaddr;
    uint32_t *pde_vaddr;
    uint32_t pte_phyaddr;

    for (int i = 1; i < 768; i++) { // 枚举页目录表page_table中存储的PDE
                                    // 第0项以及第[768, 1022]项存储的PDE是用于映射内核虚拟地址的页表，所有进程共享，不能删除
        pde_phyaddr = *(page_table_vaddr + i); // page_table中存储的PDE，这是一个物理地址，不能直接访问
        if ((pde_phyaddr & 0x00000001) == 0) // 页表不存在
            continue;
        pde_phyaddr &= 0xfffff000;

        // 虚拟地址pde_vaddr会被映射到物理地址pde_phyaddr
        // 0xffc00000的二进制高10位全部为1，作为虚拟地址的第一段
        // i << 12 作为虚拟地址的第二段
        // 虚拟地址的第三段先初始化为0, 之后的for循环会枚举第三段
        pde_vaddr = (uint32_t *)(0xffc00000 | (i << 12));

        for (int j = 0; j < 1024; j++) {

            pte_phyaddr = *(pde_vaddr + j); // PTE的物理地址
            if ((pte_phyaddr & 0x00000001) == 0)
                continue;
            pte_phyaddr &= 0xfffff000;

            // 这里不需要再计算PTE的虚拟地址，而直接从用户物理内存池的位图中删除PTE物理地址相应的位即可
            phy_page_free_in_exit(pte_phyaddr);
        }

        // 将PDE的物理页删除
        phy_page_free_in_exit(pde_phyaddr);
    }

    // 删除本进程的虚拟地址位图
    uint32_t bitmap_pages_num = DIV_ROUND_UP(cur->usrprog_vaddr.vaddr_btmp.btmp_bytes_len, PAGE_SIZE);
    cur->page_table = NULL; // 用pages_free函数删除内核内存要先将页表置为NULL
    pages_free(cur->usrprog_vaddr.vaddr_btmp.bits, bitmap_pages_num);
    cur->page_table = page_table;

    // 关闭打开的文件
    for (int i = 3; i < MAX_FILES_OPEN_PER_PROC; i++) {
        if (cur->fd_table[i] != -1) {
            sys_close(i);
        }
    }
}

// list_traversal回调函数，用于寻找ppid的子进程
static bool find_child(list_node node, int ppid) {
    struct task_st *thread = node_to_thread(node);
    if (thread->parent_pid == ppid) {
        return true;
    }
    return false;
}

//list_traversal回调函数，用于寻找ppid的死掉的子进程
static bool find_died_child(list_node node, int ppid) {
    struct task_st *thread = node_to_thread(node);
    if (thread->parent_pid == ppid && thread->status == TASK_DIED) {
        return true;
    }
    return false;
}

// list_traversal回调函数，用于把ppid的子进程全部过继给init进程
static bool init_adopt_children(list_node node, int ppid) {
    struct task_st *thread = node_to_thread(node);
    if (thread->parent_pid == ppid) {
        thread->parent_pid = 1;
    }
    return false;
}

/* 阻塞自己让子进程执行，等到子进程调用exit之后为子进程收尸，成功返回子进程的pid，失败返回-1
   child_exit_status用于保存子进程退出的状态 */
pid_t sys_wait(int32_t *child_exit_status) {
    struct task_st *cur = current_thread();
    pid_t pid = cur->pid;
    list_node node;
    struct task_st *child;
    pid_t child_pid;
    enum intr_status old_status;

    while (1) {
        old_status = intr_disable();
        node = list_traversal(&threads_all, find_died_child, pid); // 查找died子进程
        intr_set_status(old_status);

        if (node != NULL) {
            child = node_to_thread(node);
            // 记录子进程的pid
            child_pid = child->pid;
            // 记录子进程退出的状态
            *child_exit_status = child->exit_status;
            // 为子进程收尸
            thread_exit(child, false);
            // 成功为子进程收尸，返回子进程的pid
            return child_pid;
        }
        // 执行到这里说明目前没有子进程died

        old_status = intr_disable();
        node = list_traversal(&threads_all, find_child, pid); // 查找子进程
        intr_set_status(old_status);

        if (node == -1) {
            // 没有子进程，wait失败
            return -1;
        }
        thread_block(TASK_WAITING);
    }

}

/* 用来结束自己 
   唤醒父进程为自己收尸
   参数exit_status为退出的状态 */
void sys_exit(int32_t exit_status) {
    struct task_st *cur = current_thread();
    if (cur->pid == 1)
        return ;

    // 将子进程过继给init进程
    enum intr_status old_status = intr_disable();
    list_traversal(&threads_all, init_adopt_children, cur->pid);
    intr_set_status(old_status);

    // 删除分配的内存。位图。关闭文件
    release_prog_resources();

    // 修改状态及exit状态
    cur->status = TASK_DIED;
    cur->exit_status = exit_status;

    // 唤醒父进程
    struct task_st *parent = pid2thread(cur->parent_pid);
    thread_unblock(parent);
    // 父进程执行之后就会发现cur的status已经是TASK_DIED就会将cur彻底删除

    // 如果cur执行到这里父进程还没有删除cur，那就主动schedule切换到其他线程，
    // 由于cur的status是TASK_DIED，schedule()函数中不会将cur加到就绪队列，类似于阻塞的效果
    // cur再也不会被调度到了
    intr_disable();
    schedule();
    // cur再也不会执行了，真的死了
}
