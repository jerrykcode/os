#include "fs.h"
#include "file.h"
#include "super_block.h"
#include "inode.h"
#include "dir.h"
#include "file.h"
#include "ide.h"
#include "thread.h"
#include "memory.h"
#include "string.h"
#include "stdio-kernel.h"
#include "global.h"
#include "bitmap.h"
#include "debug.h"
#include "list.h"

#define SUPER_BLOCK_MAGIC_FILE_SYS  0x20210819

struct partition_st *cur_part; // 默认情况下操作的分区

/* list_traversal的回调函数 */
static bool mount_partition(list_node node, int arg) {
    char *part_name = (char *)arg;
    struct partition_st *part = elem2entry(struct partition_st, part_tag, node);
    if (!strcmp(part_name, part->name)) {
        // 找到需要被挂载的分区
        cur_part = part;
        struct disk_st *hd = cur_part->my_disk;

        // 读入超级块
        ASSERT(sizeof(struct super_block_st) == SECTOR_SIZE);
        struct super_block_st *sb = (struct super_block_st *)sys_malloc(SECTOR_SIZE);
        if (sb == NULL)
            PANIC("alloc memory failed!");

        ide_read(hd, cur_part->start_lba + 1, 1, sb);

        cur_part->sb = sb;

        // 读入块位图
        cur_part->block_btmp.btmp_bytes_len = sb->block_btmp_sec_num * SECTOR_SIZE;
        cur_part->block_btmp.bits = sys_malloc(cur_part->block_btmp.btmp_bytes_len);
        if (cur_part->block_btmp.bits == NULL)
            PANIC("alloc memory failed!");

        ide_read(hd, sb->block_btmp_lba, sb->block_btmp_sec_num, cur_part->block_btmp.bits);

        // 读入inode位图
        cur_part->inode_btmp.btmp_bytes_len = sb->inode_btmp_sec_num * SECTOR_SIZE;
        cur_part->inode_btmp.bits = sys_malloc(cur_part->inode_btmp.btmp_bytes_len);
        if (cur_part->inode_btmp.bits == NULL)
            PANIC("alloc memory failed!");

        ide_read(hd, sb->inode_btmp_lba, sb->inode_btmp_sec_num, cur_part->inode_btmp.bits);

        // 初始化inode链表
        list_init(&cur_part->open_inodes);

        return true; // 主调函数结束遍历
    }
    return false; // 主调函数继续遍历链表
}

static uint32_t max(uint32_t a, uint32_t b) {
    return a > b ? a : b;
}

static uint32_t max3(uint32_t a, uint32_t b, uint32_t c) {
    return max(max(a, b), c);
}

