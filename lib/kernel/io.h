#ifndef LIB_IO_H__
#define LIB_IO_H__
#include "stdint.h"
#include "asm.h"

/* 向端口写入1字节数据 */
static inline void outb(uint16_t port, uint8_t data) {
    asm volatile("outb %b1, %w0" : : "Nd"(port), "a"(data));
}

/* 从端口读取1字节数据 */
static inline uint8_t inb(uint16_t port) {
    uint8_t res;
    asm volatile("inb %w1, %b0" : "=a"(res) : "Nd"(port));
    return res;
}

/* 向端口写入从data_addr开始的数据，数据量word_num个字(word, 双字节) */
static inline void outsw(uint16_t port, const void *data_addr, uint32_t word_num) {
    asm volatile("cld; rep outsw" : "+S"(data_addr), "+c"(word_num) : "d"(port));
}

/* 从端口读取word_num个字(word, 双字节)的数据，并写到data_addr处 */
static inline void insw(uint16_t port, void *data_addr, uint32_t word_num) {
    asm volatile("cld; rep insw" : "+D"(data_addr), "+c"(word_num) : "d"(port) : "memory");
}

#endif
