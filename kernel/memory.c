#include "memory.h"
#include "sync.h"
#include "print.h"
#include "string.h"
#include "stddef.h"
#include "debug.h"
#include "thread.h"
#include "bitmap.h"
#include "list.h"
#include "global.h"
#include "asm.h"

#define MEM_BITMAP_BASE 0xc009a000

#define K_HEAP_START    0xc0100000

struct pool {
    struct lock_st lock;
    struct bitmap pool_btmp;
    uint32_t phy_addr_start;
    uint32_t pool_size;
};

struct pool kernel_pool, user_pool;
struct virtual_addr kernel_vaddr;

// 堆内存相关
struct arena {
    struct mem_block_desc *desc;
    uint32_t cnt;
    bool is_large;
};

// 所有内核线程共用这个mem_block_desc数组
// 每个用户进程有自己的mem_block_desc数组
struct mem_block_desc kernel_mem_block_descs[MEM_BLOCK_DESC_NUM];
//

static void mem_pool_init(uint32_t all_mem) {
    put_str("   mem_pool_init start\n");
    uint32_t used_mem = 256 * PAGE_SIZE + 0x100000; // 页表及低端1M内存
    uint32_t free_mem = all_mem - used_mem;
    uint32_t free_mem_page_num = free_mem / PAGE_SIZE;
    uint32_t kernel_page_num = free_mem_page_num / 2;
    uint32_t user_page_num = free_mem_page_num - kernel_page_num;

    put_str("       all_mem: 0x"); put_int_hex(all_mem); put_str("  free_mem: 0x"); put_int_hex(free_mem); put_char('\n');

    // 初始化锁
    lock_init(&kernel_pool.lock);
    lock_init(&user_pool.lock);

    // kernel物理内存池
    uint32_t kernel_phy_addr_start = used_mem;
    kernel_pool.phy_addr_start = kernel_phy_addr_start;
    kernel_pool.pool_size = kernel_page_num * PAGE_SIZE;
    // kernel物理内存池位图
    kernel_pool.pool_btmp.btmp_bytes_len = kernel_page_num / 8;
    kernel_pool.pool_btmp.bits = (void *)MEM_BITMAP_BASE;
    bitmap_init(&kernel_pool.pool_btmp, BTMP_MEM_FREE);
    // 输出信息
    put_str("       {\n");
    put_str("           kernel_pool_bitmap_start: 0x"); put_int_hex((int)kernel_pool.pool_btmp.bits); put_char('\n');
    put_str("           kernel_pool_bitmap_bytes_len: 0x"); put_int_hex((int)kernel_pool.pool_btmp.btmp_bytes_len); put_char('\n');
    put_str("           kernel_pool_phy_addr_start: 0x"); put_int_hex((int)kernel_pool.phy_addr_start); put_char('\n');
    put_str("           kernel_pool_size: 0x"); put_int_hex((int)kernel_pool.pool_size); put_char('\n');
    put_str("       }\n");

    // user物理内存池, 紧跟在kernel物理内存池之后
    uint32_t user_phy_addr_start = kernel_phy_addr_start + kernel_page_num * PAGE_SIZE;
    user_pool.phy_addr_start = user_phy_addr_start;
    user_pool.pool_size = user_page_num * PAGE_SIZE;
    // user物理内存池位图, 紧跟在kernel物理内存池位图之后
    user_pool.pool_btmp.btmp_bytes_len = user_page_num / 8;
    user_pool.pool_btmp.bits = kernel_pool.pool_btmp.bits + kernel_pool.pool_btmp.btmp_bytes_len;
    bitmap_init(&user_pool.pool_btmp, BTMP_MEM_FREE);
    // 输出信息
    put_str("       {\n");
    put_str("           user_pool_bitmap_start: 0x"); put_int_hex((int)user_pool.pool_btmp.bits); put_char('\n');
    put_str("           user_pool_bitmap_bytes_len: 0x"); put_int_hex((int)user_pool.pool_btmp.btmp_bytes_len); put_char('\n');
    put_str("           user_pool_phy_addr_start: 0x"); put_int_hex((int)user_pool.phy_addr_start); put_char('\n');
    put_str("           user_pool_size: 0x"); put_int_hex((int)user_pool.pool_size); put_char('\n');
    put_str("       }\n");

    // kernel虚拟地址
    kernel_vaddr.vaddr_start = K_HEAP_START;
    // kernel虚拟地址位图, 用于维护内核堆的虚拟地址, 和内核内存池大小一致; 紧跟在user物理内存池位图之后
    kernel_vaddr.vaddr_btmp.btmp_bytes_len = kernel_pool.pool_btmp.btmp_bytes_len;
    kernel_vaddr.vaddr_btmp.bits = user_pool.pool_btmp.bits + user_pool.pool_btmp.btmp_bytes_len;
    bitmap_init(&kernel_vaddr.vaddr_btmp, BTMP_MEM_FREE);

    put_str("   mem_pool_init finished\n");
}