static void partition_format(struct partition_st *part) {
    /* 一个块大小是一扇区
       分区布局:

        OS引导块 / 超级块 / 空闲块位图block_btmp / inode_btmp / inode_table / 根目录 / 空闲区域
    */

    /* 计算分区中各部分占用的扇区数 */
    uint32_t boot_sector_sec_num = 1; // 引导扇区占1扇区
    uint32_t super_block_sec_num = 1; // 超级块占1扇区
    uint32_t inode_btmp_sec_num  = DIV_ROUND_UP(MAX_FILES_PER_PART, BITS_PER_SECTOR);
    uint32_t inode_table_sec_num = DIV_ROUND_UP(MAX_FILES_PER_PART * sizeof (struct inode_st), BITS_PER_SECTOR);

    uint32_t free_sec_num = part->sec_num - boot_sector_sec_num - super_block_sec_num - \
        inode_btmp_sec_num - inode_table_sec_num;

    // 假设块位图占 X 个扇区，那么
    //      (free_sec_num - X) / BITS_PER_SECTOR = X
    //      free_sec_num = (BITS_PER_SECTOR + 1) * X
    //      X = free_sec_num / (BITS_PER_SECTOR + 1)
    uint32_t block_btmp_sec_num = DIV_ROUND_UP(free_sec_num, BITS_PER_SECTOR + 1);
    free_sec_num -= block_btmp_sec_num;
    // 到此，剩余free_sec_num扇区的空间，即free_sec_num个空闲块
    // 每个空闲块需要占用位图中的1位，所以位图至少需要free_sec_num位,
    // 也就是位图位数 >= free_sec_num
    ASSERT(block_btmp_sec_num * BITS_PER_SECTOR >= free_sec_num);

    /* 初始化超级块 */

    struct super_block_st sb;
    sb.magic = SUPER_BLOCK_MAGIC_FILE_SYS;
    sb.sec_num = part->sec_num;
    sb.inode_num = MAX_FILES_PER_PART;
    sb.part_lba_base = part->start_lba;

    sb.block_btmp_lba = sb.part_lba_base + 2; // 第0块是引导块,第1块是超级块
    sb.block_btmp_sec_num = block_btmp_sec_num;

    sb.inode_btmp_lba = sb.block_btmp_lba + sb.block_btmp_sec_num;
    sb.inode_btmp_sec_num = inode_btmp_sec_num;

    sb.inode_table_lba = sb.inode_btmp_lba + inode_btmp_sec_num;
    sb.inode_table_sec_num = inode_table_sec_num;

    sb.data_start_lba = sb.inode_table_lba + inode_table_sec_num;
    sb.root_inode_id = 0;
    sb.dir_entry_size = sizeof(struct dir_entry_st);

    k_printf("%s info:\n", part->name);
    k_printf("    magic:0x%x\n    part_lba_base:0x%x\n    all_sec_num:0x%x\n    inode_num:0x%x\n    block_btmp_lba:0x%x\n    block_btmp_sec_num:0x%x\n    inode_btmp_lba:0x%x\n    inode_btmp_sec_num:    0x%x\n    inode_table_lba:0x%x\n    inode_table_sec_num:0x%x\n    data_start_lba:0x%x\n", 
        sb.magic, sb.part_lba_base, sb.sec_num, sb.inode_num, sb.block_btmp_lba, sb.block_btmp_sec_num,
        sb.inode_btmp_lba, sb.inode_btmp_sec_num, sb.inode_table_lba, sb.inode_table_sec_num, sb.data_start_lba);

    struct disk_st *hd = part->my_disk;

    /* 将超级块写入part分区的1扇区 */

    ide_write(hd, part->start_lba + 1, 1, &sb);
    k_printf("  super_block_lba:0x%x\n", part->start_lba + 1);

    /* 找出占用扇区最大的元信息，用其尺寸创建存储缓冲区 */
    uint32_t buf_size = max3(sb.block_btmp_sec_num, sb.inode_btmp_sec_num, sb.inode_table_sec_num) * SECTOR_SIZE;
    uint8_t *buf = (uint8_t *)sys_malloc(buf_size);

    struct bitmap btmp;
    btmp.bits = buf;

    /* 初始化块位图 */
    btmp.btmp_bytes_len = sb.block_btmp_sec_num * SECTOR_SIZE;
    bitmap_init(&btmp, 0); // 初始化位图
    bitmap_setbit(&btmp, 0, 1); // 第0块预留给根目录，先占位
    for (uint32_t i = free_sec_num; i < sb.block_btmp_sec_num * BITS_PER_SECTOR; i++) {
        bitmap_setbit(&btmp, i, 1); // free_sec_num及之后的位映射的块已超出本分区范围，置1表示无效
    }

    /* 将块位图写入硬盘分区 */
    // buf作为btmp.bits指向的内存，在上面的位图操作中已被赋值
    ide_write(hd, sb.block_btmp_lba, sb.block_btmp_sec_num, buf);

    /* 初始化inode位图 */
    btmp.btmp_bytes_len = sb.inode_btmp_sec_num * SECTOR_SIZE;
    bitmap_init(&btmp, 0); // 初始化，之前块位图操作改动的buf在inode位图的范围内被清0
    bitmap_setbit(&btmp, 0, 1); // 第0个inode分配给根目录
    /* 将inode位图写入硬盘分区 */
    ide_write(hd, sb.inode_btmp_lba, sb.inode_btmp_sec_num, buf);

    /* 初始化inode数组 */
    memset(buf, 0, sb.inode_table_sec_num * SECTOR_SIZE); // 清空缓冲区
    // 将buf看作inode数组，赋值之后再写入硬盘
    struct inode_st *inode = (struct inode_st *)buf; // 第0个inode结构, 即根目录
    inode->i_size = sb.dir_entry_size * 2; // 目前根目录有两项内容，即 "." 和 ".." 目录
    inode->i_sectors[0] = sb.data_start_lba;
    /* 由于上面的memset, i_sectors数组的其他元素，以及inode的其他属性都被初始化为0
    inode->i_id = 0;
    inode->i_open_cnts = 0;
    inode->write_deny = false;
    */
    /* 将inode数组写入硬盘分区 */
    ide_write(hd, sb.inode_table_lba,  sb.inode_table_sec_num, buf);

    /* 初始化根目录的数据内容 */
    // 根目录的数据内容就是"."和".."两个struct dir_entry_st
    // 一个扇区足够
    memset(buf, 0, SECTOR_SIZE);
    // "."
    struct dir_entry_st *p_de = (struct dir_entry_st *)buf;
    memcpy(p_de->filename, ".", 1);
    p_de->inode_id = 0;
    p_de->f_type = FT_DIRECTORY;
    
    // ".."
    p_de++; // 紧接"."的数据内容之后
    memcpy(p_de->filename, "..", 2);
    p_de->inode_id = 0; // 根目录的父目录".."仍然是自己
    p_de->f_type = FT_DIRECTORY;
    /* 将根目录的数据内容写入硬盘分区 */
    ide_write(hd, sb.data_start_lba, 1, buf);

    k_printf("  root_dir_lba:0x%x\n", sb.data_start_lba);
    k_printf("%s format finished\n", part->name);
    sys_free(buf);
}

