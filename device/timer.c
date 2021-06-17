#include "timer.h"
#include "stdint.h"
#include "io.h"
#include "print.h"

#define PIT_CONTROL_PORT    0x43
#define COUNTER_0_PORT      0x40
#define COUNTER_0_NO        0
#define COUNTER_MODE        2
#define READ_WRITE_LATCH    3
#define IRQ_FREQUENCY       100
#define INPUT_FREQUENCY     1193180

static void set_frequency(uint8_t counter_no,
                          uint8_t rwl,
                          uint8_t counter_mode,
                          uint16_t init_value) {
    uint8_t control = (counter_no << 6) | (rwl << 4) | (counter_mode << 1); // 控制字
    outb(PIT_CONTROL_PORT, control);
    uint8_t counter_port = COUNTER_0_PORT + counter_no;
    outb(counter_port, (uint8_t)init_value); // 低8位
    outb(counter_port, (uint8_t)(init_value >> 8)); // 高8位
}

void timer_init() {
    put_str("timer_init start\n");
    set_frequency(COUNTER_0_NO, READ_WRITE_LATCH, COUNTER_MODE, (INPUT_FREQUENCY / IRQ_FREQUENCY));
    put_str("timer_init done\n");
}
