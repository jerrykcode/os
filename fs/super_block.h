#ifndef FS_SUPER_BLOCK_H__
#define FS_SUPER_BLOCK_H__

#include "stdint.h"

/* 超级块 */
struct super_block_st {
    uint32_t magic;
    uint32_t sec_num;
    uint32_t inode_num;
    uint32_t part_lba_base;

    uint32_t block_btmp_lba; // 块位图起始扇区地址
    uint32_t block_btmp_sec_num; // 块位图大小(占用的扇区数量)

    uint32_t inode_btmp_lba;
    uint32_t inode_btmp_sec_num;

    uint32_t inode_table_lba; // inode表 起始扇区地址
    uint32_t inode_table_sec_num; // inode表 占用的扇区数量

    uint32_t data_start_lba; // 数据区起始扇区地址
    uint32_t root_inode_id; // 根目录的inode号
    uint32_t dir_entry_size; // 目录项大小

    uint8_t pad[460]; // 加上460字节，凑齐512字节一扇区大小
} __attribute__((packed));

#endif
