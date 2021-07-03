#ifndef SYNC_H__
#define SYNC_H__

#include "stdint.h"
#include "list.h"

/* 信号量 */
struct semaphore_st {
    uint8_t value;
    struct list_st waiting_list;
};

/* 锁 */
struct lock_st {
    struct semaphore_st semaphore; // 信号量
    struct task_st *p_lock_holder; // 锁的持有者
    uint32_t holder_repeat_cnt; // 持有者持有锁的次数
};

void semaphore_init(struct semaphore_st *p_semaphore, uint8_t value);
void semaphore_down(struct semaphore_st *p_semaphore);
void semaphore_up(struct semaphore_st *p_semaphore);

void lock_init(struct lock_st *plock);
void lock_acquire(struct lock_st *plock);
void lock_release(struct lock_st *plock);

#endif