static void partition_init(struct disk_st *hd, struct partition_st *part, struct super_block_st *sb_buf) {
    memset(sb_buf, 0, SECTOR_SIZE);
    ide_read(hd, part->start_lba + 1, 1, sb_buf); // 读取超级块
    if (sb_buf->magic == SUPER_BLOCK_MAGIC_FILE_SYS) { // 已存在文件系统
        k_printf("%s has file system.\n", part->name);        
    }
    else { // 没有文件系统
        // 创建文件系统
        k_printf("formatting %s\'s partition %s...\n", hd->name, part->name);
        partition_format(part);
    }
}

/* 将pathname的最顶层目录名字解析出来，并存储到top_name中，
   返回pathname的下一层 
   eg: pathname指向"/a/b/c", 执行完成后top_name存储"a", 返回"/b/c" */
static char *path_parse(const char *pathname, char *top_name) {
    if (pathname == NULL)
        return NULL;

    if (pathname[0] == '\0')
        return pathname;

    ASSERT(top_name != NULL);

    char *p = pathname;
    while (p[0] == '/') {
        p++;
    }

    while (p[0] != '\0' && p[0] != '/') {
        *(top_name++) = *(p++);
    }

    *top_name = '\0';
    return p;
}

/* 计算路径深度 */
int32_t path_depth(const char *pathname) {
    char *p = pathname;
    char name_buf[MAX_FILE_NAME_LEN];
    int depth = 0;
    while (p && p[0]) {
        p = path_parse(p, name_buf);
        if (name_buf[0] == '\0')
            break;
        depth++;
    }
    return depth;
}

struct path_search_record {
    char path[MAX_PATH_LEN];
    struct dir_st *parent_dir;
    enum file_types file_type;
};

/* 搜索路径pathname，若存在则返回对应文件的inode号，若不存在则返回-1 */
static int search_file(const char *pathname, struct path_search_record *search_record) {
    ASSERT(search_record != NULL);
    if (pathname == NULL)
        return -1;

    if (pathname[0] != '/') {
        k_printf("ERROR search_file: ");
        return -1;
    }

    // 初始化search_record
    memset(search_record->path, 0, MAX_PATH_LEN);
    search_record->path[0] = '/';
    search_record->parent_dir = &root_dir;
    search_record->file_type = FT_DIRECTORY;

    struct dir_st *parent_dir = &root_dir;
    char name_buf[MAX_FILE_NAME_LEN];
    struct dir_entry_st entry;
    entry.inode_id = 0; // 初始化

    char *p = pathname;
    while (p[0]) {
        // 解析出一层目录
        p = path_parse(p, name_buf);
        if (name_buf[0] == '\0')
            break;

        // 增加搜索路径
        if (strlen(search_record->path) > 1) // == 1则在初始化时已经有'/'了
            strcat(search_record->path, "/");
        strcat(search_record->path, name_buf);
        if (search_record->parent_dir != &root_dir)
            dir_close(search_record->parent_dir);
        search_record->parent_dir = parent_dir;

        // 判断parent_dir中是否存在名为name_buf的目录项
        if (!dir_search(cur_part, parent_dir, name_buf, &entry)) {
            search_record->file_type = FT_UNKNOWN;            
            return -1;
        }

        search_record->file_type = entry.f_type;
        if (search_record->file_type == FT_REGULAR) {
            return entry.inode_id;
        }

        parent_dir = dir_open(cur_part, entry.inode_id);
    }

    if (parent_dir != &root_dir && parent_dir != search_record->parent_dir)
        dir_close(parent_dir);

    return entry.inode_id;
}

