#include "timer.h"
#include "stdint.h"
#include "io.h"
#include "print.h"
#include "thread.h"
#include "interrupt.h"
#include "debug.h"
#include "global.h"

#define PIT_CONTROL_PORT    0x43
#define COUNTER_0_PORT      0x40
#define COUNTER_0_NO        0
#define COUNTER_MODE        2
#define READ_WRITE_LATCH    3
#define IRQ_FREQUENCY       100
#define INPUT_FREQUENCY     1193180

#define mil_seconds_per_intr (1000 / IRQ_FREQUENCY)

uint32_t total_ticks;

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

static void intr_timer_handler() {
    total_ticks++;

    struct task_st *cur = current_thread();
    ASSERT(cur->stack_magic == STACK_MAGIC); // 检查栈溢出
    if ((--cur->ticks) == 0) {
        schedule();
    }
}

void timer_init() {
    put_str("timer_init start\n");
    total_ticks = 0;
    set_frequency(COUNTER_0_NO, READ_WRITE_LATCH, COUNTER_MODE, (INPUT_FREQUENCY / IRQ_FREQUENCY));
    register_handler(0x20, intr_timer_handler);
    put_str("timer_init done\n");
}

static void ticks_to_sleep(uint32_t sleep_ticks) {
    int end_tick = total_ticks + sleep_ticks;
    while (total_ticks < end_tick) {
        thread_yield();
    }
}

void sys_sleep(uint32_t ms) {
    uint32_t sleep_ticks = DIV_ROUND_UP(ms, mil_seconds_per_intr);
    ASSERT(sleep_ticks > 0);
    ticks_to_sleep(sleep_ticks);
}
