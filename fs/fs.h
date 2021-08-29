#ifndef FS_FS_H__
#define FS_FS_H__

#include "stdint.h"

#define MAX_FILES_PER_PART  4096
#define BITS_PER_SECTOR     4096
#define SECTOR_SIZE         512
#define BLOCK_SIZE          SECTOR_SIZE // 这里1块即1扇区

/* 文件类型 */
enum file_types {
    FT_UNKNOWN,  // 不支持的文件类型
    FT_REGULAR,  // 普通文件
    FT_DIRECTORY // 目录文件
};

extern struct partition_st *cur_part;

void filesys_init();

#endif
