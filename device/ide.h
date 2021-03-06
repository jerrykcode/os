#ifndef DEVICE_IDE_H__
#define DEVICE_IDE_H__
#include "stdint.h"
#include "stdbool.h"
#include "sync.h"
#include "bitmap.h"
#include "list.h"

#define INODE_BTMP_BIT_FREE 0
#define INODE_BTMP_BIT_USED 1
#define BLOCK_BTMP_BIT_FREE 0
#define BLOCK_BTMP_BIT_USED 1

// struct ide_bitmap 与 lib/kernel/bitmap.h 中的 struct bitmap 一样
// 如果不定义 struct ide_bitmap 而使用 struct bitmap 编译通不过!!
// kernel/memory.h 中也遇到同样的问题!!!??? :(
struct ide_bitmap {
    uint32_t btmp_bytes_len;
    uint8_t *bits;
};

// 分区结构体
struct partition_st {
    uint32_t start_lba;             // 起始扇区
    uint32_t sec_num;               // 扇区数
    struct disk_st *my_disk;           // 本分区所属的硬盘
    struct list_node_st part_tag;   // 加入链表的标记
    char name[8];
    struct super_block_st *sb;         // 本分区的超级块
    struct ide_bitmap block_btmp;       // 块位图
    struct ide_bitmap inode_btmp;       // i结点位图
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

extern struct list_st partition_list; // 分区链表

void ide_init();
void ide_read(struct disk_st *hd, uint32_t lba, uint32_t sec_num, void *dest);
void ide_write(struct disk_st *hd, uint32_t lba, uint32_t sec_num, void *src);
void intr_hd_handler(uint8_t irq_no);
#endif