void mem_block_desc_init(struct mem_block_desc desc_arr[]) {
    uint32_t block_size = 16;
    uint32_t arena_free_mem_size = PAGE_SIZE - sizeof(struct arena); // 一个arena占一页，减去头部arena结构体剩余的是空闲内存
    for (int i = 0; i < MEM_BLOCK_DESC_NUM; i++) {
        desc_arr[i].block_size = block_size;
        desc_arr[i].blocks_per_arena = arena_free_mem_size / block_size;
        list_init(&desc_arr[i].free_mem_list);
        lock_init(&desc_arr[i].list_lock);
        block_size <<= 1; // *2
    }
}

void mem_init() {
    put_str("mem_init start\n");
    uint32_t all_mem = (*(uint32_t *)(0xb00));
    mem_pool_init(all_mem);
    mem_block_desc_init(kernel_mem_block_descs);
    put_str("mem_init finished\n");
}

/* 申请page_num个页的虚拟空间，成功返回虚拟地址，失败返回NULL */
static void *vaddr_alloc(enum pool_flags pf, uint32_t page_num) {
    uint32_t vaddr_start = 0;
    if (pf == PF_KERNEL) {
        // 内核线程申请内存
        // 通过位图查询空闲页
        int bit_idx = bitmap_alloc(&kernel_vaddr.vaddr_btmp, page_num, BTMP_MEM_FREE);
        if (bit_idx == -1)
            return NULL;
        for (uint32_t i = 0; i < page_num; i++) {
            // 更新位图
            bitmap_setbit(&kernel_vaddr.vaddr_btmp, bit_idx + i, BTMP_MEM_USED);
        }
        vaddr_start = kernel_vaddr.vaddr_start + bit_idx * PAGE_SIZE;
    }
    else {
        // 用户进程申请内存
        struct task_st *cur = current_thread();
        int bit_idx = bitmap_alloc(&cur->usrprog_vaddr.vaddr_btmp, page_num, BTMP_MEM_FREE);
        if (bit_idx == -1)
            return NULL;
        for (uint32_t i = 0; i < page_num; i++)
            bitmap_setbit(&cur->usrprog_vaddr.vaddr_btmp, bit_idx + i, BTMP_MEM_USED);
        vaddr_start = cur->usrprog_vaddr.vaddr_start + bit_idx * PAGE_SIZE;
    }
    return (void *)vaddr_start;
}

/* 从m_pool指定的内存池中申请一页的物理内存 */
static void *palloc(struct pool *m_pool) {
    int bit_idx = bitmap_alloc(&m_pool->pool_btmp, 1, BTMP_MEM_FREE);
    if (bit_idx == -1)
        return NULL;
    bitmap_setbit(&m_pool->pool_btmp, bit_idx, BTMP_MEM_USED);
    return (void *)(m_pool->phy_addr_start + bit_idx * PAGE_SIZE);
}

