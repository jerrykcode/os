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

extern struct partition_st *cur_part;

int32_t path_depth(const char *pathname);
int32_t sys_open(const char *pathname, uint8_t flags);
int32_t sys_close(int32_t fd);
int32_t sys_write(int32_t fd, const void *buf, uint32_t count);
int32_t sys_read(int32_t fd, void *dest, uint32_t count);
int32_t sys_lseek(int32_t fd, int32_t offset, uint8_t whence);
int32_t sys_unlink(const char *pathname);
int32_t sys_mkdir(const char *pathname);
void filesys_init();

#endif
