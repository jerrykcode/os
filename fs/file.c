#include "file.h"
#include "fs.h"
#include "inode.h"
#include "stddef.h"
#include "stdio-kernel.h"
#include "super_block.h"
#include "thread.h"
#include "interrupt.h"
#include "list.h"
#include "debug.h"

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

/* 将内存地址src处起始的count字节数据写入文件file末尾 成功返回写入字节数量，失败返回-1 */
int32_t file_write(struct file *file, const void *src, uint32_t count) {
    if (count == 0) {
        return 0;
    }
    struct disk_st *disk = cur_part->my_disk;
    struct inode_st *fd_inode = file->fd_inode;
    if (fd_inode->i_size + count > 140 * BLOCK_SIZE) {
        k_printf("ERROR file_write: exceed max file size %d bytes, write file failed!!!\n", 140 * BLOCK_SIZE);
        return -1;
    }

    void *io_buf = sys_malloc(1024); // 1024: 2*SECTOR_SIZE inode_sync需要两扇区大小的内存
    if (io_buf == NULL) {
        k_printf("ERROR file_write: alloc memory failed!!!\n");
        return -1;
    }

    uint32_t *all_blocks = (uint32_t *)sys_malloc(140 * sizeof(uint32_t)); // 存储块的lba地址
    if (all_blocks == NULL) {
        k_printf("ERROR file_write: alloc memory failed!!!\n");
        return -1;
    }

    // 当前已使用的块数
    uint32_t file_has_used_blocks = fd_inode->i_size / BLOCK_SIZE;
    if (fd_inode->i_size % BLOCK_SIZE) { 
        // 已使用的最后一块中有剩余空间可以使用
        // 将这一块存入all_blocks中
        if (file_has_used_blocks < 12)
            all_blocks[file_has_used_blocks] = fd_inode->i_sectors[file_has_used_blocks];
        else {
            // 间接块需要读取i_sectors[12]
            ASSERT(fd_inode->i_sectors[12] != 0);
            ide_read(disk, fd_inode->i_sectors[12], 1, all_blocks + 12); 
            // 这样all_blocks中还存储了最后一块之前的一些块，但是没关系
        }
        file_has_used_blocks++;
    }
    // 写入数据之后将会使用的块数
    uint32_t file_will_use_blocks = (fd_inode->i_size + count) / BLOCK_SIZE;
    if ((fd_inode->i_size + count) % BLOCK_SIZE)
        file_will_use_blocks++;

    uint32_t block_lba;
    uint32_t block_btmp_idx;
    // 申请需要使用的块
    for (int i = file_has_used_blocks; i < file_will_use_blocks; i++) {
        if (i < 12) {
            // 直接块
	    ASSERT(fd_inode->i_sectors[i] == 0);
    	    block_lba = block_bitmap_alloc(cur_part);
	    if (block_lba == -1) {
	        k_printf("ERROR file_write: block bitmap alloc failed!!!\n");
	        sys_free(io_buf);
	        sys_free(all_blocks);
	        return -1;
	    }
            // 将bitmap同步至硬盘
            block_btmp_idx = block_lba - cur_part->sb->data_start_lba;
            ASSERT(block_btmp_idx != 0);
            bitmap_sync(cur_part, block_btmp_idx, BLOCK_BTMP);
            // 记录到all_blocks
	    all_blocks[i] = fd_inode->i_sectors[i] = block_lba;
        }
        else if (i == 12) {
            // 需要先申请一块作为i_sectors[12]
            // 再申请间接块
            ASSERT(fd_inode->i_sectors[12] == 0);
            block_lba = block_bitmap_alloc(cur_part);
            if (block_lba == -1) {
                k_printf("ERROR file_write: block bitmap alloc failed!!!\n");
	        sys_free(io_buf);
	        sys_free(all_blocks);
	        return -1;
            }
            block_btmp_idx = block_lba - cur_part->sb->data_start_lba;
            ASSERT(block_btmp_idx != 0);
            bitmap_sync(cur_part, block_btmp_idx, BLOCK_BTMP);
            fd_inode->i_sectors[12] = block_lba;

            // 申请间接块
            block_lba = block_bitmap_alloc(cur_part);
            if (block_lba == -1) {
                k_printf("ERROR file_write: block bitmap alloc failed!!!\n");
	        sys_free(io_buf);
	        sys_free(all_blocks);
	        return -1;
            }
            block_btmp_idx = block_lba - cur_part->sb->data_start_lba;
            ASSERT(block_btmp_idx != 0);
            bitmap_sync(cur_part, block_btmp_idx, BLOCK_BTMP);
            // 记录到all_blocks
            all_blocks[12] = block_lba;
        }
        else { // i > 12
            // 申请间接块，与 i == 12 的情况不同，这里i_sectors[12]已经有值了
            ASSERT(fd_inode->i_sectors[12] != 0);
            block_lba = block_bitmap_alloc(cur_part);
            if (block_lba == -1) {
                k_printf("ERROR file_write: block bitmap alloc failed!!!\n");
                sys_free(io_buf);
                sys_free(all_blocks);
                return -1;
            }
            block_btmp_idx = block_lba - cur_part->sb->data_start_lba;
            ASSERT(block_btmp_idx != 0);
            bitmap_sync(cur_part, block_btmp_idx, BLOCK_BTMP);
            // 记录到all_blocks
            all_blocks[i] = block_lba;
        }
    } // for
    // 如果申请了间接块，需要将这些间接块写入i_sectors[12]所指向的扇区
    if (file_has_used_blocks < file_will_use_blocks && file_will_use_blocks >= 12) {
        ide_write(disk, fd_inode->i_sectors[12], 1, all_blocks + 12);
    }

    // 以下开始向all_blocks中记录的这些块写入内容
    void *io_src = src;
    uint32_t bytes_left = count;

    uint32_t io_size;
    uint32_t off = fd_inode->i_size % BLOCK_SIZE;
    if (off) {
        // 如果已使用的最后一块中有剩余空间可以使用
        ide_read(disk, all_blocks[file_has_used_blocks - 1], 1, io_buf);
        io_size = BLOCK_SIZE - off;
        memcpy(io_buf + off, io_src, io_size);
        ide_write(disk, all_blocks[file_has_used_blocks - 1], 1, io_buf);
        io_src += io_size;
        bytes_left -= io_size;
    }

    // 写入all_blocks记录的块
    for (int i = file_has_used_blocks; i < file_will_use_blocks; i++) {
        if (BLOCK_SIZE <= bytes_left)
            io_size = BLOCK_SIZE;
        else {
            io_size = bytes_left;
            memset(io_buf, 0, BLOCK_SIZE);
        }
        memcpy(io_buf, io_src, io_size);
        ide_write(disk, all_blocks[i], 1, io_buf);
        io_src += io_size; 
        bytes_left -= io_size;
    }
    sys_free(all_blocks);

    ASSERT(bytes_left == 0);
    fd_inode->i_size += count;
    inode_sync(cur_part, fd_inode, io_buf);

    sys_free(io_buf);
    return count;
}

