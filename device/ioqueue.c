#include "ioqueue.h"
#include "stdint.h"
#include "stddef.h"
#include "stdbool.h"
#include "sync.h"
#include "thread.h"
#include "list.h"
#include "interrupt.h"
#include "debug.h"

void ioqueue_init(struct ioqueue_st *ioqueue) {
    ioqueue->head = 0;
    ioqueue->tail = 0;
    lock_init(&ioqueue->lock);
    list_init(&ioqueue->waiting_producer);
    list_init(&ioqueue->waiting_consumer);
}

static uint8_t next_pos(uint8_t pos) {
    return (pos + 1) % IOQUEUE_SIZE;
}

static bool ioqueue_empty(struct ioqueue_st *ioqueue) {
    return ioqueue->head == ioqueue->tail;
}

static bool ioqueue_full(struct ioqueue_st *ioqueue) {
    return next_pos(ioqueue->tail) == ioqueue->head;
}

// 生产者
void ioqueue_putchar(struct ioqueue_st *ioqueue, char c) {
    // 获取锁
    lock_acquire(&ioqueue->lock);

    // 判断是否有空位
    while (ioqueue_full(ioqueue)) {
        // 释放锁并阻塞
        
        list_push_back(&ioqueue->waiting_producer, &(current_thread()->thread_node)); // 记录在链表上

        // 释放锁和阻塞需要原子操作
        enum intr_status old_status = intr_disable();
        lock_release(&ioqueue->lock);
        thread_block(TASK_BLOCKED);
        intr_set_status(old_status);

        // 重新获取锁
        lock_acquire(&ioqueue->lock);
    }

    // 到这里已经有空位了, 生产者开始生产

    ioqueue->tail = next_pos(ioqueue->tail);
    ioqueue->queue[ioqueue->tail] = c;

    if (! list_empty(&ioqueue->waiting_consumer)) // 有阻塞等待数据的消费者 则唤醒
        thread_unblock(node_to_thread(list_pop(&ioqueue->waiting_consumer)));

    // 解锁
    lock_release(&ioqueue->lock);
}

// 消费者，与生产者类似
char ioqueue_getchar(struct ioqueue_st *ioqueue) {
    lock_acquire(&ioqueue->lock);

    while (ioqueue_empty(ioqueue)) {
        list_push_back(&ioqueue->waiting_consumer, &(current_thread()->thread_node));

        enum intr_status old_status = intr_disable();
        lock_release(&ioqueue->lock);
        thread_block(TASK_BLOCKED);
        intr_set_status(old_status);

        lock_acquire(&ioqueue->lock);
    }

    // 消费

    ioqueue->head = next_pos(ioqueue->head);
    char c = ioqueue->queue[ioqueue->head];

    if (! list_empty(&ioqueue->waiting_producer))
        thread_unblock(node_to_thread(list_pop(&ioqueue->waiting_producer)));

    lock_release(&ioqueue->lock);

    return c;
}
