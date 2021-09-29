#ifndef LIB_USR_SYSCALL_H__
#define LIB_USR_SYSCALL_H__
#include "stdint.h"
#include "thread.h"

enum SYSCALL_NR {
    SYS_GETPID,
    SYS_WRITE,
    SYS_READ,
    SYS_PUTCHAR,
    SYS_CLEAR,
    SYS_MALLOC,
    SYS_FREE,
    SYS_SLEEP,
    SYS_FORK
};

#define SYSCALL_NR 9

uint32_t getpid();
uint32_t write(int32_t fd, const void *buf, uint32_t count);
uint32_t read(int32_t fd, void *dest, uint32_t count);
void putchar(char ch);
void clear();
void *malloc(uint32_t size);
void free(void *ptr);
void sleep(uint32_t ms);
pid_t fork();

#endif
