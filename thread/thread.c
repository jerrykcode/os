#include "thread.h"
#include "sync.h"
#include "list.h"
#include "print.h"
#include "asm.h"
#include "string.h"
#include "stddef.h"
#include "stdbool.h"
#include "bitmap.h"
#include "memory.h"
#include "debug.h"
#include "interrupt.h"
#include "process.h"
#include "global.h"

extern void switch_to(struct task_st *cur, struct task_st *next);

/* 主线程 */
struct task_st *main_thread;

/* 系统空闲时运行的线程 */
struct task_st *idle_thread;

static void idle_thread_func(void *arg) {
    while (1) {
        thread_block(TASK_BLOCKED);
        asm volatile ("sti; hlt" : : : "memory");
    }
}

/* 分配pid */

// pid 池
struct pid_pool_st {
    struct bitmap pool_btmp;
    uint32_t pid_start;
    struct lock_st *pool_lock;
};

struct pid_pool_st pid_pool;

uint8_t pid_pool_btmp_bits[128] = {0}; // 最多1024个

static void pid_pool_init() {
    pid_pool.pool_btmp.bits = pid_pool_btmp_bits;
    pid_pool.pool_btmp.btmp_bytes_len = 128;
    bitmap_init(&pid_pool.pool_btmp, 0);
    pid_pool.pid_start = 1;
    lock_init(&pid_pool.pool_lock);
}

static pid_t alloc_pid() {
    lock_acquire(&pid_pool.pool_lock);
    pid_t pid = bitmap_alloc(&pid_pool.pool_btmp, 1, 0);
    bitmap_setbit(&pid_pool.pool_btmp, pid, 1);
    lock_release(&pid_pool.pool_lock);
    return pid + pid_pool.pid_start;
}

void release_pid(pid_t pid) {
    int bit_idx = pid - pid_pool.pid_start;
    lock_acquire(&pid_pool.pool_lock);
    bitmap_setbit(&pid_pool.pool_btmp, bit_idx, 0);
    lock_release(&pid_pool.pool_lock);
}

/* 为fork操作分配pid */
pid_t fork_pid() {
    return alloc_pid();
}

/* 将kernel中的main函数完善成为主线程 */
static void make_main_thread() {
    main_thread = current_thread();
    task_init(main_thread, "main", TASK_RUNNING, 31);
    list_push_back(&threads_all, &main_thread->thread_node);
}

/* 初始化线程环境 */
void thread_environment_init() {
    put_str("thread_environment_init start\n");
    list_init(&threads_all);
    list_init(&threads_ready);
    pid_pool_init();
    make_main_thread();
    idle_thread = thread_start("idle", 10, idle_thread_func, NULL);
    put_str("thread_environment_init end\n");
}

struct task_st *current_thread() {
    uint32_t esp;
    asm volatile ("movl %%esp, %0" : "=g" (esp)); // 获取当前栈顶
    return (struct task_st *)(esp & 0xfffff000); // esp所在页的起始地址即当前任务地址
}

void task_init(struct task_st *task, char *name, enum task_status status, uint32_t priority) {
    task->pid = alloc_pid();
    task->parent_pid = -1; // 初始化默认为-1，即没有父进程
    strcpy(task->name, name);
    task->status = status;
    task->priority = priority;
    task->ticks = priority;
    task->fd_table[0] = 0;
    task->fd_table[1] = 1;
    task->fd_table[2] = 2;
    for (int i = 3; i < MAX_FILES_OPEN_PER_PROC; i++) {
        task->fd_table[i] = -1;
    }
    task->page_table = NULL;
    task->cwd_inode_id = 0; // 以根目录为默认工作目录
    task->stack_magic = STACK_MAGIC;
}

/* 启动新线程 */
struct task_st *thread_start(char *name, uint32_t priority, thread_func func, void *args) {
    struct task_st *task = malloc_kernel_page(1); // 1页内存，从内核内存池申请
    task_init(task, name, TASK_READY, priority);
    thread_stack_init(task, func, args);
    list_push_back(&threads_all, &task->thread_node);
    list_push_back(&threads_ready, &task->thread_ready_node);
    return task;
}

void thread_func_entry(thread_func func, void *args) {
    intr_enable();
    func(args);
}

