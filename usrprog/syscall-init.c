#include "syscall-init.h"
#include "syscall.h"
#include "stdint.h"
#include "print.h"
#include "thread.h"
#include "console.h"
#include "string.h"
#include "memory.h"
#include "timer.h"
#include "fs.h"
#include "fork.h"
#include "exec.h"
#include "wait_exit.h"
#include "debug.h"

typedef void *syscall;

syscall syscall_table[SYSCALL_NR];

/* 初始化系统调用 */
void syscall_init() {
    put_str("syscall_init start\n");
    syscall_table[SYS_GETPID]     = sys_getpid;
    syscall_table[SYS_WRITE]      = sys_write;
    syscall_table[SYS_READ]       = sys_read;
    syscall_table[SYS_PUTCHAR]    = sys_putchar;
    syscall_table[SYS_CLEAR]      = cls_screen;
    syscall_table[SYS_MALLOC]     = sys_malloc;
    syscall_table[SYS_FREE]       = sys_free;
    syscall_table[SYS_SLEEP]      = sys_sleep;
    syscall_table[SYS_FORK]       = sys_fork;
    syscall_table[SYS_EXECV]      = sys_execv;
    syscall_table[SYS_WAIT]       = sys_wait;
    syscall_table[SYS_EXIT]       = sys_exit;
    syscall_table[SYS_GETCWD]     = sys_getcwd;
    syscall_table[SYS_OPEN]       = sys_open;
    syscall_table[SYS_CLOSE]      = sys_close;
    syscall_table[SYS_LSEEK]      = sys_lseek;
    syscall_table[SYS_UNLINK]     = sys_unlink;
    syscall_table[SYS_MKDIR]      = sys_mkdir;
    syscall_table[SYS_OPENDIR]    = sys_opendir;
    syscall_table[SYS_CLOSEDIR]   = sys_closedir;
    syscall_table[SYS_CHDIR]      = sys_chdir;
    syscall_table[SYS_RMDIR]      = sys_rmdir;
    syscall_table[SYS_READDIR]    = sys_readdir;
    syscall_table[SYS_REWINDDIR]  = sys_rewinddir;
    syscall_table[SYS_STAT]       = sys_stat;
    syscall_table[SYS_PS]         = sys_ps;

    /* 供调试使用的系统调用 */
    syscall_table[DEBUG_VADDR_START] = debug_sys_vaddr_start;

    put_str("syscall_init finished");
}

/*
系统调用函数
*/
uint32_t sys_getpid() {
    return current_thread()->pid;
}


void debug_sys_vaddr_start() {
    int byte = current_thread()->usrprog_vaddr.vaddr_btmp.bits[0];
    int bit = (byte >> 7) & 1;
    void *vaddr = (void *)(current_thread()->usrprog_vaddr.vaddr_start);
    uint32_t *pte = pte_ptr(vaddr);
    uint32_t *pde = pde_ptr(vaddr);
    if (bit) {
        ASSERT(*pde & 0x00000001);
        ASSERT(*pte & 0x00000001);
    }
}
