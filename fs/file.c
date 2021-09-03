#include "file.h"
#include "fs.h"
#include "inode.h"
#include "stddef.h"
#include "stdio-kernel.h"
#include "super_block.h"
#include "thread.h"
#include "interrupt.h"
#include "list.h"

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

/* 在parent_dir目录下创建普通文件(非目录)，成功返回描述符，失败返回-1 */
int32_t file_create(struct dir_st *parent_dir, char *filename, uint8_t flag) {
    void *io_buf = sys_malloc(1024);
    if (io_buf == NULL) {
        k_printf("ERROR file_create: alloc memory failed!!!\n");
        return -1;
    }

    uint8_t roll_back_step = 0; // 用于失败时回滚各资源状态

    // 为新文件分配inode号
    int32_t inode_id = inode_bitmap_alloc(cur_part);
    if (inode_id == -1) {
        k_printf("ERROR file_create: alloc inode failed!!!\n");
        sys_free(io_buf);
        return -1;
    }

    // 在内存中创建inode并初始化
    struct inode_st *new_file_inode = (struct inode_st *)sys_malloc(sizeof(struct inode_st));
    if (new_file_inode == NULL) {
        k_printf("ERROR file_create: alloc memory failed!!!\n");
        roll_back_step = 1;
        goto ROLL_BACK;
    }

    inode_init(inode_id, new_file_inode);

    // 从文件表中获取一个空闲位
    int32_t fd = get_free_slot_in_global();
    if (fd == -1) {
        roll_back_step = 2;
        goto ROLL_BACK;
    }

    file_table[fd].fd_pos = 0;
    file_table[fd].fd_flag = flag;
    file_table[fd].fd_inode = new_file_inode;

    // 创建目录项
    struct dir_entry_st entry;
    memset(&entry, 0, sizeof(struct dir_entry_st));
    dir_init_entry(&entry, filename, inode_id, FT_REGULAR);

    /* 内存数据同步到硬盘 */

    // 目录项写入parent_dir目录
    if (!dir_sync_entry(parent_dir, &entry, io_buf)) {
        k_printf("ERROR file_create: dir_sync_entry failed!!!\n");
        roll_back_step = 3;
        goto ROLL_BACK;
    }

    // inode写入硬盘
    memset(io_buf, 0, 1024);
    inode_sync(cur_part, new_file_inode, io_buf);

    // inode位图同步到硬盘
    bitmap_sync(cur_part, inode_id, INODE_BTMP);

    /* new_file_inode加入open_inodes链表 */
    list_push_back(&cur_part->open_inodes, &new_file_inode->inode_tag);
    new_file_inode->i_open_cnts = 1;

    sys_free(io_buf);
    return pcb_fd_install(fd);

ROLL_BACK:
    switch (roll_back_step) {
        case 3:
            memset(&file_table[fd], 0, sizeof(struct file));
            // 这里不要break, 对于case 3 也需要执行case 2 和 case 1 的内容 
        case 2:
            sys_free(new_file_inode);
        case 1:
            bitmap_setbit(&cur_part->inode_btmp, inode_id, INODE_BTMP_BIT_FREE);
            break;
    }
    sys_free(io_buf);
    return -1;
}

/* 打开文件 */
int32_t file_open(uint32_t inode_id, uint8_t flag) {
    int32_t fd = get_free_slot_in_global();
    if (fd == -1) {
        return -1;
    }

    file_table[fd].fd_pos = 0;
    file_table[fd].fd_flag = flag;
    file_table[fd].fd_inode = inode_open(cur_part, inode_id); 

    if (flag & O_WONLY || flag & O_RW) { // 文件可写
        enum intr_status old_status = intr_disable(); // 关闭中断
        if ( !file_table[fd].fd_inode->write_deny) { // 若当前没有其他进程写该文件
            file_table[fd].fd_inode->write_deny = true;
            intr_set_status(old_status); // 恢复中断状态
        }
        else { // 当前已有进程在写该文件
            intr_set_status(old_status);
            k_printf("file can`t be write now, try again later!!!\n");
            inode_close(cur_part, file_table[fd].fd_inode);
            file_table[fd].fd_inode = NULL;
            return -1;
        }
    }

    return pcb_fd_install(fd);
}

/* 关闭文件 */
int32_t file_close(struct file *file) {
    if (file == NULL)
        return -1;
    if (file->fd_inode == NULL)
        return -1;
    file->fd_inode->write_deny = false;
    inode_close(cur_part, file->fd_inode);
    return 0;
}