/* 在页表中添加虚拟地址与物理页的映射，使虚拟地址vaddr经过分页后得到物理页的物理地址page_phyaddr */
static void vaddr_page_map(void *vaddr, void *page_phyaddr) {
    uint32_t vaddr_val = (uint32_t)vaddr; // vaddr的值
    uint32_t page_phyaddr_val = (uint32_t)page_phyaddr; // page_phyaddr的值
    uint32_t *vaddr_pte = pte_ptr(vaddr_val), *vaddr_pde = pde_ptr(vaddr_val);
    if (*vaddr_pde & 0x00000001) {  // 页目录项和页表项的第0位为P，此处判断页目录项是否存在
        ASSERT(!(*vaddr_pte & 0x00000001));
        if (!(*vaddr_pte & 0x00000001)) {
            // 将页的物理地址写入页表项，注意页表项只存储页物理地址的高20位，这是因为页的物理地址一定是4096的倍数
            // 页的物理地址的低12位一定为0。页表项的高20位存储页物理地址的高20位，页表项第12位存储其他属性
            // US = 1, RW = 1, P = 1
            *vaddr_pte = (page_phyaddr_val | PG_US_U | PG_RW_W | PG_P_1);
        }
        else {
            // 正常不会执行到这里
            PANIC("pte repeat!!!");
            *vaddr_pte = (page_phyaddr_val | PG_US_U | PG_RW_W | PG_P_1);
        }
    }
    else {
        // 页表不存在，需要先创建页表并赋值给页目录项
        *vaddr_pde = ((uint32_t)palloc(&kernel_pool) | PG_US_U | PG_RW_W | PG_P_1); 
        memset((void *)((int)vaddr_pte & 0xfffff000), 0, PAGE_SIZE); // 将新创建的页表空间初始化为0
        ASSERT(!(*vaddr_pte & 0x00000001));
        *vaddr_pte = (page_phyaddr_val | PG_US_U | PG_RW_W | PG_P_1);
    }
}

void *malloc_kernel_page(uint32_t pages_num) {
    lock_acquire(&kernel_pool.lock);
    void *vaddr = malloc_page(PF_KERNEL, pages_num);
    lock_release(&kernel_pool.lock);
    if (vaddr != NULL) {
        memset(vaddr, 0, pages_num * PAGE_SIZE);
    }
    return vaddr;
}

void *malloc_user_page(uint32_t pages_num) {
    lock_acquire(&user_pool.lock);
    void *vaddr = malloc_page(PF_USER,pages_num);    
    lock_release(&user_pool.lock);
    if (vaddr != NULL) {
        memset(vaddr, 0, pages_num * PAGE_SIZE);
    }
    return vaddr;
}

void *malloc_page(enum pool_flags pf, uint32_t pages_num) {
    // 申请虚拟地址
    void *vaddr_start = vaddr_alloc(pf, pages_num);
    if (vaddr_start == NULL)
        return NULL;
    // 为每一页申请物理地址
    // 虚拟地址是连续的，而每一页的物理地址可能是不连续的
    // 对每一页物理地址做映射
    for (int i = 0; i < pages_num; i++) {        
        struct pool *m_pool = (pf == PF_KERNEL ? &kernel_pool : &user_pool);
        // 申请一页内存，得到这一页的物理地址
        void *page_phyaddr = palloc(m_pool);
        if (page_phyaddr == NULL) {
            // 失败，将虚拟地址和已经成功申请的物理页释放
            return NULL;
        }
        // 映射
        vaddr_page_map(vaddr_start + i * PAGE_SIZE, page_phyaddr);
    }
    return vaddr_start;
}

/* 申请一页物理内存，并将其虚拟地址映射为vaddr
   返回 (void *)vaddr
   本函数需要调用者确保vaddr没有被使用 */
void *malloc_page_with_vaddr(enum pool_flags pf, uint32_t vaddr) {
    struct pool *m_pool = (pf == PF_KERNEL ? &kernel_pool : &user_pool);

    lock_acquire(&m_pool->lock);

    void *page_phyaddr = palloc(m_pool); // 申请一页物理内存
    if (page_phyaddr == NULL) {
        vaddr = NULL;
    }
    else {
        // 获取虚拟地址位图 以及 vaddr在位图中对应的位
        struct bitmap *m_btmp;
        uint32_t bit_idx;
        if (pf == PF_KERNEL) {
            m_btmp = (struct bitmap *)&kernel_vaddr.vaddr_btmp;
            bit_idx = (vaddr - kernel_vaddr.vaddr_start) / PAGE_SIZE;
        }
        else {
            struct virtual_addr *m_usrprog_vaddr = &current_thread()->usrprog_vaddr;
            m_btmp = (struct bitmap *)&m_usrprog_vaddr->vaddr_btmp;
            bit_idx = (vaddr - m_usrprog_vaddr->vaddr_start) / PAGE_SIZE;
        }
        // 将虚拟地址位图中 vaddr 对应的位置1
        bitmap_setbit(m_btmp, bit_idx, BTMP_MEM_USED);

        // 添加物理地址与虚拟地址的映射
        vaddr_page_map((void *)vaddr, page_phyaddr);
    }

    lock_release(&m_pool->lock);

    return (void *)vaddr;
}

