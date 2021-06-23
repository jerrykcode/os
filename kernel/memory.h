#ifndef MEMORY_H__
#define MEMORY_H__
#include "stdint.h"
#include "bitmap.h"

enum pool_flags {
    PF_KERNEL,
    PF_USER
};

#define PG_P_1  1
#define PG_P_0  0
#define PG_RW_R 0
#define PG_RW_W 2
#define PG_US_S 0
#define PG_US_U 4

struct virtual_addr {
    struct bitmap vaddr_btmp;
    uint32_t vaddr_start;
};

extern struct pool kernel_pool, user_pool;

void mem_init();
void *get_kernel_page(uint32_t pages_num);
void *malloc_page(enum pool_flags pf, uint32_t pages_num);
uint32_t *pte_ptr(uint32_t vaddr);
uint32_t *pde_ptr(uint32_t vaddr);

#endif
