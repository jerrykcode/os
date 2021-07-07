#include "interrupt.h"
#include "global.h"
#include "stdint.h"
#include "print.h"
#include "io.h"
#include "asm.h"

#define INTR_NUM    0x30    // 目前总共支持的中断数

#define PIC_M_CTRL  0x20    // 8259A 主片的控制端口是0x20
#define PIC_M_DATA  0x21    // 主片数据端口
#define PIC_S_CTRL  0xa0    // 从片控制端口
#define PIC_S_DATA  0xa1    // 从片数据端口

#define EFLAGS_IF   0x200   // IF位在EFLAGS中的位置: 第9位

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
intr_handler intr_handler_table[INTR_NUM];
char *intr_name[INTR_NUM];

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

    // 打开时钟中断与键盘中断
    outb(PIC_M_DATA, 0xfc);
    outb(PIC_S_DATA, 0xff);

    put_str("pic init done\n");
}

/* 通用中断处理函数 */
void general_intr_handler(uint8_t vec_nr) {
    if (vec_nr == 0x27 || vec_nr == 0x2f) {
        return;
    }
    set_cursor(0);
    for (int i = 0; i < 320; i++) // 清空屏幕上方4行
        put_char(' ');
    set_cursor(0);
    put_str("!!!! excetion message begin !!!!\n");
    put_str("       \n");
    put_str(intr_name[vec_nr]);
    if (vec_nr == 14) { // Page-fault
        int page_fault_vaddr = 0;
        asm volatile ("movl %%cr2, %0" : "=r" (page_fault_vaddr));
        put_str("\npage fault addr is: 0x"); put_int_hex(page_fault_vaddr);
    }
    put_str("!!!! excetion message end !!!!\n");
    while (1) {}
}

/* 初始化 完成通用中断处理函数注册及异常名称注册 */
static void intr_handler_table_init() {
    for (int i = 0; i < INTR_NUM; i++) {
        intr_handler_table[i] = general_intr_handler; //初始化为通用中断处理函数
        intr_name[i] = "unknown"; // 初始化为"unknown"
    }
    intr_name[0] = "#DE Divide Error";
    intr_name[1] = "#DB Debug Exception";
    intr_name[2] = "NMI Interrupt";
    intr_name[3] = "#BP Breakpoint Exception";
    intr_name[4] = "#OF Overflow Exception";
    intr_name[5] = "#BR Bound Range Exceed Exception";
    intr_name[6] = "#UD Invalid Opcode Exception";
    intr_name[7] = "#NM Device Not Available Exception";
    intr_name[8] = "#DF Double Fault Exception";
    intr_name[9] = "Coprocessor Segment Overrun";
    intr_name[10] = "#TS Invalid TSS Exception";
    intr_name[11] = "#NP Segment Not Present";
    intr_name[12] = "#SS Stack Fault Exception";
    intr_name[13] = "#GP General Protection Exception";
    intr_name[14] = "#PF Page-Fault Exception";
    // intr_name[15] 第15项intel保留
    intr_name[16] = "#MF x87 FPU Floating-Point Error";
    intr_name[17] = "#AC Alignment Check Exception";
    intr_name[18] = "#MC Machine-Check Exception";
    intr_name[19] = "#XF SIMD Floating-Point Exception";
}

enum intr_status intr_get_status() {
    uint32_t eflags;
    asm volatile("pushfl; popl %0" : "=g"(eflags));
    return (eflags & EFLAGS_IF) ? INTR_ON : INTR_OFF;
}

enum intr_status intr_set_status(enum intr_status status) {
    return status == INTR_ON ? intr_enable() : intr_disable();
}

enum intr_status intr_enable() {
    if (intr_get_status() == INTR_ON)
        return INTR_ON;
    asm volatile("sti");
    return INTR_OFF;
}

enum intr_status intr_disable() {
    if (intr_get_status() == INTR_OFF)
        return INTR_OFF;
    asm volatile("cli" : : : "memory");
    return INTR_ON;
}

/* 中断有关初始化工作 */
void intr_init() {
    idt_init();
    pic_init();
    intr_handler_table_init();
    uint64_t idt_operand = (sizeof(idt) - 1) | ((uint64_t)(uint32_t)idt << 16);
    asm volatile("lidt %0" : : "m"(idt_operand));
}

void register_handler(uint8_t vec_no, intr_handler handler) {
    intr_handler_table[vec_no] = handler;
}
