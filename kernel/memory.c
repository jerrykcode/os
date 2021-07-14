#include "memory.h"
#include "print.h"
#include "string.h"
#include "stddef.h"
#include "debug.h"
#include "thread.h"

#define MEM_BITMAP_BASE 0xc009a000

#define K_HEAP_START    0xc0100000

#define BTMP_MEM_FREE   0
#define BTMP_MEM_USED   1

struct pool {
    struct bitmap pool_btmp;
    uint32_t phy_addr_start;
    uint32_t pool_size;
};

struct pool kernel_pool, user_pool;
struct virtual_addr kernel_vaddr;

static void mem_pool_init(uint32_t all_mem) {
    put_str("   mem_pool_init start\n");
    uint32_t used_mem = 256 * PAGE_SIZE + 0x100000; // 页表及低端1M内存
    uint32_t free_mem = all_mem - used_mem;
    uint32_t free_mem_page_num = free_mem / PAGE_SIZE;
    uint32_t kernel_page_num = free_mem_page_num / 2;
    uint32_t user_page_num = free_mem_page_num - kernel_page_num;

    put_str("       all_mem: 0x"); put_int_hex(all_mem); put_str("  free_mem: 0x"); put_int_hex(free_mem); put_char('\n');

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

void mem_init() {
    put_str("mem_init start\n");
    uint32_t all_mem = (*(uint32_t *)(0xb00));
    mem_pool_init(all_mem);
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
    void *vaddr = malloc_page(PF_KERNEL, pages_num);
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
