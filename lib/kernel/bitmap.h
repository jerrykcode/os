#ifndef LIB_STRING_H__
#define LIB_STRING_H__
#include "stdint.h"

struct bitmap {
    uint32_t btmp_bytes_len;
    uint8_t *bits;
};

/* 将btmp的位图的每一位初始化为init_val*/
void bitmap_init(struct bitmap *btmp, uint8_t init_val);

/* 获取位图的第bit_idx位的值 */
uint8_t bitmap_getbit(struct bitmap *btmp, uint32_t bit_idx);

/* 在位图中申请连续的num_bits个值为bit_val的位，返回起始位下标，若不存在则返回-1 */
int bitmap_alloc(struct bitmap *btmp, uint32_t num_bits, uint8_t bit_val);

/* 设置位图的第bit_idx位的值为bit_val */
void bitmap_setbit(struct bitmap *btmp, uint32_t bit_idx, uint8_t bit_val);
#endif
