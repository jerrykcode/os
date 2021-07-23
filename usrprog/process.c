#include "process.h"
#include "thread.h"
#include "memory.h"
#include "list.h"
#include "stdint.h"
#include "interrupt.h"
#include "debug.h"
#include "asm.h"
#include "bitmap.h"
#include "global.h"
#include "stddef.h"
#include "string.h"

extern void intr_exit(void);

static void start_process(void *filename);
static void create_usr_vaddr_btmp(struct task_st *usrprog); 
static uint32_t *create_usr_page_table();
 
/* 创建用户进程，filename为用户程序地址 */
void process_execute(void *filename, char *name) {
    struct task_st *thread = malloc_kernel_page(1); // 申请内核页作为内核线程
    task_init(thread, name, TASK_READY, DEFAULT_PRIORITY); // 初始化内核线程
    create_usr_vaddr_btmp(thread); // 创建用户进程虚拟地址位图
    thread->page_table = create_usr_page_table(); // 创建用户进程页表
    mem_block_desc_init(thread->usrprog_mem_block_descs); // 初始化用户进程的堆内存块描述符数组
    thread_stack_init(thread, start_process, filename); // 将start_process作为内核线程执行的函数

    // 加入队列
    enum intr_status old_status = intr_disable();
    ASSERT(!list_exist(&threads_all, &thread->thread_node));
    list_push_back(&threads_all, &thread->thread_node);
    ASSERT(!list_exist(&threads_ready, &thread->thread_ready_node));
    list_push_back(&threads_ready, &thread->thread_ready_node);
    intr_set_status(old_status);
}

/* 调度时激活即将执行的进程或线程: 
    1. 更新cr3寄存器为新进程或线程的页表物理地址
    2. 若是进程，则更新TSS的esp0，即用户进程中断时的0特权级栈 */
void process_active(struct task_st *thread) {
    // 1. 更新cr3寄存器为新进程或线程的页表物理地址
    uint32_t page_table_phy_addr;
    if (thread->page_table == NULL) {
        // 内核线程，页表物理地址为0x100000 (1MB)
        page_table_phy_addr = 0x100000;
    }
    else {
        // 用户进程
        // 用户进程的page_table存储的是内核页表下的虚拟地址
        // 通过当前的内核页表 更新为物理地址
       page_table_phy_addr = vaddr2phy(thread->page_table);
    }
    // 更新cr3
    asm volatile ("movl %0, %%cr3" : : "r"(page_table_phy_addr) : "memory");

    // 2. 若是进程，则更新TSS的esp0，即用户进程中断时的0特权级栈
    if (thread->page_table) {
        update_tss_esp(thread);
    }
}

/* 为用户进程创建虚拟地址位图 */
static void create_usr_vaddr_btmp(struct task_st *usrprog) {
    usrprog->usrprog_vaddr.vaddr_start = USR_VADDR_START;
    uint32_t btmp_page_num = DIV_ROUND_UP((0xc0000000 - USR_VADDR_START) / PAGE_SIZE / 8, PAGE_SIZE);
    // 申请的是内核内存, 用户进程不能直接访问到
    usrprog->usrprog_vaddr.vaddr_btmp.bits = malloc_kernel_page(btmp_page_num);
    usrprog->usrprog_vaddr.vaddr_btmp.btmp_bytes_len = (0xc0000000 - USR_VADDR_START) / PAGE_SIZE / 8;
    bitmap_init(&usrprog->usrprog_vaddr.vaddr_btmp, BTMP_MEM_FREE);
}

/* 创建用户进程的页表 */
static uint32_t *create_usr_page_table() {
    uint32_t *page_table_vaddr = malloc_kernel_page(1); // 申请一页内核内存, 注意得到的是虚拟地址
    if (page_table_vaddr == NULL) {
        return NULL;
    }

    // 将内核页表的第 [768(0x300), 1022] 共255项复制到 新页表的[768, 1022]项, 以共享内核地址
    // 每项大小为4字节
    memcpy((uint32_t *)((uint32_t)page_table_vaddr + 0x300 * 4), (uint32_t *)(0xfffff000 + 0x300 * 4), 255 * 4);
    // 将内核页表第0项复制到新页表的第0项, 以共享内核地址
    memcpy(page_table_vaddr, (uint32_t *)0xfffff000, 4);

    // 将新页表的第1023项(最后一项)指向 新页表本身的物理地址，这样在新页表下，
    // kernel/memory.h 中的 pte_ptr和pde_pte函数 可以起作用
    uint32_t page_table_phy = vaddr2phy((uint32_t)page_table_vaddr); // 转换物理地址
    page_table_vaddr[1023] = page_table_phy | PG_US_U | PG_RW_W | PG_P_1;

    return page_table_vaddr;
}

static void start_process(void *filename) {
    void *function = filename;

    struct task_st *cur = current_thread();

    // 在当前线程栈顶制作一个struct intr_stack结构，制造出用户进程中断的假象
    // 最后通过 "jmp intr_exit" 退出中断 执行到用户进程代码
    cur->self_stack = (uint32_t *)((uint32_t)cur + PAGE_SIZE - sizeof(struct intr_stack));
    struct intr_stack *intr_stack = (struct intr_stack *)cur->self_stack;    
    // 为中断栈填充值
    intr_stack->ss = SELECTOR_U_DATA; // 用户数据段
    intr_stack->esp = (void *)((uint32_t)malloc_page_with_vaddr(PF_USER, USR_STACK3_VADDR) + PAGE_SIZE); // 用户进程3特权级栈
    intr_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);
    intr_stack->cs = SELECTOR_U_CODE; // 用户代码段
    intr_stack->eip = function;
    intr_stack->error_code = 0;
    intr_stack->ds = intr_stack->es = intr_stack->fs = SELECTOR_U_DATA;
    intr_stack->gs = 0; // 用户态不使用
    intr_stack->eax = intr_stack->ecx = intr_stack->edx = intr_stack->ebx
        = intr_stack->esp_dummy = intr_stack->ebp = intr_stack->esi = intr_stack->edi = 0; // 用户进程首次执行，这些寄存器值为0
    intr_stack->vec_no = 0x20;
    asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g"(cur->self_stack) : "memory"); // 退出中断
}
