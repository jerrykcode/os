#ifndef MEMORY_H__
#define MEMORY_H__
#include "stdint.h"
#include "bitmap.h"
#include "list.h"
#include "sync.h"

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

#define BTMP_MEM_FREE   0
#define BTMP_MEM_USED   1

extern struct pool kernel_pool, user_pool;

// 堆內存相关
struct mem_block {
    struct list_node_st node;
};

struct mem_block_desc {
    uint32_t block_size;
    uint32_t blocks_per_arena;
    struct list_st free_mem_list;
    struct lock_st list_lock;
};

// 7个mem_block_desc, 
// 分别大小16，32，64，128，256，512，1024字节
#define MEM_BLOCK_DESC_NUM    7

//

void mem_block_desc_init(struct mem_block_desc desc_arr[]);
void mem_init();
void *malloc_kernel_page(uint32_t pages_num);
void *malloc_user_page(uint32_t pages_num);
void *malloc_page(enum pool_flags pf, uint32_t pages_num);
void pages_free(uint32_t vaddr, uint32_t pages_num);
void phy_page_free_in_exit(uint32_t phy_addr);
void *malloc_page_with_vaddr(enum pool_flags pf, uint32_t vaddr);
void *malloc_page_for_vaddr_in_fork(enum pool_flags pf, uint32_t vaddr);
uint32_t *pte_ptr(uint32_t vaddr);
uint32_t *pde_ptr(uint32_t vaddr);
uint32_t vaddr2phy(uint32_t vaddr);
void *sys_malloc(uint32_t size);
void sys_free(void *ptr);

#endif
