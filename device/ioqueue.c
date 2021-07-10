#include "ioqueue.h"
#include "stdint.h"
#include "stddef.h"
#include "stdbool.h"
#include "sync.h"
#include "thread.h"
#include "interrupt.h"
#include "debug.h"

void ioqueue_init(struct ioqueue_st *ioqueue) {
    ioqueue->head = 0;
    ioqueue->tail = 0;
    lock_init(&ioqueue->producer_lock);
    lock_init(&ioqueue->consumer_lock);
    ioqueue->producer = NULL;
    ioqueue->consumer = NULL;
}

static uint8_t next_pos(uint8_t pos) {
    return (pos + 1) % IOQUEUE_SIZE;
}

static bool ioqueue_empty(struct ioqueue_st *ioqueue) {
    return ioqueue->tail == ioqueue->head;
}

static bool ioqueue_full(struct ioqueue_st *ioqueue) {
    return next_pos(ioqueue->head) == ioqueue->tail;
}

void ioqueue_putchar(struct ioqueue_st *ioqueue, char c) {
    // 获取生产者锁，以下代码段在任意时刻最多只有一个生产者线程在执行
    lock_acquire(&ioqueue->producer_lock);

    enum intr_status old_status = intr_disable(); //关中断
    while (ioqueue_full(ioqueue)) {
        // 判断queue满 与阻塞当前线程 组成原子操作
        // 不存在阻塞的时候已经有消费者消费导致queue有空位的情况        
        ioqueue->producer = current_thread();
        thread_block(TASK_BLOCKED);
    }
    intr_set_status(old_status); // 恢复中断

    // 生产者生产
    ioqueue->head = next_pos(ioqueue->head);
    ioqueue->queue[ioqueue->head] = c;

    // 若有消费者线程阻塞等待queue中的数据，则唤醒该消费者
    if (ioqueue->consumer != NULL) {
        struct task_st *blocked_consumer = ioqueue->consumer;
        ioqueue->consumer = NULL;
        ASSERT(blocked_consumer->status == TASK_BLOCKED);
        thread_unblock(blocked_consumer);
    }

    // 解锁
    lock_release(&ioqueue->producer_lock);
}

// 消费者，与生产者类似
char ioqueue_getchar(struct ioqueue_st *ioqueue) {

    lock_acquire(&ioqueue->consumer_lock);

    enum intr_status old_status = intr_disable();
    while (ioqueue_empty(ioqueue)) {
        ioqueue->consumer = current_thread();
        thread_block(TASK_BLOCKED);
    }
    intr_set_status(old_status);

    ioqueue->tail = next_pos(ioqueue->tail);
    char result = ioqueue->queue[ioqueue->tail];

    if (ioqueue->producer != NULL) {
        struct task_st *blocked_producer = ioqueue->producer;
        ioqueue->producer = NULL;
        ASSERT(blocked_producer->status == TASK_BLOCKED);
        thread_unblock(blocked_producer);
    }

    lock_release(&ioqueue->consumer_lock);

    return result;
}
