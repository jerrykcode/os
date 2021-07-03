#include "console.h"
#include "sync.h"
#include "stdint.h"
#include "print.h"

struct lock_st console_lock;

void console_init() {
    lock_init(&console_lock);
}

void console_put_char(uint8_t c) {
    lock_acquire(&console_lock);
    put_char(c);
    lock_release(&console_lock);
}

void console_put_str(uint8_t *str) {
    lock_acquire(&console_lock);
    put_str(str);
    lock_release(&console_lock);
}

void console_put_int_hex(uint32_t number) {
    lock_acquire(&console_lock);
    put_int_hex(number);
    lock_release(&console_lock);
}