/* 打开或创建路径为pathname的普通文件(非目录)，成功返回描述符，失败返回-1 */
int32_t sys_open(const char *pathname, uint8_t flags) {
    ASSERT(pathname != NULL);
    uint32_t pathname_len = strlen(pathname);
    ASSERT(pathname_len >= 1);
    if (pathname[strlen(pathname) - 1] == '/') {
        k_printf("ERROR sys_open: can not open a directory!!!\n");
        return -1;
    }

    ASSERT(flags <= 7); // 7 ==>二进制 111

    int32_t fd = -1;    // 文件描述符，初始化位-1，默认找不到

    struct path_search_record *search_record = (struct path_search_record *)sys_malloc(sizeof(struct path_search_record));
    if (search_record == NULL) {
        k_printf("ERROR sys_open: alloc memory failed!!!\n");
        return -1;
    }
    memset(search_record, 0, sizeof(struct path_search_record));

    // 记录目录深度
    uint32_t pathname_depth = path_depth(pathname);

    // 搜索文件
    int inode_id = search_file(pathname, search_record);
    bool found = (inode_id != -1);
    // 检查pathname路径上是否有中间目录不存在
    uint32_t searched_depth = path_depth(search_record->path);
    if (pathname_depth != searched_depth) {
        k_printf("ERROR sys_open: cannot access %s:\n   subpath %s isn`t exist!!!\n", pathname, search_record->path);
        dir_close(search_record->parent_dir);
        sys_free(search_record);
        return -1;
    }

    if (search_record->file_type == FT_DIRECTORY) {
        k_printf("cannot open a directory with open(), use opendir() instead");
        dir_close(search_record->parent_dir);
        sys_free(search_record);
        return -1;
    }

    bool create = flags & O_CREATE;

    if (found && create) { // 要创建的文件已存在
        k_printf("%s has already exist!\n", pathname);
        dir_close(search_record->parent_dir);
        sys_free(search_record);
        return -1;
    }

    if (!found && !create) { // 要打开的文件不存在
        k_printf("%s dosn`t exist!\n", pathname);
        dir_close(search_record->parent_dir);
        sys_free(search_record);
        return -1;
    }

    // 到这里只有两种情况:
    // a) found && !create 打开已存在的文件
    // b) !found && create 创建不存在的文件
    if (found) { // found && !create
        fd = file_open(inode_id, flags);
        // 若file_open失败返回-1，存储在fd中，本函数最终亦返回-1
        dir_close(search_record->parent_dir);
        sys_free(search_record);
    }
    else { // !found && create
        k_printf("creating file...\n");
        fd = file_create(search_record->parent_dir, strrchr(pathname, '/') + 1, flags);
        dir_close(search_record->parent_dir);
        sys_free(search_record);
    }

    return fd;
}

static int32_t fd_local2global(int32_t local_fd) {
    if (local_fd < 0)
        return -1;
    return current_thread()->fd_table[local_fd];    
}

int32_t sys_close(int32_t fd) {
    if (fd < stderr_fd) {
        return -1;
    }
    int32_t global_fd = fd_local2global(fd);
    int32_t result = file_close(&file_table[global_fd]);
    current_thread()->fd_table[fd] = -1;
    return result;
}

/* 向文件描述符fd写入buf处count字节的数据 */
int32_t sys_write(int32_t fd, const void *buf, uint32_t count) {
    if (fd < 0) {
        k_printf("sys_write: fd: %d error\n", fd);
        return -1;
    }

    if (fd == stdout_fd) {
        char tmp_buf[1024] = {0};
        memcpy(tmp_buf, buf, count);
        console_put_str(tmp_buf);
        return count;
    }

    int32_t g_fd = fd_local2global(fd);
    struct file *f = &file_table[g_fd];
    if (f->fd_flag & O_WONLY || f->fd_flag & O_RW) {
        return file_write(f, buf, count);
    }
    else {
        k_printf("sys_write: not allowed to write file without flag O_WONLY or O_RW\n");
        return -1;
    }
}

