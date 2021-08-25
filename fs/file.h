#ifndef FS_FILE_H__
#define FS_FILE_H__

#include "stdint.h"
#include "ide.h"

/* 文件结构 */
struct file {
    uint32_t fd_pos; // 当前文件操作的偏移地址，0~文件大小-1
    uint32_t fd_flag;
    struct inode_st *fd_inode;
};

/* 标准文件输入输出描述符 */
enum std_fd {
    stdin_fd,   // 0 标准输入
    stdout_fd,  // 1 标准输出
    stderr_fd   // 2 标准错误
};

enum bitmap_type {
    INODE_BTMP,
    BLOCK_BTMP
};

#define MAX_FILE_OPEN   32 // 系统可打开的最大文件数

extern struct file file_table[MAX_FILE_OPEN];

int32_t get_free_slot_in_global();
int32_t pcb_fd_install(int32_t global_fd_idx);
int32_t inode_bitmap_alloc(struct partition_st *part);
int32_t block_bitmap_alloc(struct partition_st *part);
void bitmap_sync(struct partition_st *part, uint32_t bit_idx, uint8_t btmp_type);
#endif
