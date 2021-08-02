#include "syscall-init.h"
#include "syscall.h"
#include "stdint.h"
#include "print.h"
#include "thread.h"
#include "console.h"
#include "string.h"
#include "memory.h"

typedef void *syscall;

syscall syscall_table[SYSCALL_NR];

/* 初始化系统调用 */
void syscall_init() {
    put_str("syscall_init start\n");
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_WRITE] = sys_write;
    syscall_table[SYS_MALLOC] = sys_malloc;
    syscall_table[SYS_FREE] = sys_free;
    put_str("syscall_init finished");
}

/*
系统调用函数
*/
uint32_t sys_getpid() {
    return current_thread()->pid;
}

uint32_t sys_write(char *str) {
    console_put_str(str);
    return strlen(str);
}
