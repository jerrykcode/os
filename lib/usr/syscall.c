#include "syscall.h"
#include "asm.h"

/*
系统调用通过0x80中断进入0特权级栈
系统调用序号用eax存储
参数用ebx, ecx, edx存储，目前最多3个参数
返回值存储在eax中
*/

/*
无参数的系统调用
*/
#define _syscall0(NUMBER) ({    \
    int retval;                 \
    asm volatile (              \
    "int $0x80"                 \
    : "=a" (retval)             \
    : "a" (NUMBER)              \
    : "memory"                  \
    );                          \
    retval;                     \
})

/*
1个参数的系统调用
*/
#define _syscall1(NUMBER, ARG0) ({      \
    int retval;                         \
    asm volatile (                      \
    "int $0x80"                         \
    : "=a" (retval)                     \
    : "a" (NUMBER), "b" (ARG0)          \
    : "memory"                          \
    );                                  \
    retval;                             \
})

/*
2参数系统调用
*/
#define _syscall2(NUMBER, ARG0, ARG1) ({    \
    int retval;                             \
    asm volatile (                          \
    "int $0x80"                             \
    : "=a" (retval)                         \
    : "a" (NUMBER), "b" (ARG0), "c" (ARG1)  \
    : "memory"                              \
    );                                      \
    retval;                                 \
})

/*
3参数系统调用
*/
#define _syscall3(NUMBER, ARG0, ARG1, ARG2) ({  \
    int retval;                                 \
    asm volatile (                              \
    "int $0x80"                                 \
    : "=a" (retval)                             \
    : "a" (NUMBER), "b" (ARG0), "c" (ARG1), "d" (ARG2)  \
    : "memory"                                  \
    );                                          \
    retval;                                     \
})

/* 返回当前任务的pid */
uint32_t getpid() {
    return _syscall0(SYS_GETPID);
}

/* 向文件描述符fd对应的文件写入buf处的count字节数据 */
uint32_t write(int32_t fd, const void *buf, uint32_t count) {
    return _syscall3(SYS_WRITE, fd, buf, count);
}

/* 从文件描述符fd对应的文件读取count字节数据存入dest */
uint32_t read(int32_t fd, void *dest, uint32_t count) {
    return _syscall3(SYS_READ, fd, dest, count);
}

/* 申请size字节内存 */
void *malloc(uint32_t size) {
    return _syscall1(SYS_MALLOC, size);
}

/* 释放内存 */
void free(void *ptr) {
    _syscall1(SYS_FREE, ptr);
}

/* 暂停执行ms毫秒 */
void sleep(uint32_t ms) {
    _syscall1(SYS_SLEEP, ms);
}

/* fork系统调用 */
pid_t fork() {
    _syscall0(SYS_FORK);
}
