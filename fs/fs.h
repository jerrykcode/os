#ifndef FS_FS_H__
#define FS_FS_H__

#include "stdint.h"

#define MAX_FILES_PER_PART  4096
#define BITS_PER_SECTOR     4096
#define SECTOR_SIZE         512
#define BLOCK_SIZE          SECTOR_SIZE // 这里1块即1扇区
#define MAX_PATH_LEN        512

/* 文件类型 */
enum file_types {
    FT_UNKNOWN,  // 不支持的文件类型
    FT_REGULAR,  // 普通文件
    FT_DIRECTORY // 目录文件
};

/* 打开文件的选项 */
enum oflags {
    O_RONLY     /* 000 */,
    O_WONLY     /* 001 */,
    O_RW        /* 010 */,
    O_CREATE = 4/* 100 */
};

/* 文件读写位置偏移量 */
enum whence {
    SEEK_SET = 1,
    SEEK_CUR,
    SEEK_END
};

/* 文件属性结构体 */
struct stat_st {
    int32_t inode_id;
    uint32_t file_size;
    enum file_types file_type;
};

struct dir_entry_st;
struct dir_st;

extern struct partition_st *cur_part;

int32_t fd_local2global(int32_t local_fd);
int32_t path_depth(const char *pathname);
int32_t sys_open(const char *pathname, uint8_t flags);
int32_t sys_close(int32_t fd);
int32_t sys_stat(const char *path, struct stat_st *stat);
int32_t sys_write(int32_t fd, const void *buf, uint32_t count);
int32_t sys_read(int32_t fd, void *dest, uint32_t count);
void sys_putchar(char ch);
int32_t sys_lseek(int32_t fd, int32_t offset, uint8_t whence);
int32_t sys_unlink(const char *pathname);
int32_t sys_mkdir(const char *pathname);
int32_t sys_rmdir(const char *pathname);
struct dir_entry_st *sys_readdir(struct dir_st *dir);
void sys_rewinddir(struct dir_st *dir);
char *sys_getcwd(char *buf, uint32_t size);
int32_t sys_chdir(const char *path);
struct dir_st *sys_opendir(const char *pathname);
void sys_closedir(struct dir_st *dir);
void filesys_init();

#endif