/* 返回虚拟地址对应的物理地址所在页的页表项的地址 */
uint32_t *pte_ptr(uint32_t vaddr) {
    uint32_t pte = 0xffc00000; // 高10位置1，表示第1023个页目录项，指向页目录本身
    pte |= vaddr >> 10; // [10, 20)位存储vaddr高10位的值，[20, 30)位存储vaddr[10, 20)位的值
    pte &= 0xfffffffc;  // [30, 32)位置0，也就是[20, 32)位存储vaddr[10, 20)位的值乘4(左移2位)
    return (uint32_t *)pte;
}

/* 返回虚拟地址对应的物理地址所在的页所在的页表的页目录项的地址 */
uint32_t *pde_ptr(uint32_t vaddr) {
    uint32_t pde = 0xfffff000; //高20位置1
    pde |= vaddr >> 20; // [20, 30)位存储vaddr高10位的值
    pde &= 0xfffffffc;  // [30, 32)位置0
    return (uint32_t *)pde;
}

/* 返回虚拟地址对应的物理地址 */
uint32_t vaddr2phy(uint32_t vaddr) {
    uint32_t *pte = pte_ptr(vaddr);
    return ((*pte) & 0xfffff000) + (vaddr & 0x00000fff);
}

/* 返回arena中的第i个内存块地址 */
static struct mem_block *arena2block(struct arena *a, int i) {
    return (struct mem_block *)((uint32_t)a + sizeof(struct arena) + i * a->desc->block_size);
}

/* 返回内存块所在的arena地址 */
static struct arena *block2arena(struct mem_block *block) {
    return (struct arena *)((uint32_t)block & 0xfffff000);
}

/*
申请size字节内存
*/
void *sys_malloc(uint32_t size) {
    enum pool_flags pf;
    struct task_st *cur = current_thread();
    if (cur->page_table) {
        // 用户进程
        pf = PF_USER;
    }
    else {
        // 内核线程
        pf = PF_KERNEL;
    }
    if (size > 1024) { // 大内存
        uint32_t page_num = DIV_ROUND_UP(size + sizeof(struct arena), PAGE_SIZE);
        // malloc_user_page和malloc_kernel_page函数中已加锁
        struct arena *a = (pf == PF_USER ? malloc_user_page(page_num) : malloc_kernel_page(page_num));
        a->desc = NULL;
        a->cnt = page_num; // 对于大内存，cnt表示内存页数
        a->is_large = true; // 是大内存
        return (void *)((uint32_t)a + sizeof(struct arena));
    }
    else { // 小于1024字节的内存
        struct mem_block_desc *mem_block_descs = (pf == PF_USER ? cur->usrprog_mem_block_descs : kernel_mem_block_descs);
        struct mem_block_desc *desc;
        // 搜索大小合适的内存块描述符
        for (int i = 0; i < MEM_BLOCK_DESC_NUM; i++)
            if (mem_block_descs[i].block_size >= size) {// 第i种内存块大小足够
                desc = &mem_block_descs[i];
                break;
            }
        lock_acquire(&desc->list_lock);
        if (list_empty(&desc->free_mem_list)) { // 没有空闲的内存块
            // 创建arena
            // malloc_user_page和malloc_kernel_page函数中已加锁
            struct arena *a = (pf == PF_USER ? malloc_user_page(1) : malloc_kernel_page(1));
            a->desc = desc;
            a->cnt = desc->blocks_per_arena; // 该arena含有的内存块数
            a->is_large = false;
            // 将arena拆分成若干内存块，并加入desc->free_mem_list链表
            for (int i = 0; i < desc->blocks_per_arena; i++) {
                struct mem_block *block = arena2block(a, i); // 第i个内存块地址
                list_push_back(&desc->free_mem_list, &block->node); // 加入链表
            }
        }
        // 申请内存块
        // 链表中存储的是struct mem_block的node属性地址，而node是mem_block结构体首个属性
        struct mem_block *block = list_pop(&desc->free_mem_list);
        lock_release(&desc->list_lock);
        struct arena *a = block2arena(block);
        a->cnt--;
        return (void *)block;
    }
}

// 以下是释放内存相关函数

