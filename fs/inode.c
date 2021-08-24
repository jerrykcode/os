#include "inode.h"
#include "stddef.h"
#include "fs.h"
#include "ide.h"
#include "global.h"
#include "list.h"
#include "thread.h"
#include "debug.h"
#include "interrupt.h"

/* 描述inode位置 */
struct inode_position {
    bool span_two_sectors; // inode是否跨越两个扇区
    uint32_t sec_lba; // inode所在的扇区号
    uint32_t off_size; // inode在扇区内的字节偏移量
};

/* 获取inode的位置 */
static void get_inode_position(struct partition_st *part, uint32_t inode_id, struct inode_position *inode_pos) {
    ASSERT(part != NULL);
    ASSERT(inode_id < 4096);
    ASSERT(inode_pos != NULL);
    uint32_t inode_st_size = sizeof(struct inode_st);
    uint32_t inode_off = inode_id * inode_st_size; // 相对于inode数组inode_table的偏移量
    uint32_t inode_off_sec = inode_off / SECTOR_SIZE;
    uint32_t inode_off_in_sec = inode_off % SECTOR_SIZE;
    if (SECTOR_SIZE - inode_off_in_sec < inode_st_size) { // 扇区内剩余的空间存储不下一个inode结构
        inode_pos->span_two_sectors = true;
    }
    else {
        inode_pos->span_two_sectors = false;
    }

    inode_pos->sec_lba = part->sb->inode_table_lba + inode_off_sec;
    inode_pos->off_size = inode_off_in_sec;
}

/* 将inode写入到分区part */
void inode_sync(struct partition_st *part, const struct inode_st *inode, void *io_buf) {
    uint32_t inode_id = inode->i_id;
    struct inode_position inode_pos;
    get_inode_position(part, inode_id, &inode_pos);

    // 硬盘存储的inode不需要i_open_cnts, write_deny与inode_tag属性
    struct inode_st pure_inode;
    memcpy(&pure_inode, inode, sizeof(struct inode_st));
    pure_inode.i_open_cnts = 0;
    pure_inode.write_deny = false;
    pure_inode.inode_tag.pre = pure_inode.inode_tag.next = NULL;

    // 将pure_inode写入硬盘
    // 先从硬盘读出分区，将分区中关于inode的部分修改为pure_inode之后再写回硬盘
    // 若inode跨越分区，则需要读写两个分区
    struct disk_st *hd = part->my_disk;
    uint32_t sec_num = inode_pos->span_two_sectors ? 2 : 1;
    // 读取扇区
    ide_read(hd, inode_pos->sec_lba, sec_num, io_buf);
    // 修改扇区内容中inode_id对应的inode
    memcpy(io_buf + inode_pos->off_size, &pure_inode, sizeof(struct inode_st));
    // 将扇区写回硬盘
    ide_write(hd, inode_pos->sec_lba, sec_num, io_buf);
}

/* list_traversal的回调函数 寻找分区打开的inode中特定inode号的inode */

// 回调函数参数
struct inode_search_arg {
    uint32_t inode_id;
    struct inode_st *inode_found;
};

// 回调函数
static bool inode_search(list_node node, int arg) {
    struct inode_search_arg *m_arg = (struct inode_search_arg *)arg;
    struct inode_st *inode = elem2entry(struct inode_st, inode_tag, node);
    if (inode->i_id == m_arg->inode_id) {
        m_arg->inode_found = inode;
        return true;
    }
    return false;
}

/* 返回分区part中inode号为inode_id的inode */
struct inode_st *inode_open(struct partition_st *part, uint32_t inode_id) {
    struct inode_search_arg arg;
    arg.inode_id = inode_id;
    arg.inode_found = NULL;
    // 遍历打开的inode
    list_traveral(&part->open_inodes, inode_search, (int)&arg);
    if (arg.inode_found != NULL) {// 找到了
        arg.inode_found->i_open_cnts++;
        return arg.inode_found;
    }

    /* 打开的inode中没有待寻找的inode, 说明这个inode没有被打开过, 需要现在打开 */
    
    // 硬盘定位
    struct inode_position inode_pos;
    get_inode_position(part, inode_id, &inode_pos);

    // 为inode分配内存
    // 由于inode需要使用内核内存，而如果当前执行的是用户进程，sys_malloc就会分配用户内存
    // 所以需要将当前PCB中的页表临时置为NULL, 以使sys_malloc分配内核内存，之后再还原页表
    struct task_st *cur = current_thread();
    uint32_t *page_table_backup = cur->page_table;
    cur->page_table = NULL;

    struct inode_st *inode_found = (struct inode_st *)sys_malloc(sizeof(struct inode_st));

    cur->page_table = page_table_backup;
    if (inode_found == NULL)
        PANIC("alloc memory failed!!!");

    // 从硬盘读取inode
    uint32_t sec_num = inode_pos.span_two_sectors ? 2 : 1;
    void *io_buf = sys_malloc(sec_num * SECTOR_SIZE); // 分配IO读入缓存
    if (io_buf == NULL) {
        sys_free(inode_found);
        PANIC("alloc memory failed!!!");
    }

    struct disk_st *hd = part->my_disk;
    ide_read(hd, inode_pos.sec_lba, sec_num, io_buf);

    // 复制到inode_found
    memcpy(inode_found, io_buf + inode_pos.off_size, sizeof(struct inode_st));
    sys_free(io_buf);

    inode_found->i_open_cnts = 1;

    // 加入打开的inode链表
    list_push_back(&part->open_inodes, inode_found->inode_tag);

    return inode_found;
}

/* 关闭inode或减少inode打开次数 */
void inode_close(struct partition_st *part, struct inode_st *inode) {
    enum intr_status old_status = intr_disable();
    if (--inode->i_open_cnt == 0) {
        list_remove(&part->open_inodes, inode);
        // inode使用内核内存
        // 释放内存时，需要将页表临时设置为NULL
        // 完成后再复原页表
        struct task_st *cur = current_thread();
        uint32_t *page_table_backup = cur->page_table;
        cur->page_table = NULL;

        sys_free(inode);

        cur->page_table = page_table_backup;
    }
    intr_set_status(old_status);
}

/* 初始化new_inode */
void inode_init(uint32_t inode_id, struct inode_st *new_inode) {
    new_inode->i_id = inode_id;
    new_inode->i_size = 0;
    new_inode->i_open_cnts = 0;
    new_inode->write_deny = false;

    // 初始化i_sectors
    for (int i = 0; i < 13; i++) {
        new_inode->i_sectors[i] = 0;
    }
}
