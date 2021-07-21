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

/* 打印字符串 */
uint32_t write(char *str) {
    return _syscall1(SYS_WRITE, str);
}