int32_t sys_read(int32_t fd, void *dest, uint32_t count) {
    if (fd < 0) {
        k_printf("sys_read: error fd: %d\n", fd);
        return -1;
    }
    int32_t g_fd = fd_local2global(fd);
    return file_read(&file_table[g_fd], dest, count);
}

/* 重置用于文件读写操作的偏移指针，成功返回新的偏移量，失败返回-1 */
int32_t sys_lseek(int32_t fd, int32_t offset, uint8_t whence) {
    if (fd < 0) {
        k_printf("sys_leek: invalid fd:%d\n", fd);
        return -1;
    }
    if (whence < SEEK_SET || whence > SEEK_END) {
        k_printf("sys_leek: invalid whence:%d\n", whence);
        return -1;
    }
    int32_t g_fd = fd_local2global(fd);
    struct file *pf = &file_table[g_fd];
    uint32_t f_size = pf->fd_inode->i_size;

    uint32_t new_pos;
    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = pf->fd_pos + offset;
            break;
        case SEEK_END:
            new_pos = f_size + offset;
            break;
    }

    if (new_pos < 0 || new_pos >= f_size) {
        k_printf("sys_lseek: position out of bounds\n");
        return -1;
    }

    pf->fd_pos = new_pos;
    return pf->fd_pos;
}

/* 删除文件(非目录), 成功返回0， 失败返回-1 */
int32_t sys_unlink(const char *pathname) {
    ASSERT(strlen(pathname) < MAX_PATH_LEN);

    struct path_search_record record;
    int32_t inode_id = search_file(pathname, &record);
    if (inode_id == -1) {
        k_printf("can not reach path: %s\n", pathname);
        dir_close(record.parent_dir);
        return -1;
    }

    if (record.file_type == FT_DIRECTORY) {
        k_printf("can't unlink a directory by sys_unlink(), use rmdir() instead!\n");
        dir_close(record.parent_dir);
        return -1;
    }

    for (int i = 0; i < MAX_FILE_OPEN; i++) {
        if (file_table[i].fd_inode->i_id == inode_id) {
            k_printf("can't unlink a file that is in use!\n");
            dir_close(record.parent_dir);
            return -1;
        }
    }

    void *io_buf = sys_malloc(2 * SECTOR_SIZE);
    if (io_buf == NULL) {
        k_printf("ERROR sys_unlink: alloc memory failed!!!\n");
        dir_close(record.parent_dir);
        return -1;
    }

    dir_delete_entry(cur_part, record.parent_dir, inode_id, io_buf);
    inode_delete(cur_part, inode_id);

    dir_close(record.parent_dir);
    sys_free(io_buf);
    return 0;
}

void filesys_init() {
    struct ide_channel_st *channel;
    struct disk_st *hd;
    struct partition_st *part;
    struct super_block_st *sb_buf = sys_malloc(sizeof(struct super_block_st));
    if (sb_buf == NULL) {
        PANIC("memory alloc failed!!!\n");
    }

    int i, j, k;
    for (i = 0; i < channel_num; i++) { // 枚举通道
        channel = &channels[i];
        for (j = 0; j < 2; j++) { // 枚举硬盘，一个通道2块硬盘
            if (j == 0)
                continue; // 跳过内核硬盘hd60M.img

            hd = &channel->devices[j];

            for (k = 0; k < 4; k++) { // 枚举主分区，最多4个
                part = &hd->prim_parts[k];
                if (part->sec_num != 0) { // 如果分区存在
                    partition_init(hd, part, sb_buf);
                }
            }

            for (k = 0; k < 8; k++) { // 枚举逻辑分区, 最多支持8个
                part = &hd->logic_parts[k];
                if (part->sec_num != 0) {
                    partition_init(hd, part, sb_buf);
                }
            }
        }
    }
    sys_free(sb_buf);

    // 确定默认分区
    char default_part_name[8] = "sdb1";
    // 挂载分区
    list_traversal(&partition_list, mount_partition, (int)default_part_name);

    // 打开当前分区的根目录
    open_root_dir(cur_part);

    // 初始化文件表
    for (i = 0; i < MAX_FILE_OPEN; i++) {
        file_table[i].fd_inode = NULL;
    }
}