/* 释放一页物理内存 */
static void phy_page_free(uint32_t phy_addr) {
    struct pool *m_pool;
    if (phy_addr >= user_pool.phy_addr_start)
        m_pool = &user_pool;
    else
        m_pool = &kernel_pool;
    int bit_index = (phy_addr - m_pool->phy_addr_start) / PAGE_SIZE;
    // 将相应的位置为BTMP_MEM_FRE
    bitmap_setbit(&m_pool->pool_btmp, bit_index, BTMP_MEM_FREE);
}

/* 删除页表中的虚拟地址映射 */
static void vaddr_page_map_remove(uint32_t vaddr) {
    uint32_t *pte = pte_ptr(vaddr);
    *pte &= ~PG_P_1; // pte的P位置0
    asm volatile ("invlpg %0" : : "m"(vaddr) : "memory"); // 更新TLB
}

/* 释放虚拟地址从vaddr起始的连续pages_num页虚拟内存 */
static void virtual_pages_free(struct virtual_addr *m_vaddr, uint32_t vaddr, uint32_t pages_num) {
    int bit_index = (vaddr - m_vaddr->vaddr_start) / PAGE_SIZE;
    for (int i = 0; i < pages_num; i++) {
        bitmap_setbit(&m_vaddr->vaddr_btmp, bit_index + i, BTMP_MEM_FREE);
    }
}

/* 释放虚拟地址从vaddr开始的连续pages_num页内存 */
static void pages_free(uint32_t vaddr, uint32_t pages_num) {
    ASSERT(pages_num >= 1 && (vaddr % PAGE_SIZE == 0));
    uint32_t phyaddr;
    struct task_st *cur = current_thread();
    struct virtual_addr *m_vaddr;
    struct lock_st *m_lock = NULL;
    if (cur->page_table) {
        // 用户进程
        m_vaddr = &cur->usrprog_vaddr;
        m_lock = &user_pool.lock;
        lock_acquire(m_lock);
        for (int i = 0; i < pages_num; i++) {
            phyaddr = vaddr2phy(vaddr);
            // 确保phyaddr在用户内存池内
            ASSERT((phyaddr % PAGE_SIZE == 0) && phyaddr >= user_pool.phy_addr_start);
            phy_page_free(phyaddr);
            vaddr_page_map_remove(vaddr);
            vaddr += PAGE_SIZE;
        }
    }
    else {
        // 内核线程
        m_vaddr = &kernel_vaddr;
        m_lock = &kernel_pool.lock;
        lock_acquire(m_lock);
        for (int i = 0; i < pages_num; i++) {
            phyaddr = vaddr2phy(vaddr);
            // 确保phyaddr是内核内存
            ASSERT((phyaddr % PAGE_SIZE == 0) && phyaddr >= kernel_pool.phy_addr_start \
                && phyaddr < user_pool.phy_addr_start);
            phy_page_free(phyaddr);
            vaddr_page_map_remove(vaddr);
            vaddr += PAGE_SIZE;
        }
    }
    virtual_pages_free(m_vaddr, vaddr, pages_num);
    ASSERT(m_lock != NULL);
    lock_release(m_lock);
}

void sys_free(void *ptr) {
    ASSERT(ptr != NULL);
    if (ptr != NULL) {
        struct mem_block *m_block = (struct mem_block *)ptr;
        struct arena *a = block2arena(m_block);
        lock_acquire(&a->desc->list_lock);
        // 回收block，加入链表
        list_push_back(&a->desc->free_mem_list, &m_block->node);
        if (a->cnt < a->desc->blocks_per_arena - 1) {
            // arena中仍存在未回收的block, arena不能回收
            a->cnt++;
            // 解锁并返回
            lock_release(&a->desc->list_lock);
            return;
        }
        // arena的所有block均已回收，整个arena可以被回收
        a->cnt = 0;
        // 遍历移除arena中的所有block
        for (int i = 0; i < a->desc->blocks_per_arena; i++) {
            m_block = arena2block(a, i);
            ASSERT(list_exist(&a->desc->free_mem_list, &m_block->node));
            list_remove(&a->desc->free_mem_list, &m_block->node);
        }
        // 链表操作完毕，解锁
        lock_release(&a->desc->list_lock);
        // 删除arena这一整页内存
        pages_free((uint32_t)a, 1);
    }
}
