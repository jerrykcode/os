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

/* 输出一个字符到控制台stdout */
void putchar(char ch) {
    _syscall1(SYS_PUTCHAR, ch);
}

/* 清空屏幕 */
void clear() {
   _syscall0(SYS_CLEAR);
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

int32_t execv(const char *path, const char *argv[]) {
    return _syscall2(SYS_EXECV, path, argv);
}

pid_t wait(int32_t *status) {
    return _syscall1(SYS_WAIT, status);
}

void exit(int32_t status) {
    _syscall1(SYS_EXIT, status);
}

char *getcwd(char *buf, uint32_t size) {
    return _syscall2(SYS_GETCWD, buf, size);
}

int32_t open(char*pathname, uint8_t flags) {
    return _syscall2(SYS_OPEN, pathname, flags);
}

int32_t close(int32_t fd) {
    return _syscall1(SYS_CLOSE, fd);
}

int32_t lseek(int32_t fd, int32_t offset, uint8_t whence) {
    return _syscall3(SYS_LSEEK, fd, offset, whence);
}

int32_t unlink(const char *pathname) {
    return _syscall1(SYS_UNLINK, pathname);
}

int32_t mkdir(const char *pathname) {
    return _syscall1(SYS_MKDIR, pathname);
}

struct dir_st *opendir(const char *name) {
    return _syscall1(SYS_OPENDIR, name);
}

int32_t closedir(struct dir_st *dir) {
    return _syscall1(SYS_CLOSEDIR, dir);
}

int32_t rmdir(const char *pathname) {
    return _syscall1(SYS_RMDIR, pathname);
}

int32_t chdir(const char *path) {
    return _syscall1(SYS_CHDIR, path);
}

struct dir_entry_st *readdir(struct dir_st *dir) {
    return _syscall1(SYS_READDIR, dir);
}

void rewinddir(struct dir_st *dir) {
    _syscall1(SYS_REWINDDIR, dir);
}

int32_t stat(const char *path, struct stat_st *stat) {
    return _syscall2(SYS_STAT, path, stat);
}

void ps() {
    _syscall0(SYS_PS);
}

void debug_vaddr_start() {
    _syscall0(DEBUG_VADDR_START);
}
