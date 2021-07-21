#ifndef LIB_USR_SYSCALL_INIT_H__
#define LIB_USR_SYSCALL_INIT_H__
#include "stdint.h"

void syscall_init();

/*
系统调用函数
*/
uint32_t sys_getpid();
uint32_t sys_write(char *str);

#endif
