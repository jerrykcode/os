#ifndef THREAD_H__
#define THREAD_H__
#include "stdint.h"
#include "list.h"
#include "memory.h"

typedef uint16_t pid_t;

typedef void (*thread_func)(void *);

#define STACK_MAGIC 0x20010201
#define MAX_FILES_OPEN_PER_PROC 8
#define TASK_NAME_LEN 20

struct list_st threads_all;
struct list_st threads_ready;

enum task_status {
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_WAITING,
    TASK_HANGING,
    TASK_DIED
};

// 此结构描述中断时压入栈的内容
struct intr_stack {
    uint32_t vec_no; // 中断向量号 kernel.S中压入
    // 以下是pushad压入的寄存器值
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    //
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;

    uint32_t error_code;
    void (*eip)(void);
    uint32_t cs;
    uint32_t eflags;
    void *esp;
    uint32_t ss;
};

// 此结构描述线程在TASK_READY状态时栈中的部分内容
// 这些内容用于从其他线程切换到本线程
//
// 对于新线程，thread_stack中的内容由thread_stack_init函数赋值
// 对于已执行的线程，thread_stack中的内容由call调用函数及push指令压入
struct thread_stack {
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;

    // 对于新线程:
    // ret_func[0] thread_func_entry函数地址
    // ret_func[1] thread_func_entry返回地址(目前无实用)
    // ret_func[2] thread_func_entry参数: 新线程函数地址
    // ret_func[3] thread_func_entry参数: 新线程函数参数(void *)
    //
    // 对于已执行的线程:
    // ret_func[0] switch_to函数的返回地址，返回到schedule函数
    // ret_func[1] switch_to的参数: cur(当前线程)
    // ret_func[2] switch_to的参数: next(新线程)
    // ret_func[3] 栈中的值，没有特别意义
    uint32_t ret_func[4];
};

// 任务(线程或者进程)
struct task_st {
    uint32_t *self_stack; // 栈指针
    pid_t pid; // 任务id
    pid_t parent_pid; // 父进程pid
    char name[TASK_NAME_LEN]; // 任务名
    enum task_status status; // 状态
    uint32_t ticks; // 剩余时间片
    uint32_t priority; // 优先级，即初始ticks
    int32_t fd_table[MAX_FILES_OPEN_PER_PROC]; // 文件描述符数组
    uint32_t *page_table; // 页表指针，线程的page_table为NULL
    struct virtual_addr usrprog_vaddr; // 用户进程的虚拟地址(用户虚拟内存位图)
    struct mem_block_desc usrprog_mem_block_descs[MEM_BLOCK_DESC_NUM]; // 用户进程的mem_block_desc(堆内存块描述符数组)
    struct list_node_st thread_node; // 任务队列节点
    struct list_node_st thread_ready_node; // 就绪队列节点
    uint32_t cwd_inode_id; // 进程所在的工作目录的inode号
    uint32_t stack_magic; // 判断栈溢出
};

void thread_environment_init();
pid_t fork_pid();
struct task_st *current_thread();
void task_init(struct task_st *task, char *name, enum task_status status, uint32_t priority);
struct task_st *thread_start(char *name, uint32_t priority, thread_func func, void *args);
void thread_stack_init(struct task_st *task, thread_func func, void *args);
void schedule();
void thread_block(enum task_status status);
void thread_unblock(struct task_st *pthread);
void thread_yield();
struct task_st *node_to_thread(list_node node);
void sys_ps();

#endif
