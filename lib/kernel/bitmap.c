#include "bitmap.h"

/* 将btmp的位图的每一位初始化为init_val*/
void bitmap_init(struct bitmap *btmp, uint8_t init_val) {
    // init_val为每一位的初始值，只能为0或1
    // 这里将init_val更新为每一字节的初始值
    // 若每一位的初始值为0，则每一字节的初始值亦为0
    // 若每一位的初始值为1，则每一字节的初始值为0xff
    if (init_val)
        init_val = 0xff;

    for (int i = 0; i < btmp->btmp_bytes_len; i++)
        btmp->bits[i] = init_val;
}

/* 获取位图的第bit_idx位的值 */
uint8_t bitmap_getbit(struct bitmap *btmp, uint32_t bit_idx) {
    uint32_t byte_idx = bit_idx / 8; // bit_idx在第几个字节
    bit_idx %= 8; // bit_idx更新为在字节中第几位
    return (btmp->bits[byte_idx] >> (7 - bit_idx)) & 1; //取值
}

/* 在位图中申请连续的num_bits个值为bit_val的位，返回起始位下标，若不存在则返回-1 */
int bitmap_alloc(struct bitmap *btmp, uint32_t num_bits, uint8_t bit_val) {
    uint8_t opposite_byte_val; // 与bit_val相反的值组成的字节值
    if (bit_val)
        opposite_byte_val = 0x00;
    else
        opposite_byte_val = 0xff;
    uint32_t cur_byte_idx = 0;
    uint32_t bits_count = 0;
    int result_idx; // 返回值

    while (cur_byte_idx < btmp->btmp_bytes_len) {

        // 以下for循环跳过连续的值为opposite_byte_val的字节
        for ( ; cur_byte_idx < btmp->btmp_bytes_len; cur_byte_idx++)
            if (btmp->bits[cur_byte_idx] != opposite_byte_val) break;
        if (cur_byte_idx == btmp->btmp_bytes_len)
            break;
        // cur_byte_idx字节不等于opposite_byte_val, 说明cur_byte_idx字节的位中存在bit_val

        uint8_t bit_idx = 0;
        // 以下for循环找出 cur_byte_idx字节 中首个 值为bit_val 的位
        for ( ; bit_idx < 8; bit_idx++) // 逐位判断
            if ( ((btmp->bits[cur_byte_idx] >> (7 - bit_idx)) & 1) == bit_val)
                break;
        uint32_t cur_byte_bits = cur_byte_idx << 3; // 记录 0 ~ cur_byte_idx字节的总共位数(cur_byte_bits = cur_byte_idx * 8)
        result_idx = cur_byte_bits + bit_idx; // 赋值给result_idx

        // 以下嵌套循环 从cur_byte_idx字节的bit_idx位 开始向后遍历位，寻找连续的值为bit_val的位
        for ( ; cur_byte_idx < btmp->btmp_bytes_len; cur_byte_idx++) {
            cur_byte_bits = cur_byte_idx << 3; //cur_byte_idx * 8
            // 逐位判断
            for ( ; bit_idx < 8; bit_idx++) {
                if ( ((btmp->bits[cur_byte_idx] >> (7 - bit_idx)) & 1) == bit_val)
                    bits_count++;
                else {
                    // 若位的值不为bit_val, 则只能重新寻找
                    bits_count = 0;
                    result_idx = cur_byte_bits + bit_idx + 1; //result_idx更新为下一位
                }
                if (bits_count == num_bits) {
                    // 达到需要的位数，返回
                    return result_idx;
                }
            }
            if (result_idx > cur_byte_bits + 7) {
                // 回到while循环
                cur_byte_idx++;
                break;
            }
            bit_idx = 0;
        }
    }
    return -1;
}

/* 设置位图的第bit_idx位的值为bit_val */
void bitmap_setbit(struct bitmap *btmp, uint32_t bit_idx, uint8_t bit_val) {
    uint32_t byte_idx = bit_idx / 8;
    bit_idx %= 8;
    if (bit_val) {
        // 第bit_idx位由0更新为1
        btmp->bits[byte_idx] |= (1 << (7 - bit_idx));
    }
    else {
        // 第bit_idx位由1更新为0
        btmp->bits[byte_idx] &= (~(1 << (7 - bit_idx)));
    }
}
