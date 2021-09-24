#include "ide.h"
#include "sync.h"
#include "io.h"
#include "timer.h"
#include "stdio.h"
#include "stdio-kernel.h"
#include "interrupt.h"
#include "memory.h"
#include "debug.h"
#include "string.h"
#include "global.h"
#include "stddef.h"

/* 定义硬盘各寄存器的端口号 */
#define reg_data(channel)	 (channel->port_base + 0)
#define reg_error(channel)	 (channel->port_base + 1)
#define reg_sec_num(channel)	 (channel->port_base + 2)
#define reg_lba_l(channel)	 (channel->port_base + 3)
#define reg_lba_m(channel)	 (channel->port_base + 4)
#define reg_lba_h(channel)	 (channel->port_base + 5)
#define reg_dev(channel)	 (channel->port_base + 6)
#define reg_status(channel)	 (channel->port_base + 7)
#define reg_cmd(channel)	 (reg_status(channel))
#define reg_alt_status(channel)  (channel->port_base + 0x206)
#define reg_ctl(channel)	 reg_alt_status(channel)

/* reg_alt_status寄存器的一些关键位 */
#define BIT_STAT_BSY	 0x80	      // 硬盘忙
#define BIT_STAT_DRDY	 0x40	      // 驱动器准备好	 
#define BIT_STAT_DRQ	 0x8	      // 数据传输准备好了

/* device寄存器的一些关键位 */
#define BIT_DEV_MBS	0xa0	    // 第7位和第5位固定为1
#define BIT_DEV_LBA	0x40
#define BIT_DEV_DEV	0x10

/* 一些硬盘操作的指令 */
#define CMD_IDENTIFY	   0xec	    // identify指令
#define CMD_READ_SECTOR	   0x20     // 读扇区指令
#define CMD_WRITE_SECTOR   0x30	    // 写扇区指令

/* 定义可读写的最大扇区数,调试用的 */
#define max_lba ((80*1024*1024/512) - 1)	// 只支持80MB硬盘

uint8_t channel_num;
struct ide_channel_st channels[2];

struct list_st partition_list; // 分区链表

int32_t ext_lba_base = 0; // 总扩展分区的起始lba, 初始为0

uint8_t p_idx = 0, l_idx = 0; // 主分区和逻辑分区的下标

struct partition_st_table_entry {
    uint8_t bootable;   // 是否可引导
    uint8_t start_head; // 起始磁头号
    uint8_t start_sec;  // 起始扇区号
    uint8_t start_chs;  // 起始柱面号
    uint8_t fs_type;    // 分区类型
    uint8_t end_head;   // 结束磁头号
    uint8_t end_sec;    // 结束扇区号
    uint8_t end_chs;    // 结束柱面号
    uint32_t start_lba; // 本分区起始扇区的lba地址
    uint32_t sec_num;   // 本分区的扇区数量
} __attribute__ ((packed)); // 保证此结构体占16字节大小

/* 引导扇区 MBR所在的扇区 */
struct boot_sector {
    uint8_t boot[446];  // 引导代码
    struct partition_st_table_entry partition_table[4];
    uint16_t signature; // 启动扇区的结束标志是0x55, 0xaa
} __attribute__ ((packed));

/* 向寄存器指定操作的硬盘 */
static void select_disk(struct disk_st *hd) {
    uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA;
    if (hd->dev_no == 1) { // 从盘
        reg_device |= BIT_DEV_DEV;
    }
    outb(reg_dev(hd->my_channel), reg_device);
}

/* 向寄存器指定起始扇区地址及待操作的扇区数量 */
static void select_sector(struct disk_st *hd, uint32_t lba, uint8_t sec_num) {
    ASSERT(lba <= max_lba);
    struct ide_channel_st *channel = hd->my_channel;
    /* 写入channel相应的寄存器 */
    outb(reg_sec_num(channel), sec_num); // 指定扇区数量, 若为0，则表示256个
    outb(reg_lba_l(channel), lba); // 指定lba低8位
    outb(reg_lba_m(channel), lba >> 8); // 指定lba 8~15位
    outb(reg_lba_h(channel), lba >> 16); // 指定lba 16~23位
    outb(reg_dev(channel), \ 
        BIT_DEV_MBS | BIT_DEV_LBA | (hd->dev_no == 1 ? BIT_DEV_DEV : 0) \
        | lba >> 24); // lba的24~27位
}

/* 向通道channel发cmd命令 */
static void cmd_out(struct ide_channel_st *channel, uint8_t cmd) {
    channel->expecting_intr = true;
    outb(reg_cmd(channel), cmd);
}

/* 从硬盘读入sec_num个扇区的内容到dest
    最多读入256个，sec_num作为uint8_t类型最大255，sec_num为0时表示读入256个扇区 */
static void read_from_sector(struct disk_st *hd, void *dest, uint8_t sec_num) {
    uint32_t size_in_byte;
    if (sec_num == 0) {
        // 0表示256个
        size_in_byte = 256 * 512;
    }
    else {
        size_in_byte = sec_num * 512;
    }
    insw(reg_data(hd->my_channel), dest, size_in_byte / 2);
}

