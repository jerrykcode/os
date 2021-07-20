#ifndef LIB_USR_SYSCALL_H__
#define LIB_USR_SYSCALL_H__
#include "stdint.h"

enum SYSCALL_NR {
    SYS_GETPID
};

#define SYSCALL_NR 1

uint32_t getpid();

#endif
