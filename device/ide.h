#ifndef DEVICE_IDE_H__
#define DEVICE_IDE_H__
#include "stdint.h"
#include "stdbool.h"
#include "sync.h"
#include "bitmap.h"

// 分区结构体
struct partition_st {
    uint32_t start_lba;             // 起始扇区
    uint32_t sec_num;               // 扇区数
    struct disk_st *my_disk;           // 本分区所属的硬盘
    struct list_node_st part_tag;   // 加入链表的标记
    char name[8];
    struct super_block *sb;         // 本分区的超级块
    struct bitmap block_btmp;       // 块位图
    struct bitmap inode_btmp;       // i结点位图
    struct list_st open_inodes;     // 本分区打开的i结点链表
};

// 硬盘结构体
struct disk_st {
    char name[8];
    struct ide_channel_st *my_channel; // 本硬盘所属的通道
    uint8_t dev_no;
    struct partition_st prim_parts[4]; // 主分区，最多4个
    struct partition_st logic_parts[8];// 逻辑分区，逻辑上数量无限，这里上限支持8个
};

// ata通道结构体
struct ide_channel_st {
    char name[8];
    uint16_t port_base;             // 本通道的起始端口号
    uint8_t irq_no;                 // 使用的中断号
    struct lock_st lock;            // 通道锁
    bool expecting_intr;            // 是否在等待来自硬盘的中断
    struct semaphore_st disk_done;  // 用于阻塞
    struct disk_st devices[2];      // 1个通道连接2个硬盘，一从一主
};

extern uint8_t channel_num;
extern struct ide_channel_st channels[];

void ide_init();
#endif
