#include "sync.h"
#include "list.h"
#include "interrupt.h"
#include "thread.h"
#include "debug.h"
#include "stdint.h"
#include "stddef.h"

void semaphore_init(struct semaphore_st *p_semaphore, uint8_t value) {
    p_semaphore->value = value;
    list_init(&p_semaphore->waiting_list);
}

void semaphore_down(struct semaphore_st *p_semaphore) {
    enum intr_status old_status = intr_disable();

    struct task_st *cur = current_thread();
    while (p_semaphore->value == 0) { // 被其他线程持有
        list_push_back(&p_semaphore->waiting_list, &cur->thread_node); // 加入等待队列
        thread_block(TASK_BLOCKED); // 阻塞当前线程
    }
    // 执行到这里 已获取锁
    p_semaphore->value--;

    intr_set_status(old_status);
}

void semaphore_up(struct semaphore_st *p_semaphore) {
    enum intr_status old_status = intr_disable();

    p_semaphore->value++;

    if (!list_empty(&p_semaphore->waiting_list))
        thread_unblock(node_to_thread(list_pop(&p_semaphore->waiting_list))); // 唤醒一个阻塞的线程

    intr_set_status(old_status);
}

void lock_init(struct lock_st *plock) {
    semaphore_init(&plock->semaphore, 1);
    plock->p_lock_holder = NULL;
    plock->holder_repeat_cnt = 0;
}

/* 获取锁 */
void lock_acquire(struct lock_st *plock) {
    if (plock->p_lock_holder == current_thread()) {
        // 当前线程已经获取该锁
        plock->holder_repeat_cnt++;
        return;
    }

    semaphore_down(&plock->semaphore);

    plock->p_lock_holder = current_thread();
    plock->holder_repeat_cnt = 1;
}

/* 释放锁 */
void lock_release(struct lock_st *plock) {
    ASSERT(plock->p_lock_holder == current_thread());

    plock->holder_repeat_cnt--;
    if (plock->holder_repeat_cnt > 0)
        return;

    plock->p_lock_holder = NULL;
    semaphore_up(&plock->semaphore);
}
