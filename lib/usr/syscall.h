#ifndef LIB_USR_SYSCALL_H__
#define LIB_USR_SYSCALL_H__
#include "stdint.h"

enum SYSCALL_NR {
    SYS_GETPID,
    SYS_WRITE,
    SYS_MALLOC,
    SYS_FREE,
    SYS_SLEEP
};

#define SYSCALL_NR 2

uint32_t getpid();
uint32_t write(int32_t fd, const void *buf, uint32_t count);
void *malloc(uint32_t size);
void free(void *ptr);
void sleep(uint32_t ms);

#endif
