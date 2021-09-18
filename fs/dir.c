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

static void it_init(struct dir_entry_iterator *it, struct dir_st *dir);
static void it_close(struct dir_entry_iterator *it);

/* 打开分区part中inode号为inode_id的目录，并返回目录指针 */
struct dir_st *dir_open(struct partition_st *part, uint32_t inode_id) {
    struct dir_st *dir = (struct dir_st *)sys_malloc(sizeof(struct dir_st));
    dir->inode = inode_open(part, inode_id);
    dir->dir_pos = 0;
    it_init(&dir->dir_it, dir);
    return dir;
}

/* 关闭目录 */
void dir_close(struct dir_st *dir) {
    if (dir == &root_dir) // 根目录不做处理
        return;
    inode_close(&cur_part, dir->inode);
    it_close(&dir->dir_it);
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

/* 将目录项entry写入dir目录, io_buf由主调函数提供, io_buf需要2*SECTOR_SIZE字节 */
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

// 删除block
static void block_delete(struct partition_st *part, uint32_t block_lba) {
    uint32_t block_btmp_idx = block_lba - part->sb->data_start_lba;
    ASSERT(block_btmp_idx > 0);
    bitmap_setbit(&part->block_btmp, block_btmp_idx, 0);
    bitmap_sync(part, block_btmp_idx, BLOCK_BTMP);
}

/* 在分区part的目录dir中删除inode号为dl_inode_id的目录项，io_buf由主调函数提供 */
bool dir_delete_entry(struct partition_st *part, struct dir_st *dir, uint32_t dl_inode_id, void *io_buf) {
    struct disk_st *disk = part->my_disk;
    struct inode_st *dir_inode = dir->inode;
    uint32_t *all_blocks = (uint32_t *)sys_malloc(140 * sizeof(uint32_t));
    if (all_blocks == NULL) {
        k_printf("ERROR dir_delete_entry: alloc memory failed!!!\n");
        return false;
    }

    int i, j;
    for (i = 0; i < 12; i++) {
        // 复制直接块lba
        all_blocks[i] = dir_inode->i_sectors[i];
    }
    if (dir_inode->i_sectors[12]) {
        // 读取间接块lba
        ide_read(disk, dir_inode->i_sectors[12], 1, all_blocks + 12);
    }

    uint32_t entry_count;
    struct dir_entry_st *dir_entry;
    uint32_t entry_per_block = BLOCK_SIZE / sizeof(struct dir_entry_st);
    uint32_t dl_entry_idx = -1;
    bool result = false; // 返回值，初始化为默认false

    for (i = 0; i < 140; i++) { // 枚举所有块
        if (all_blocks[i] == 0)
            continue;

        entry_count = 0; // 记录块中的目录项数目, 初始化为0
        ide_read(disk, all_blocks[i], 1, io_buf); // 读入块
        dir_entry = (struct dir_entry_st *)io_buf; // 目录项指针，初始指向io_buf第0项，即块中首个目录项
        
        for (j = 0; j < entry_per_block; j++) { // 枚举目录项
            if ((dir_entry + j)->f_type != FT_UNKNOWN) {
                entry_count++;
                if ((dir_entry + j)->inode_id == dl_inode_id) {
                    // 找到要删除的目录项了
                    dl_entry_idx = j;
                    // 这个时候不能break, 因为还要entry_count继续计数
                }
            }
        }
        if (dl_entry_idx == -1) // 这一块中没找到，继续去下一块中找
            continue;

        // 以下代码 已经找到待删除目录项了

        if (entry_count > 1) { // 除了待删除项以外还有其他目录项
            memset(dir_entry + dl_entry_idx, 0, sizeof(struct dir_entry_st)); // 将待删除的目录项填充0
            ide_write(disk, all_blocks[i], 1, io_buf); // 写回硬盘
        }
        else { // entry_count == 1, 即块中除了待删除项以外没有其他项了
            // 那么删除之后，这一块中就没有内容了，所以这一块也需要被释放掉
            block_delete(part, all_blocks[i]);
            all_blocks[i] = 0;

            if (i < 12) { // 刚刚删掉的块是直接块
                dir_inode->i_sectors[i] = 0;
            }
            else { // i >= 12 删掉的是间接块
                // 检测是否还存在间接块
                for (j = 12; j < 140; j++) {
                    if (all_blocks[j])
                         break;
                }

                if (j < 140) { // 存在间接块
                    // 更新i_sectors[12]指向的块的内容
                    ide_write(disk, dir_inode->i_sectors[12], 1, all_blocks + 12);
                }
                else { // 不存在间接块
                    // i_sectors[12]已失去意义，也要被释放
                    block_delete(part, dir_inode->i_sectors[12]);
                    dir_inode->i_sectors[12] = 0;
                } // if 是否存在间接块 ?
            } // if 是否直接块 ?
        } // if 块中是否存在其他目录项 ?

        dir_inode->i_size -= sizeof(struct dir_entry_st);
        inode_sync(part, dir_inode, io_buf);
        result = true;
        break;
    } //for

    sys_free(all_blocks);
    return result;
}

// 迭代器相关函数
static void it_init(struct dir_entry_iterator *it, struct dir_st *dir) {
    it->dir = dir;
    it->cur_block_idx = 0;
    it->cur_entry_idx = 0;
    it->block_buf = NULL;
    it->blocks = NULL;
    it->entry_count = 0;
    it->rewind = false;
}

static struct dir_entry_st *it_next(struct dir_entry_iterator *it) {
    struct dir_st *dir = it->dir;
    struct inode_st *dir_inode = dir->inode;
    uint32_t dir_entry_size = cur_part->sb->dir_entry_size;

    ASSERT(dir_inode->i_size % dir_entry_size == 0);
    if (it->entry_count >= dir_inode->i_size / dir_entry_size) // 已读取所有目录项
        return NULL;

    if (it->block_buf == NULL) {
        ASSERT(it->cur_block_idx == 0); // it_init()之后首次执行it_next()
        it->block_buf = sys_malloc(BLOCK_SIZE);
        if (it->block_buf == NULL) {
            k_printf("ERROR it_next: alloc memory failed!\n");
            return NULL;
        }
        ide_read(cur_part->my_disk, dir_inode->i_sectors[0], 1, it->block_buf); //从硬盘读入dir的第0个块
    }
    else if (it->rewind) {
        // rewind过，重新读取dir_inode->i_sectors[0]
        ASSERT(it->cur_block_idx == 0);
        ide_read(cur_part->my_disk, dir_inode->i_sectors[0], 1, it->block_buf);
    }

    uint32_t entry_per_block = BLOCK_SIZE / dir_entry_size;
    struct dir_entry_st *entry;
    while (it->cur_block_idx < 140) {
        entry = (struct dir_entry_st *)it->block_buf;
        for ( ; it->cur_entry_idx < entry_per_block; it->cur_entry_idx++) {
            if ((entry + it->cur_entry_idx)->f_type != FT_UNKNOWN) { // 找到一个目录项
                dir->dir_pos += dir_entry_size;
                it->entry_count++;
                return entry + it->cur_entry_idx++; // ++是为了指向下一项，方便下一次调用it_next()
            }
        }
        // 进入下一个块
        if (++it->cur_block_idx < 12) { // 更新it->cur_block_idx并判断是直接块还是间接块
            // 从硬盘读取新的块，it->block_buf更新为新的块的内容
            ide_read(cur_part->my_disk, dir_inode->i_sectors[it->cur_block_idx], 1, it->block_buf);
        }
        else { // 间接块
            if (it->blocks == NULL) {
                it->blocks = (uint32_t *)sys_malloc(BLOCK_SIZE);
                if (it->blocks == NULL) {
                    k_printf("ERROR it_next: alloc memory failed!!!\n");
                    return NULL;
                }
                ide_read(cur_part->my_disk, dir_inode->i_sectors[12], 1, it->blocks);
            }
            ide_read(cur_part->my_disk, it->blocks[it->cur_block_idx], 1, it->block_buf);
        } 
        it->cur_entry_idx = 0; // 进入新的块后要从0开始遍历这个块中的目录项
    }
    return NULL;
}

static void it_close(struct dir_entry_iterator *it) {
    if (it->block_buf)
        sys_free(it->block_buf);

    if (it->blocks)
        sys_free(it->blocks);
}

/* 读取目录，成功返回一个目录项，失败返回NULL */
struct dir_entry_st *dir_read(struct dir_st *dir) {
    if (dir == NULL)
        return NULL;
    return it_next(&dir->dir_it);
}

/* 在父目录的inode结构中寻找inode号为child_inode_id的子目录项的名字 */
static bool search_child_filename(struct inode_st *parent_inode, int32_t child_inode_id, char *filename, void *io_buf) {
    struct partition_st *part = cur_part;
    uint32_t *all_blocks = (uint32_t *)sys_malloc(BLOCK_SIZE);
    if (all_blocks == NULL) {
        k_printf("ERROR search_child_filename: alloc memory failed!!!\n");
        return false;
    }
    uint32_t blocks_num = 12;
    int i, j;
    for (i = 0; i < blocks_num; i++) {
        all_blocks[i] = parent_inode->i_sectors[i];
    }
    if (parent_inode->i_sectors[12]) {
        ide_read(part->my_disk, parent_inode->i_sectors[12], 1, all_blocks + 12);
        blocks_num = 140;
    }

    struct dir_entry_st *entry;
    uint32_t entry_per_block = BLOCK_SIZE / part->sb->dir_entry_size;
    for (i = 0; i < blocks_num; i++) {
        if (all_blocks[i] == 0)
            continue;
        ide_read(part->my_disk, all_blocks[i], 1, io_buf);
        entry = (struct dir_entry_st *)io_buf;
        for (j = 0; j < entry_per_block; j++) {
            if ((entry + j)->inode_id == child_inode_id) {
                strcpy(filename, (entry + j)->filename);
                sys_free(all_blocks);
                return true;
            }
        }
    }

    sys_free(all_blocks);
    return false;
}

/* 通过目录的inode结构体寻找其名字，并返回其父目录的inode结构体 */
static struct inode_st *dir_inode2filename(struct inode_st *inode, char *filename, void *io_buf) {
    struct partition_st *part = cur_part;
    ASSERT(inode->i_sectors[0] != 0);
    ide_read(part->my_disk, inode->i_sectors[0], 1, io_buf);
    struct dir_entry_st *parent_entry = (struct dir_entry_st *)(io_buf) + 1; // 第1个目录项，理论上是父目录".."
    ASSERT(parent_entry->f_type == FT_DIRECTORY);
    ASSERT(strcmp(parent_entry->filename, "..") == 0);
    int32_t parent_inode_id = parent_entry->inode_id;
    struct inode_st *parent_inode = inode_open(part, parent_inode_id);
    search_child_filename(parent_inode, inode->i_id, filename, io_buf);
    return parent_inode;
}

static uint32_t max(uint32_t a, uint32_t b) {
    return a > b ? a : b;
}

/* 计算inode号为dir_inode_id的目录的完整绝对路径, 成功返回0，失败返回-1 */
int32_t dir_getcwd(int32_t dir_inode_id, char *pathname) {
    if (dir_inode_id < 0)
        return -1;

    if (dir_inode_id == 0) { // 根目录
        pathname[0] = '/';
        pathname[1] = '\0';
        return 0;
    }

    void *buf = sys_malloc(max(BLOCK_SIZE, MAX_PATH_LEN));
    if (buf == NULL) {
        k_printf("ERROR dir_getcwd: alloc memory failed!!!\n");
        return -1;
    }
    void *io_buf = buf;
    char filename[MAX_FILE_NAME_LEN] = {0};
    struct partition_st *part = cur_part;
    struct inode_st *inode = inode_open(part, dir_inode_id);

    uint32_t depth = 0;
    struct inode_st *parent_inode;
    while (inode->i_id) { // inode不是根目录
        parent_inode = dir_inode2filename(inode, filename, io_buf);
        inode_close(part, inode);
        inode = parent_inode;
        strcat(pathname, "/");
        strcat(pathname, filename);
        depth++;
    }
    inode_close(part, inode);

    char *tmp_path = (char *)buf;
    memset(tmp_path, 0, MAX_PATH_LEN);
    for (int i = 0; i < depth; i++) {
        char *last_slash = strrchr(pathname, '/');
        strcat(tmp_path, last_slash);
        *last_slash = '\0';
    }
    memcpy(pathname, tmp_path, strlen(tmp_path));

    sys_free(buf);
}