/* 将src处sec_num个扇区大小的内容写入硬盘 */
static void write2sector(struct disk_st *hd, void *src, uint8_t sec_num) {
    uint32_t size_in_byte;
    if (sec_num == 0) {
        size_in_byte = 256 * 512;
    }
    else {
        size_in_byte = sec_num * 512;
    }
    outsw(reg_data(hd->my_channel), src, size_in_byte / 2);
}

/* 等待硬盘准备 最多等待30秒 */
static bool busy_wait(struct disk_st *hd) {
    struct ide_channel_st *channel = hd->my_channel;
    uint32_t time_limit = 30 * 1000;
    while (time_limit) {
        time_limit -= 10;
        if (! (inb(reg_status(channel)) & BIT_STAT_BSY)) { // 硬盘准备好了
            return (inb(reg_status(channel)) & BIT_STAT_DRQ);
        }
        else {
            sys_sleep(10);
        }
    }
    return false;
}

/* 从硬盘hd的lba扇区读取sec_num个扇区的内容至内存地址dest处 */
void ide_read(struct disk_st *hd, uint32_t lba, uint32_t sec_num, void *dest) {
    ASSERT(lba < max_lba);
    ASSERT(sec_num > 0);
    lock_acquire(&hd->my_channel->lock);

    // 指定硬盘
    select_disk(hd);

    // 每次最多读取256个扇区
    while (sec_num) {
        // 本次读取的扇区数量
        uint32_t sec_num_this_time = sec_num < 256 ? sec_num : 256;
        sec_num -= sec_num_this_time;

        // 指定起始扇区及扇区数量
        select_sector(hd, lba, (uint8_t)sec_num_this_time);
        lba += sec_num_this_time;
        // 发送命令
        cmd_out(hd->my_channel, CMD_READ_SECTOR);
        // 阻塞，等待被硬盘中断唤醒
//        semaphore_down(&hd->my_channel->disk_done);

        // 等待硬盘准备
        if (busy_wait(hd)) {
            read_from_sector(hd, dest, (uint8_t)sec_num_this_time);
            dest = (void *)((uint32_t)dest + sec_num_this_time * 512); // 一个扇区512字节
        }
        else {
            char error[64];
            sprintf(error, "%s read sector #%d failed!!!\n", hd->name, lba);
            PANIC(error);
        }
    }

    lock_release(&hd->my_channel->lock);
}

/* 将内存地址src处起始的sec_num扇区内容写入硬盘hd的lba扇区处 */
void ide_write(struct disk_st *hd, uint32_t lba, uint32_t sec_num, void *src) {
    ASSERT(lba < max_lba);
    ASSERT(sec_num > 0);
    lock_acquire(&hd->my_channel->lock);

    // 指定硬盘
    select_disk(hd);

    // 分次写入，每次最多写入256个扇区
    while (sec_num) {
        // 本次写入的扇区数量
        uint32_t sec_num_this_time = sec_num < 256 ? sec_num : 256;
        sec_num -= sec_num_this_time;

        // 指定起始扇区及扇区数量
        select_sector(hd, lba, (uint8_t)sec_num_this_time); // 0表示256
        lba += sec_num_this_time;

        // 发送命令
        cmd_out(hd->my_channel, CMD_WRITE_SECTOR);

        // 等待硬盘准备
        if (busy_wait(hd)) {
            // 写入
            write2sector(hd, src, (uint8_t)sec_num_this_time);
            src = (void *)((uint32_t)src + sec_num_this_time * 512);
        }
        else {
            char error[64];
            sprintf(error, "%s write sector #%d failed!!!\n", hd->name, lba);
            PANIC(error);
        }

        // 阻塞
 //       semaphore_down(&hd->my_channel->disk_done);
    }

    lock_release(&hd->my_channel->lock);
}

/* 将src处len个字节 相邻两两交换后 存入dst */
static void swap_pairs_bytes_cpy(const char *src, char *dst, uint32_t len) {
    int i;
    for (i = 0; i < len; i+= 2) {
        dst[i + 1] = *src++;
        dst[i] = *src++;
    }
    dst[i] = '\0';
}

/* 获取硬盘参数信息 */
static void identify_disk(struct disk_st *hd) {
    select_disk(hd);
    cmd_out(hd->my_channel, CMD_IDENTIFY);
//    semaphore_down(&hd->my_channel->disk_done);

    if (busy_wait(hd)) {
        char id_info[512];
        read_from_sector(hd, id_info, 1);
        char buf[64];
        uint8_t sn_start = 10 * 2, sn_len = 20, md_start = 27 * 2, md_len = 40;
        swap_pairs_bytes_cpy(&id_info[sn_start], buf, sn_len);
        k_printf("  disk %s info:\n     SN: %s\n", hd->name, buf);
        swap_pairs_bytes_cpy(&id_info[md_start], buf, md_len);
        k_printf("      MODULE: %s\n", buf);
        uint32_t sectors = *(uint32_t *)&id_info[60 * 2];
        k_printf("      SECTORS: %d\n", sectors);
        k_printf("      CAPACITY: %dMB\n", sectors * 512 / 1024 / 1024);
    }
    else {
        char error[64];
        sprintf(error, "%s identify failed!!!\n", hd->name);
        PANIC(error);
    }
}

