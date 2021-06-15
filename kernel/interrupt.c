#include "interrupt.h"
#include "global.h"
#include "stdint.h"
#include "print.h"
#include "io.h"
#include "asm.h"

#define INTR_NUM    0x21    // 目前总共支持的中断数

#define PIC_M_CTRL  0x20    // 8259A 主片的控制端口是0x20
#define PIC_M_DATA  0x21    // 主片数据端口
#define PIC_S_CTRL  0xa0    // 从片控制端口
#define PIC_S_DATA  0xa1    // 从片数据端口


struct intr_gate_desc {
    // 中断门描述符
    uint16_t func_offset_low_word;  // 中断处理程序在目标代码段内的偏移量的低0~15位
    uint16_t func_selector;         // 中断处理程序目标代码段描述符选择子
    uint8_t dcount;                 // 此项为双字计数字段，是门描述符中的第4字节。此项固定值，不用考虑
    uint8_t attribute;
    uint16_t func_offset_high_word; // 中断处理程序在目标代码段内的偏移量的高16位
};

static struct intr_gate_desc idt[INTR_NUM];
extern intr_handler intr_entry_table[INTR_NUM];

/* 初始化中断描述符表idt */
static void idt_init() {
    for (int i = 0; i < INTR_NUM; i++) {
        // 创建中断门描述符
        idt[i].func_offset_low_word = (uint32_t)intr_entry_table[i] & 0x0000ffff;
        idt[i].func_selector = SELECTOR_K_CODE;
        idt[i].dcount = 0;
        idt[i].attribute = INTR_DESC_ATTR_DPL0;
        idt[i].func_offset_high_word = ((uint32_t)intr_entry_table[i] & 0xffff0000) >> 16;
    }
    put_str("idt init done\n");
}

/* 初始化可编程中断控制器8259A */
static void pic_init() {
    // 初始化主片
    outb(PIC_M_CTRL, 0x11); // ICW1 b00010001   级联，边沿触发
    outb(PIC_M_DATA, 0x20); // ICW2 b00100000   主片的起始中断向量号为0x20
    outb(PIC_M_DATA, 0x04); // ICW3 b00000100   使用IR2引脚来级联从片
    outb(PIC_M_DATA, 0x01); // ICW4 b00000001   x86，手动发送EOI结束中断

    // 初始化从片
    outb(PIC_S_CTRL, 0x11); // ICW1 b00010001   同主片ICW1
    outb(PIC_S_DATA, 0x28); // ICW2 b00101000   主片的中断向量号是0x20~0x27, 从片顺延，从0x28开始
    outb(PIC_S_DATA, 0x02); // ICW3 b00000010   连接在主片的IR2引脚上
    outb(PIC_S_DATA, 0x01); // ICW4 b00000001   同主片ICW4

    // 屏蔽除主片IR0外的所有中断
    // 即只响应时钟中断
    outb(PIC_M_DATA, 0xfe);
    outb(PIC_S_DATA, 0xff);

    put_str("pic init done\n");
}

/* 中断有关初始化工作 */
void intr_init() {
    idt_init();
    pic_init();
    uint64_t idt_operand = (sizeof(idt) - 1) | ((uint64_t)(uint32_t)idt << 16);
    asm volatile("lidt %0" : : "m"(idt_operand));
}