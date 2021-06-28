#ifndef THREAD_H__
#define THREAD_H__
#include "stdint.h"
#include "list.h"

typedef void (*thread_func)(void *);

#define STACK_MAGIC 0x20010201

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
    uint32_t ed;

    uint32_t error_code;
    void (*eip)(void);
    uint32_t cs;
    uint32_t eflags;
    void *esp;
    uint32_t ss;
};

// 此结构描述线程在TASK_READY状态时栈中的部分内容
// 这些内容用于从其他线程切换到本线程
struct thread_stack {
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;

    // 对于新线程:
    // ret_func[0] 新线程函数入口地址
    // ret_func[1] 新线程函数的返回地址(无实用)
    // ret_func[2] 新线程函数参数: void *类型
    //
    // 对于已执行的线程:
    // ret_func[0] switch_to函数的返回地址，返回到schedule函数
    // ret_func[1] schedule函数的返回地址，返回到timer函数
    // ret_func[2] timer函数的返回地址，返回到中断entry函数
    uint32_t ret_func[3];
};

// 任务(线程或者进程)
struct task_st {
    uint32_t *self_stack; // 栈指针
    char name[20]; // 任务名
    enum task_status status; // 状态
    uint32_t ticks; // 剩余时间片
    uint32_t priority; // 优先级，即初始ticks
    uint32_t *page_table; // 页表指针，线程的page_table为NULL
    struct list_node_st thread_node; // 任务队列节点
    struct list_node_st thread_ready_node; // 就绪队列节点
    uint32_t stack_magic; // 判断栈溢出
};

void thread_environment_init();
struct task_st *current_thread();
void task_init(struct task_st *task, char *name, enum task_status status, uint32_t priority);
void thread_start(char *name, uint32_t priority, thread_func func, void *args);
void thread_stack_init(struct task_st *task, thread_func func, void *args);
void schedule();

#endif
