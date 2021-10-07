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
    SYS_FORK,
    SYS_GETCWD,
    SYS_OPEN,
    SYS_CLOSE,
    SYS_LSEEK,
    SYS_UNLINK,
    SYS_MKDIR,
    SYS_OPENDIR,
    SYS_CLOSEDIR,
    SYS_CHDIR,
    SYS_RMDIR,
    SYS_READDIR,
    SYS_REWINDDIR,
    SYS_STAT,
    SYS_PS
};

#define SYSCALL_NR 23

struct dir_st;
struct stat_st;

uint32_t getpid();
uint32_t write(int32_t fd, const void *buf, uint32_t count);
uint32_t read(int32_t fd, void *dest, uint32_t count);
void putchar(char ch);
void clear();
void *malloc(uint32_t size);
void free(void *ptr);
void sleep(uint32_t ms);
pid_t fork();
char *getcwd(char *buf, uint32_t size);
int32_t open(char*pathname, uint8_t flags);
int32_t close(int32_t fd);
int32_t lseek(int32_t fd, int32_t offset, uint8_t whence);
int32_t unlink(const char *pathname);
int32_t mkdir(const char *pathname);
struct dir_st *opendir(const char *name);
int32_t closedir(struct dir_st *dir);
int32_t rmdir(const char *pathname);
int32_t chdir(const char *path);
struct dir_entry_st *readdir(struct dir_st *dir);
void rewinddir(struct dir_st *dir);
int32_t stat(const char *path, struct stat_st *stat);
void ps();

#endif
