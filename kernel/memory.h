#ifndef MEMORY_H__
#define MEMORY_H__
#include "stdint.h"
#include "bitmap.h"

enum pool_flags {
    PF_KERNEL,
    PF_USER
};

#define PAGE_SIZE       4096

#define PG_P_1  1
#define PG_P_0  0
#define PG_RW_R 0
#define PG_RW_W 2
#define PG_US_S 0
#define PG_US_U 4

// struct __bitmap 与 lib/kernel/bitmap.h 中的 struct bitmap 一样
// 如果不定义 struct __bitmap 而使用 struct bitmap 编译通不过!!
struct __bitmap {
    uint32_t btmp_bytes_len;
    uint8_t *bits;
};

struct virtual_addr {
    struct __bitmap vaddr_btmp;
    uint32_t vaddr_start;
};

extern struct pool kernel_pool, user_pool;

void mem_init();
void *malloc_kernel_page(uint32_t pages_num);
void *malloc_page(enum pool_flags pf, uint32_t pages_num);
uint32_t *pte_ptr(uint32_t vaddr);
uint32_t *pde_ptr(uint32_t vaddr);

#endif
