#include "memory.h"
#include "print.h"
#include "string.h"
#include "stddef.h"

#define PAGE_SIZE       4096

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

    // kernel物理内存池
    uint32_t kernel_phy_addr_start = used_mem;
    kernel_pool.phy_addr_start = kernel_phy_addr_start;
    kernel_pool.pool_size = kernel_page_num * PAGE_SIZE;
    // kernel物理内存池位图
    kernel_pool.pool_btmp.btmp_bytes_len = kernel_page_num / 8;
    kernel_pool.pool_btmp.bits = (void *)MEM_BITMAP_BASE;
    bitmap_init(&kernel_pool.pool_btmp, BTMP_MEM_FREE);

    // user物理内存池, 紧跟在kernel物理内存池之后
    uint32_t user_phy_addr_start = kernel_phy_addr_start + kernel_page_num * PAGE_SIZE;
    user_pool.phy_addr_start = user_phy_addr_start;
    user_pool.pool_size = user_page_num * PAGE_SIZE;
    // user物理内存池位图, 紧跟在kernel物理内存池位图之后
    user_pool.pool_btmp.btmp_bytes_len = user_page_num / 8;
    user_pool.pool_btmp.bits = kernel_pool.pool_btmp.bits + kernel_pool.pool_btmp.btmp_bytes_len;
    bitmap_init(&user_pool.pool_btmp, BTMP_MEM_FREE);

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

void *get_kernel_page(uint32_t pages_num) {
    return NULL;
}

void *malloc_page(enum pool_flags pf, uint32_t pages_num) {
    return NULL;
}

uint32_t *pte_ptr(uint32_t vaddr) {
    return NULL;
}

uint32_t *pde_ptr(uint32_t vaddr) {
    return NULL;
}