void thread_stack_init(struct task_st *task, thread_func func, void *args) {
    struct thread_stack *stack;

    if (task->page_table == NULL) {
        // 线程
        stack = (struct thread_stack *)((uint32_t)task + PAGE_SIZE - sizeof(struct thread_stack));
                                                    // task起始地址 + 页大小 - thread_stack结构大小
    }
    else {
        // 用户进程
        stack = (struct thread_stack *)((uint32_t)task + PAGE_SIZE 
            - sizeof(struct intr_stack) - sizeof(struct thread_stack)); // 预留intr_stack的空间
    }

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

//    put_str("\n##############################################\n\n SCHEDULE!!!! \n\n#####################################################\n");

    ASSERT(intr_get_status() == INTR_OFF);

    struct task_st *cur = current_thread();
    if (cur->status == TASK_RUNNING) {
        // 运行态的线程时间片耗尽
        cur->ticks = cur->priority;
        cur->status = TASK_READY;
        list_push_back(&threads_ready, &cur->thread_ready_node);
    }
    else {
        // 线程阻塞或者yield
    }

    if (list_empty(&threads_ready)) {
        // 系统没有就绪态的线程，运行idle线程
        thread_unblock(idle_thread);
    }

    ASSERT(!list_empty(&threads_ready));
    list_node node = list_pop(&threads_ready);
    struct task_st *next = node_to_thread(node);
    next->status = TASK_RUNNING;

    process_active(next); // 更新cr3为next的页表物理地址。并在next为用户进程时更新TSS中的esp0

    switch_to(cur, next);
}

/* 阻塞当前线程 */
void thread_block(enum task_status status) {
    ASSERT(status == TASK_BLOCKED || status == TASK_WAITING || status == TASK_HANGING);
    if (status == TASK_BLOCKED || status == TASK_WAITING || status == TASK_HANGING) {
        struct task_st *cur = current_thread();
        enum intr_status old_intr_status = intr_disable(); //关闭中断
        cur->status = status;
        schedule(); // 在status不为TASK_RUNNING的情况下调用schedule(), 当前线程不会加入就绪队列中，以后中断无法调度到本线程，实现阻塞
        intr_set_status(old_intr_status);        
    }
}

/* 将线程解除阻塞 */
void thread_unblock(struct task_st *pthread) {
    enum task_status status = pthread->status;
    ASSERT(status == TASK_BLOCKED || status == TASK_WAITING || status == TASK_HANGING);
    if (status == TASK_BLOCKED || status == TASK_WAITING || status == TASK_HANGING) {
        enum intr_status old_intr_status = intr_disable(); // 关闭中断
        bool not_in_ready_list = ! list_exist(&threads_ready, &pthread->thread_ready_node);
        ASSERT(not_in_ready_list);
        if (not_in_ready_list) {
            pthread->status = TASK_READY;
            list_push_front(&threads_ready, &pthread->thread_ready_node); // 插入就绪队列头部，下次会调度到ta
        }
        intr_set_status(old_intr_status); // 恢复之前的中断状态
    }
}

/* 主动让出CPU, 当前线程加入就绪队列，切换至其他线程 */
void thread_yield() {
    struct task_st *cur = current_thread();
    enum intr_status old_status = intr_disable(); // 关闭中断
    cur->status = TASK_READY; // 运行态转为就绪态
    if (list_empty(&threads_ready)) {
        // 若没有就绪线程，则唤醒idle_thread
        // 这样schedule之后才能切换到idle_thread
        thread_unblock(idle_thread);
    }
    list_push_back(&threads_ready, &cur->thread_ready_node); // 加入就绪队列
    schedule(); // 调度，切换至其他线程，由于状态已切换至就绪态，schedule()函数中不会再次加入队列 \
                    也不会更新时间片
    intr_set_status(old_status);
}

/* 线程退出函数，由其他线程调用，一般由父进程调用以销毁子进程。
   删除线程栈以及页表
   虚拟地址位图以及分配的内存在进程的exit()函数中释放 */