/* 扫描硬盘hd中地址为ext_lba的扇区中的所有分区 */
static void partition_scan(struct disk_st *hd, uint32_t ext_lba) {
    struct boot_sector *bs = sys_malloc(sizeof(struct boot_sector));
    ide_read(hd, ext_lba, 1, bs);
    struct partition_st_table_entry *p = bs->partition_table;

    for (int i = 0; i < 4; i++) {
        switch (p->fs_type) {
            case 0x5:   // 扩展分区
                // 递归
                if (ext_lba_base) {
                    partition_scan(hd, p->start_lba + ext_lba_base);
                }
                else {
                    ext_lba_base = p->start_lba;
                    partition_scan(hd, p->start_lba);
                }
                break;
            case 0x0:   // 无效的分区类型
                break;
            default:
                // 有效的分区类型
                if (ext_lba == 0) {
                    hd->prim_parts[p_idx].start_lba = ext_lba + p->start_lba;
                    hd->prim_parts[p_idx].sec_num = p->sec_num;
                    hd->prim_parts[p_idx].my_disk = hd;
                    list_push_back(&partition_list, &hd->prim_parts[p_idx].part_tag);
                    sprintf(hd->prim_parts[p_idx].name, "%s%d", hd->name, p_idx + 1); // 主分区从1开始
                    p_idx++;
                    ASSERT(p_idx < 4);
                }
                else {
                    hd->logic_parts[l_idx].start_lba = ext_lba + p->start_lba;
                    hd->logic_parts[l_idx].sec_num = p->sec_num;
                    hd->logic_parts[l_idx].my_disk = hd;
                    list_push_back(&partition_list, &hd->logic_parts[l_idx].part_tag);
                    sprintf(hd->logic_parts[l_idx].name, "%s%d", hd->name, l_idx + 5); // 逻辑分区从5开始
                    l_idx++;
                    if (l_idx >= 8)
                        return;
                }
                break;
        } //switch
        p++;
    }
    sys_free(bs);
}

/* 打印分区信息 */
static bool partition_info(list_node node, int arg) {
    struct partition_st *part = elem2entry(struct partition_st, part_tag, node);
    k_printf("  %s start_lba: 0x%x, sec_num:0x%x\n", part->name, part->start_lba, part->sec_num);
    return false;
}

/* 硬盘中断处理函数 */
void intr_hd_handler(uint8_t irq_no) {
    ASSERT(irq_no == 0x2e || irq_no == 0x2f);
    uint8_t channel_index = irq_no - 0x2e;
    struct ide_channel_st *channel = &channels[channel_index];
    ASSERT(channel->irq_no == irq_no);
    if (channel->expecting_intr) {
        channel->expecting_intr = false;
        semaphore_up(&channel->disk_done);
        inb(reg_status(channel));
    }
}

/* 硬盘数据结构初始化 */
void ide_init() {
    k_printf("ide_init start\n");
    uint8_t hd_num = *((uint8_t *)0x475); // 硬盘数量 \
        低端1MB内存的虚拟地址和物理地址相同，可以直接访问到物理地址0x475，\
        0x475被BIOS指定存储硬盘数量
    list_init(&partition_list);
    ASSERT(hd_num > 0);
    channel_num = DIV_ROUND_UP(hd_num, 2); // 一个通道2块硬盘
    struct ide_channel_st *channel;
    for (int i = 0; i < channel_num; i++) {
        channel = &channels[i];
        sprintf(channel->name, "ide%d", i);
        switch (i) {
            case 0:
                channel->port_base = 0x1f0;
                channel->irq_no = 0x20 + 14;
                break;
            case 1:
                channel->port_base = 0x170;
                channel->irq_no = 0x20 + 15;
                break;
            default:
                break;
        }
        channel->expecting_intr = false;
        lock_init(&channel->lock);
        semaphore_init(&channel->disk_done, 0);

        register_handler(channel->irq_no, intr_hd_handler); // 注册中断处理程序

        // 获取两个硬盘的参数及分区信息
        for (int j = 0; j < 2; j++) {
            struct disk_st *hd = &channel->devices[j];
            memset(hd, 0, sizeof(struct disk_st));
            hd->my_channel = channel;
            hd->dev_no = j;
            sprintf(hd->name, "sd%c", 'a' + i * 2 + j); // 第i通道的第j硬盘
            identify_disk(hd);
            if (j) { // j == 0则表示是内核所在的硬盘，这里就不处理了:(
                // 扫描硬盘上的分区
                partition_scan(hd, 0);
            }
            p_idx = 0, l_idx = 0; // 刷新主分区及逻辑分区的下标，下次其他硬盘扫描分区时重新从0开始计数
        }
    }
    k_printf("\n    all partition info:\n");
    // 遍历所有分区并打印信息
    list_traversal(&partition_list, partition_info, (int)NULL);
    k_printf("ide_init finished\n");
}
