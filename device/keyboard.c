#include "keyboard.h"
#include "print.h"
#include "interrupt.h"
#include "io.h"
#include "global.h"

#define KEYBOARD_BUF_PORT 0x60

static void intr_keyboard_handler() {
    put_char('k');
    inb(KEYBOARD_BUF_PORT);
}

void keyboard_init() {
    put_str("keyboard_init start\n");
    register_handler(0x21, intr_keyboard_handler);
    put_str("keyboard_int finished\n");
}

