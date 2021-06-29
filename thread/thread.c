#include "thread.h"
#include "list.h"
#include "print.h"
#include "asm.h"
#include "string.h"
#include "stddef.h"
#include "bitmap.h"
#include "memory.h"
#include "debug.h"
#include "interrupt.h"

extern void switch_to(struct task_st *cur, struct task_st *next);

struct list_st threads_all;
struct list_st threads_ready;

/* 将kernel中的main函数完善成为主线程 */
static void make_main_thread() {
    struct task_st *main_task = current_thread();
    task_init(main_task, "main", TASK_RUNNING, 31);
    list_push(&threads_all, &main_task->thread_node);
}

/* 初始化线程环境 */
void thread_environment_init() {
    put_str("thread_environment_init start\n");
    list_init(&threads_all);
    list_init(&threads_ready);
    make_main_thread();
    put_str("thread_environment_init end\n");
}

struct task_st *current_thread() {
    uint32_t esp;
    asm volatile ("movl %%esp, %0" : "=g" (esp)); // 获取当前栈顶
    return (struct task_st *)(esp & 0xfffff000); // esp所在页的起始地址即当前任务地址
}

void task_init(struct task_st *task, char *name, enum task_status status, uint32_t priority) {
    strcpy(task->name, name);
    task->status = status;
    task->priority = priority;
    task->ticks = priority;
    task->page_table = NULL;
    task->stack_magic = STACK_MAGIC;
}

/* 启动新线程 */
void thread_start(char *name, uint32_t priority, thread_func func, void *args) {
    struct task_st *task = malloc_kernel_page(1); // 1页内存，从内核内存池申请
    task_init(task, name, TASK_READY, priority);
    thread_stack_init(task, func, args);
    list_push(&threads_all, &task->thread_node);
    list_push(&threads_ready, &task->thread_ready_node);
}

void thread_func_entry(thread_func func, void *args) {
    intr_enable();
    func(args);
}

void thread_stack_init(struct task_st *task, thread_func func, void *args) {
    struct thread_stack *stack = (struct thread_stack *)((uint32_t)task + PAGE_SIZE - sizeof(struct thread_stack));
                                                    // task起始地址 + 页大小 - thread_stack结构大小

    task->self_stack = (uint32_t *)stack;

    stack->ebp = 0;
    stack->ebx = 0;
    stack->edi = 0;
    stack->esi = 0;

    // 对于新线程:
    // ret_func[0] thread_func_entry函数地址
    // ret_func[1] thread_func_entry返回地址(目前无实用)
    // ret_func[2] thread_func_entry参数: 新线程函数地址
    // ret_func[3] thread_func_entry参数: 新线程函数参数(void *)
    stack->ret_func[0] = (uint32_t)thread_func_entry;
    stack->ret_func[1] = 0x00000000;
    stack->ret_func[2] = (uint32_t)func;
    stack->ret_func[3] = (uint32_t)args;
}

void schedule() {

    put_str("\n##############################################\n\n SCHEDULE!!!! \n\n#####################################################\n");

    ASSERT(intr_get_status() == INTR_OFF);

    struct task_st *cur = current_thread();
    if (cur->status == TASK_RUNNING) {
        // 运行态的线程时间片耗尽
        cur->ticks = cur->priority;
        cur->status = TASK_READY;
        list_push(&threads_ready, &cur->thread_ready_node);
    }
    else {
        // 线程阻塞
    }

    list_node node = list_pop(&threads_ready);
    struct task_st *next = (struct task_st *)((uint32_t)node & 0xfffff000);
    next->status = TASK_RUNNING;

    switch_to(cur, next);
}
