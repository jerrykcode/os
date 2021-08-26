#include "file.h"
#include "fs.h"
#include "stddef.h"
#include "stdio-kernel.h"
#include "super_block.h"
#include "thread.h"

struct file file_table[MAX_FILE_OPEN]; // 文件表

/* 从文件表中获取一个空闲位，成功返回其下标，失败返回-1 */
int32_t get_free_slot_in_global() {
    for (int i = 3; i < MAX_FILE_OPEN; i++)
        if (file_table[i].fd_inode == NULL) {
            return i;
        }
    k_printf("exceed max open files\n");
    return -1;
}

/* 从当前PCB的文件描述符数组中获取一个空闲下标，并将全局描述符下标存入该下标 
 * 成功返回该下标，失败 返回-1 */
int32_t pcb_fd_install(int32_t global_fd_idx) {
    struct task_st *cur = current_thread();
    for (int i = 0; i < MAX_FILES_OPEN_PER_PROC; i++)
        if (cur->fd_table[i] == -1) {
            cur->fd_table[i] = global_fd_idx;
            return i;
        }
    k_printf("exceed max open files in this process!\n");
    return -1;
}

/* 分配一个inode, 返回inode号 */
int32_t inode_bitmap_alloc(struct partition_st *part) {
    int bit_idx = bitmap_alloc(&part->inode_btmp, 1, INODE_BTMP_BIT_FREE);
    if (bit_idx != -1) {
        bitmap_setbit(&part->inode_btmp, bit_idx, INODE_BTMP_BIT_USED);
    }
    return bit_idx;
}

/* 分配一个空闲块(1个块即1扇区)，返回扇区lba */
int32_t block_bitmap_alloc(struct partition_st *part) {
    int bit_idx = bitmap_alloc(&part->block_btmp, 1, BLOCK_BTMP_BIT_FREE);
    if (bit_idx == -1)
        return -1;

    bitmap_setbit(&part->block_btmp, bit_idx, BLOCK_BTMP_BIT_USED);
    return part->sb->data_start_lba + bit_idx;
}

/* 将内存中bitmap第bit_idx位所在的扇区同步到硬盘 */
void bitmap_sync(struct partition_st *part, uint32_t bit_idx, uint8_t btmp_type) {
    uint32_t off_sec = bit_idx / BITS_PER_SECTOR; // bit_idx偏移的扇区数量
    uint32_t off_size = off_sec * SECTOR_SIZE; // bit_idx偏移的扇区数量对应的字节数量
    uint32_t sec_lba;
    uint8_t *btmp_off;
    switch (btmp_type) {
        case INODE_BTMP:
            sec_lba = part->sb->inode_btmp_lba + off_sec;
            btmp_off = part->inode_btmp.bits + off_size;
            break;
        case BLOCK_BTMP:
            sec_lba = part->sb->block_btmp_lba + off_sec;
            btmp_off = part->block_btmp.bits + off_size;
            break;
        default:
            break;
    }
    ide_write(part->my_disk, sec_lba, 1, btmp_off);
}
