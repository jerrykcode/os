#include "dir.h"
#include "super_block.h"
#include "memory.h"
#include "debug.h"
#include "string.h"
#include "stdio-kernel.h"
#include "file.h"

struct dir_st root_dir;

/* 打开根目录 */
void open_root_dir(struct partition_st *part) {
    root_dir.inode = inode_open(part, part->sb->root_inode_id);
    root_dir.dir_pos = 0;
}

/* 打开分区part中inode号为inode_id的目录，并返回目录指针 */
struct dir_st *dir_open(struct partition_st *part, uint32_t inode_id) {
    struct dir_st *dir = (struct dir_st *)sys_malloc(sizeof(struct dir_st));
    dir->inode = inode_open(part, inode_id);
    dir->dir_pos = 0;
    return dir;
}

/* 关闭目录 */
void dir_close(struct dir_st *dir) {
    if (dir == &root_dir) // 根目录不做处理
        return;
    inode_close(&cur_part, dir->inode);
    sys_free(dir);
}

/* 在分区part的dir目录下查询名字为entry_name的目录项，
   若存在就将该目录项填充到entry并返回true，
   若不存在就返回false */
bool dir_search(struct partition_st *part, struct dir_st *dir, const char *entry_name, struct dir_entry_st *entry) {
    ASSERT(part != NULL);
    ASSERT(dir != NULL);
    ASSERT(entry != NULL);

    struct inode_st *inode = dir->inode;
    uint32_t blocks_num = 140;
    uint32_t *all_blocks = (uint32_t *)sys_malloc(140 * sizeof(uint32_t));
    if (all_blocks == NULL) {
        k_printf("ERROR dir_search: alloc memory failed!!!");
        return false;
    }

    int i, j;
    for (i = 0; i < 12; i++) { // 直接块
        all_blocks[i] = inode->i_sectors[i];
    }

    bool more_blocks = false; // 记录有无间接块
    if (inode->i_sectors[12] != 0) { // 存在间接块
        more_blocks = true;
        ide_read(part->my_disk, inode->i_sectors[12], 1, &all_blocks[12]);
    }

    void *io_buf = sys_malloc(SECTOR_SIZE);
    if (io_buf == NULL) {
        k_printf("ERROR dir_search: alloc memory failed!!!");
        sys_free(all_blocks);
        return false;
    } 

    struct dir_entry_st *pe;
    uint32_t entry_per_block = SECTOR_SIZE / sizeof(struct dir_entry_st);
    bool found = false;

    for (i = 0; i < blocks_num; i++) {
        if (all_blocks[i] == 0) {
            if (!more_blocks && i == 12) // 没有间接块, 已经遍历完
                break;
            continue;
        }

        ide_read(part->my_disk, all_blocks[i], 1, io_buf);
        pe = (struct dir_entry_st *)io_buf;

        for (j = 0; j < entry_per_block; j++) {
            if (strcmp(pe->filename, entry_name) == 0) {// 找到了
                memcpy(entry, pe, sizeof(struct dir_entry_st));
                found = true;
                break;
            }
            pe++;
        }
        if (found)
            break;
    }
    sys_free(all_blocks);
    sys_free(io_buf);
    return found;
}

/* 初始化目录项 */
void dir_init_entry(struct dir_entry_st *entry, const char *filename, uint32_t inode_id, uint8_t f_type) {
    ASSERT(entry != NULL);
    ASSERT(strlen(filename) <= MAX_FILE_NAME_LEN);
    strcpy(entry->filename, filename);
    entry->inode_id = inode_id;
    entry->f_type = f_type;
}

