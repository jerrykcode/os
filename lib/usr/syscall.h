#ifndef LIB_USR_SYSCALL_H__
#define LIB_USR_SYSCALL_H__
#include "stdint.h"

enum SYSCALL_NR {
    SYS_GETPID,
    SYS_WRITE
};

#define SYSCALL_NR 2

uint32_t getpid();
uint32_t write(char *str);

#endif
