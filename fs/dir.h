#ifndef FS_DIR_H__
#define FS_DIR_H__

#include "stdint.h"
#include "inode.h"
#include "fs.h"

#define MAX_FILE_NAME_LEN   32

/* 目录结构 */
struct dir_st {
    struct inode_st *inode;
    uint32_t dir_pos;  // 记录在目录内的偏移
    uint8_t dir_buf[512]; // 目录的数据缓存
};

/* 目录项结构 */
struct dir_entry_st {
    // 每个目录项描述目录中的一个文件或子目录
    char filename[MAX_FILE_NAME_LEN]; // 文件或目录名
    uint32_t inode_id; // 文件或目录对应的inode号
    enum file_types f_type; // 文件类型
};

#endif