/* 将目录项entry写入dir目录, io_buf由主调函数提供 */
// 执行该函数时分区为全局变量cur_part
bool dir_sync_entry(struct dir_st *dir, struct dir_entry_st *entry, void *io_buf) {
    ASSERT(dir != NULL);
    ASSERT(entry != NULL);
    ASSERT(io_buf != NULL);

    struct inode_st *inode = dir->inode;
    uint32_t blocks_num = 140;
    uint32_t *all_blocks = (uint32_t *)sys_malloc(140 * sizeof(uint32_t));
    if (all_blocks == NULL) {
        k_printf("ERROR dir_sync_entry: alloc memory failed!!!");
        return false;
    }

    int i, j;
    for (i = 0; i < 12; i++) {
        all_blocks[i] = inode->i_sectors[i]; // 直接块
    }

    if (inode->i_sectors[12] != 0) {
        // 读取间接块
        ide_read(cur_part->my_disk, inode->i_sectors[12], 1, all_blocks[12]);
    }

    struct dir_entry_st *pe;
    uint32_t entry_per_block = SECTOR_SIZE / sizeof(struct dir_entry_st);
    bool result = false;
    int32_t btmp_changed_bits_idx[2] = {-1}; // 记录位图更改的位

    for (i = 0; i < blocks_num; i++) { // 枚举块
        if (all_blocks[i]) { // 块存在            
            ide_read(cur_part->my_disk, all_blocks[i], 1, io_buf);
            pe = (struct dir_entry_st *)io_buf;
            for (j = 0; j < entry_per_block; j++) { // 枚举块中的block
                if (pe->filename[0] == '\0') { // 空闲
                    memcpy(pe, entry, sizeof(struct dir_entry_st));
                    ide_write(cur_part->my_disk, all_blocks[i], 1, io_buf);
                    inode->i_size += sizeof(struct dir_entry_st);
                    result = true;
                    goto SYNC_INODE;
                }
                pe++;
            }
        }
        else { // 块不存在
            // 分配一个块
            int32_t block_lba = block_bitmap_alloc(cur_part);
            if (block_lba == -1) {
                k_printf("ERROR dir_sync_entry: alloc bitmap failed!!!");
                goto END;
            }
            btmp_changed_bits_idx[0] = block_lba - cur_part->sb->data_start_lba; // 记录

            if (i < 12) { // 直接块
                inode->i_sectors[i] = block_lba;
            }
            else if (i == 12) { // inode->i_sectors[12]是一个lba地址，该地址的的扇区可以存储128个间接块
                                // 刚刚分配的块给inode->i_sectors[12]使用，
                                // 现在需要再分配一个块，作为第一个间接块
                inode->i_sectors[12] = block_lba;
                block_lba = block_bitmap_alloc(cur_part);
                if (block_lba == -1) {
                    k_printf("ERROR dir_sync_entry: alloc bitmap failed!!!");
                    inode->i_sectors[12] = 0; // 复原
                    bitmap_setbit(&cur_part->block_btmp, btmp_changed_bits_idx[0], BLOCK_BTMP_BIT_FREE);
                    goto END;
                }
                btmp_changed_bits_idx[1] = block_lba - cur_part->sb->data_start_lba; // 记录

                all_blocks[12] = block_lba; // 首个间接块
                // 将 &all_blocks[12] 开始的一扇区内容(128个间接块，目前只有1个)写入inode->i_sectors[12]处
                ide_write(cur_part->my_disk, inode->i_sectors[12], 1, all_blocks + 12);
            }
            else { // i > 12 间接块
                all_blocks[i] = block_lba;
                // 新增了一个间接块
                // 更新inode->i_sectors[12]处扇区的内容
                ide_write(cur_part->my_disk, inode->i_sectors[12], 1, all_blocks + 12);
            }

            // 将entry写入block_lba块
            memset(io_buf, 0, SECTOR_SIZE);
            memcpy(io_buf, entry, sizeof(struct dir_entry_st));
            ide_write(cur_part->my_disk, block_lba, 1, io_buf);
            inode->i_size += sizeof(struct dir_entry_st);
            result = true;
            goto SYNC_BTMP;
        }
    }

// 持久化操作
SYNC_BTMP:
    for (i = 0; i < 2; i++) {
        if (btmp_changed_bits_idx[i] != -1) {
            bitmap_sync(cur_part, btmp_changed_bits_idx[i], BLOCK_BTMP);
        }
    }

SYNC_INODE:
    inode_sync(cur_part, inode, io_buf);

END:
    sys_free(all_blocks);
    return result;
}