void thread_exit(struct task_st *task_over, bool need_schedule) {
    enum intr_status old_status = intr_disable();
    ASSERT(task_over->status == TASK_DIED);

    if (list_exist(&threads_ready, &task_over->thread_ready_node))
        list_remove(&threads_ready, &task_over->thread_ready_node);

    ASSERT(list_exist(&threads_all, &task_over->thread_node));
    list_remove(&threads_all, &task_over->thread_node);

    struct task_st *cur = current_thread();
    uint32_t *page_table_backup = cur->page_table;
    if (task_over->page_table != NULL) {
        cur->page_table = NULL;
        pages_free(task_over->page_table, 1);
        cur->page_table = page_table_backup;
    }

    if (task_over != main_thread) {
        cur->page_table = NULL;
        pages_free(task_over, 1);
        cur->page_table = page_table_backup;
    }

    release_pid(task_over->pid);

    if (need_schedule)
        schedule();

    intr_set_status(old_status);
}

struct task_st *node_to_thread(list_node node) {
    return (struct task_st *)((uint32_t)node & 0xfffff000);
}

/* 根据pid查找线程栈 */

static struct check_pid_arg {
    pid_t pid;
    struct task_st *thread;
};

// list_traversal回调函数
static bool check_pid(list_node node, int arg) {
    struct check_pid_arg *pid_arg = (struct check_pid_arg *)arg;
    struct task_st *thread = node_to_thread(node);
    if (thread->pid == pid_arg->pid) {
        pid_arg->thread = thread;
        return true;
    }
    pid_arg->thread = NULL;
    return false;
}

struct task_st *pid2thread(pid_t pid) {
    struct check_pid_arg arg;
    arg.pid = pid;
    list_traversal(&threads_all, check_pid, (int)&arg);
    return arg.thread;
}

static void pad_print(uint32_t pad_len, void *ptr, char format) {
    uint32_t str_len;
    int16_t val16;
    int32_t val32;
    switch(format) {
        case 's':
            k_printf("%s", (char *)ptr);
            str_len = strlen((char *)ptr);
            break;
        case 'd':
            val16 = *(int16_t *)ptr;
            k_printf("%d", val16);
            if (val16 == 0) {
                str_len = 1;
                break;
            }
            str_len = 0;
            while (val16) {
                val16 /= 10;
                str_len++;
            }
            break;
        case 'x':
            val32 = *(int32_t *)ptr;
            k_printf("%x", val32);
            if (val32 == 0) {
                str_len = 1;
                break;
            }
            str_len = 0;
            while (val32) {
                val32 /= 16;
                str_len++;
            }
            break;
        default: return;
    }
    for ( ; str_len < pad_len; str_len++)
        console_put_char(' ');
}

/* list_traversal回调函数，用于输出线程的信息 */
static bool thread_info(list_node node, int arg) {
    struct task_st *thread = elem2entry(struct task_st, thread_node, node);

    uint32_t pad_len = 16;
    pad_print(pad_len, thread->pid, 'd');

    if (thread->parent_pid != -1)
        pad_print(pad_len, thread->parent_pid, 'd');
    else
        pad_print(pad_len, "NULL", 's');

    switch (thread->status) {
        case TASK_RUNNING:
            pad_print(pad_len, "RUNNING", 's');
            break;
        case TASK_READY:
            pad_print(pad_len, "READY", 's');
            break;
        case TASK_BLOCKED:
            pad_print(pad_len, "BLOCKED", 's');
            break;
        case TASK_WAITING:
            pad_print(pad_len, "WAITING", 's');
            break;
        case TASK_HANGING:
            pad_print(pad_len, "HANGING", 's');
            break;
        case TASK_DIED:
            pad_print(pad_len, "DIED", 's');
            break;
        default: break;
    }

    pad_print(pad_len, &thread->priority, 'x');
    pad_print(pad_len, &thread->ticks, 'x');

    pad_print(pad_len, thread->name, 's');

    console_put_char('\n');

    return false; // 迎合list_traversal函数
}

void sys_ps() {
    uint32_t pad_len = 16;
    pad_print(pad_len, "PID", 's');
    pad_print(pad_len, "PPID", 's');
    pad_print(pad_len, "STATUS", 's');
    pad_print(pad_len, "PRIORITY", 's');
    pad_print(pad_len, "TICKS", 's');
    pad_print(pad_len, "NAME", 's');
    console_put_char('\n');
    list_traversal(&threads_all, thread_info, 0);
}
