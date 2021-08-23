#include "fs.h"
#include "super_block.h"
#include "inode.h"
#include "dir.h"
#include "ide.h"
#include "memory.h"
#include "string.h"
#include "stdio-kernel.h"
#include "global.h"
#include "bitmap.h"
#include "debug.h"
#include "list.h"

#define SUPER_BLOCK_MAGIC_FILE_SYS  0x20210819

struct partition_st *cur_part; // 默认情况下操作的分区

extern struct list_st partition_list;

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
}
