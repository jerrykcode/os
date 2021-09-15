#ifndef FS_DIR_H__
#define FS_DIR_H__

#include "stdint.h"
#include "inode.h"
#include "fs.h"
#include "ide.h"
#include "stddef.h"

#define MAX_FILE_NAME_LEN   32

// 用于遍历目录项的迭代器
struct dir_entry_iterator {
    struct dir_st *dir;     // 遍历的目录
    uint32_t cur_block_idx; // 当前遍历到第几个块了
    uint32_t cur_entry_idx; // 当前块中遍历到第几个项了
    void *block_buf;        // 在内存中缓存当前块
    uint32_t *blocks;       // 在内存中缓存所有的间接块
    uint32_t entry_count;   // 已经遍历了几个目录项了
    bool rewind;            // 是否刚刚执行了rewind
};

/* 目录结构 */
struct dir_st {
    struct inode_st *inode;
    uint32_t dir_pos;  // 记录在目录内的偏移
    struct dir_entry_iterator dir_it; // 用于遍历目录项的迭代器
    uint8_t dir_buf[512]; // 目录的数据缓存
};

/* 目录项结构 */
struct dir_entry_st {
    // 每个目录项描述目录中的一个文件或子目录
    char filename[MAX_FILE_NAME_LEN]; // 文件或目录名
    uint32_t inode_id; // 文件或目录对应的inode号
    enum file_types f_type; // 文件类型
};

extern struct dir_st root_dir;

void open_root_dir(struct partition_st *part);
struct dir_st *dir_open(struct partition_st *part, uint32_t inode_id);
void dir_close(struct dir_st *dir);
bool dir_search(struct partition_st *part, struct dir_st *dir, const char *entry_name, struct dir_entry_st *entry);
void dir_init_entry(struct dir_entry_st *entry, const char *filename, uint32_t inode_id, uint8_t f_type);
bool dir_sync_entry(struct dir_st *dir, struct dir_entry_st *entry, void *io_buf);
bool dir_delete_entry(struct partition_st *part, struct dir_st *dir, uint32_t dl_inode_id, void *io_buf);
struct dir_entry_st *dir_read(struct dir_st *dir);

#endif
