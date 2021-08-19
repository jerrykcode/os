#ifndef FS_INODE_H__
#define FS_INODE_H__

#include "stdint.h"
#include "stddef.h"
#include "list.h"

/* inode结构 */
struct inode_st {
    uint32_t i_id;  // inode号
    uint32_t i_size; // 若inode是文件，i_size表示文件大小，若inode是目录，i_size包括所有子目录大小之和
    uint32_t i_open_cnts; // 此文件被打开的次数
    bool write_deny; // 写文件不能并行,进程写文件前检查此标识
    uint32_t i_sectors[13]; // i_sectors[0-11]是直接块，i_sectors[12]存储一级间接块指针
    struct list_node_st inode_tag; // 用于存入链表
};

#endif