/* 將文件file中的count個字節讀入內存地址dest處 */
// 等會去下載簡體輸入法，這個函數的註釋先這樣湊合下，~看起來好像也挺好QWQ
int32_t file_read(struct file *file, void *dest, uint32_t count) {
    if (count == 0) {
        return 0;
    }
    struct indoe_st *fd_inode = file->fd_inode;
    if (file->fd_pos + count > fd_inode->i_size) { // 如果超出文件結尾
        count = fd_inode->i_size - file->fd_pos;
        if (count == 0) 
            return -1;
    }

    struct disk_st *disk = cur_part->my_disk;

    uint32_t start_block_idx = file->fd_pos / BLOCK_SIZE; // 要讀取的首個塊(block)
    uint32_t end_block_idx = (file->fd_pos + count - 1) / BLOCK_SIZE; // 要讀取的最後一個塊

    uint32_t *all_blocks = (uint32_t *)sys_malloc(140 * sizeof(uint32_t));
    if (all_blocks == NULL) {
        k_printf("ERROR file_read: alloc memory failed!!!\n");
        return -1;
    }
    void *io_buf = sys_malloc(BLOCK_SIZE);
    if (io_buf == NULL) {
        k_printf("ERROR file_read: alloc memory failed!!!\n");
        sys_free(all_blocks);
        return -1;
    }

    // 將需要讀取的直接塊lba記錄入all_blocks
    for (int i = start_block_idx; i <= min(11, end_block_idx); i++) {
        ASSERT(fd_inode->i_sectors[i] != 0);
        all_blocks[i] = fd_inode->i_sectors[i];
    }

    // 將間接塊記錄到all_blocks
    if (end_blocks_idx >= 12) {
        ASSERT(fd_inode->i_sectors[12] != 0);
        ide_read(disk, fd_inode->i_sectors[12], 1, all_blocks + 12);
    }

    // 讀取all_blocks中記錄的塊
    void *io_dest = dest;
    uint32_t io_size;
    uint32_t bytes_left = count;
    uint32_t off = file->fd_pos % BLOCK_SIZE;
    if (off) {
        ide_read(disk, all_blocks[start_block_idx], 1, io_buf);
        io_size = BLOCK_SIZE - off;
        // 首個塊前off字節不讀取
        memcpy(io_dest, io_buf + off, io_size);
        io_dest += io_size;
        bytes_left -= io_size;
        start_block_idx++;
    }

    // 以下的塊均從塊起始位置讀取
    for (int i = start_block_idx; i <= end_block_idx; i++) {
        io_size = min(BLOCK_SIZE, bytes_left);
        ide_read(disk, all_blocks[i], 1, io_buf);
        memcpy(io_dest, io_buf, io_size);
        io_dest += io_size;
        bytes_left -= io_size;
    }

    ASSERT(bytes_left == 0);

    sys_free(all_blocks);
    sys_free(io_buf);
    return count;
}
