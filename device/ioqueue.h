#ifndef IOQUEUE_H__
#define IOQUEUE_H__

#include "stdint.h"
#include "thread.h"
#include "sync.h"

#define IOQUEUE_SIZE    32

struct ioqueue_st {
    char queue[IOQUEUE_SIZE];
    uint8_t head, tail;
    struct lock_st producer_lock, consumer_lock;
    struct task_st *producer, *consumer;
};

void ioqueue_init(struct ioqueue_st *ioqueue);
void ioqueue_putchar(struct ioqueue_st *ioqueue, char c);
char ioqueue_getchar(struct ioqueue_st *ioqueue);

#endif
