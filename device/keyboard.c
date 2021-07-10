#include "keyboard.h"
#include "ioqueue.h"
#include "print.h"
#include "interrupt.h"
#include "io.h"
#include "global.h"
#include "stdbool.h"

#define KEYBOARD_BUF_PORT 0x60

// 定义特定键的通码与断码
// 通码 | 0x0080 得到断码

#define ctrl_l_make     0x1d
#define ctrl_l_break    0x9d
#define ctrl_r_make     0xe01d
#define ctrl_r_break    0xe09d

#define shift_l_make    0x2a
#define shift_l_break   0xaa
#define shift_r_make    0x36
#define shift_r_break   0xb6

#define alt_l_make      0x38
#define alt_l_break     0xb8
#define alt_r_make      0xe038
#define alt_r_break     0xe0b8

#define caps_lock_make  0x3a
#define caps_lock_break 0xba

struct ioqueue_st ioqueue;

static bool ctrl_down, shift_down, alt_down, caps_lock_on;
static bool ext_scancode;

static void status_init() {
    ctrl_down = shift_down = alt_down = caps_lock_on = false;
    ext_scancode = false;
}

#define esc             '\033'
#define backspace       '\b'
#define tab             '\t'
#define enter           '\r'
#define delete          '\177'

static char shift_table[][2] = {
/* 0x00 */   {0, 0},
/* 0x01 */   {esc, esc},
/* 0x02 */   {'1', '!'},
/* 0x03 */   {'2', '@'},
/* 0x04 */   {'3', '#'},
/* 0x05 */   {'4', '$'},
/* 0x06 */   {'5', '%'},
/* 0x07 */   {'6', '^'},
/* 0x08 */   {'7', '&'},
/* 0x09 */   {'8', '*'},
/* 0x0a */   {'9', '('},
/* 0x0b */   {'0', ')'},
/* 0x0c */   {'-', '_'},
/* 0x0d */   {'=', '+'},
/* 0x0e */   {backspace, backspace},
/* 0x0f */   {tab, tab},
/* 0x10 */   {'q', 'Q'},
/* 0x11 */   {'w', 'W'},
/* 0x12 */   {'e', 'E'},
/* 0x13 */   {'r', 'R'},
/* 0x14 */   {'t', 'T'},
/* 0x15 */   {'y', 'Y'},
/* 0x16 */   {'u', 'U'},
/* 0x17 */   {'i', 'I'},
/* 0x18 */   {'o', 'O'},
/* 0x19 */   {'p', 'P'},
/* 0x1a */   {'[', '{'},
/* 0x1b */   {']', '}'},
/* 0x1c */   {enter, enter},
/* 0x1d */   {0, 0},    // 左ctrl键, 控制键不会使用shift_table数组
/* 0x1e */   {'a', 'A'},
/* 0x1f */   {'s', 'S'},
/* 0x20 */   {'d', 'D'},
/* 0x21 */   {'f', 'F'},
/* 0x22 */   {'g', 'G'},
/* 0x23 */   {'h', 'H'},
/* 0x24 */   {'j', 'J'},
/* 0x25 */   {'k', 'K'},
/* 0x26 */   {'l', 'L'},
/* 0x27 */   {';', ':'},
/* 0x28 */   {'\'', '\"'},
/* 0x29 */   {'`', '~'},
/* 0x2a */   {0, 0},    // 左shift键, 控制键不会使用shift_table数组
/* 0x2b */   {'\\', '|'},
/* 0x2c */   {'z', 'Z'},
/* 0x2d */   {'x', 'X'},
/* 0x2e */   {'c', 'C'},
/* 0x2f */   {'v', 'V'},
/* 0x30 */   {'b', 'B'},
/* 0x31 */   {'n', 'N'},
/* 0x32 */   {'m', 'M'},
/* 0x33 */   {',', '<'},
/* 0x34 */   {'.', '>'},
/* 0x35 */   {'/', '?'},
/* 0x36 */   {0, 0},    // 右shift键, 控制键不会使用shift_table数组
/* 0x37 */   {'*', '*'},
/* 0x38 */   {0, 0},    // 左alt键, 控制键不会使用shift_table数组
/* 0x39 */   {' ', ' '},
/* 0x3a */   {0, 0}    // caps_lock键, 控制键不会使用shift_table数组
};

static void intr_keyboard_handler() {
    uint16_t scancode = (uint16_t)inb(KEYBOARD_BUF_PORT);
    if (scancode == 0xe0) {
        // 输入的是扩展字符，需要和下一次中断输入的扫描码配合使用
        ext_scancode = true;
        return;
    }

    if (ext_scancode) {
        scancode = 0xe000 | scancode;
        ext_scancode = false;
    }

    bool is_make_code = ((scancode & 0x0080) == 0); // scancode的第8位为0表示通码，为1表示断码
    if (! is_make_code) {
        // 断码
        switch (scancode) {
            case ctrl_l_break:
            case ctrl_r_break:
                ctrl_down = false; // 松开ctrl
                break;
            case shift_l_break:
            case shift_r_break:
                shift_down = false;
                break;
            case alt_l_break:
            case alt_r_break:
                alt_down = false;
                break;
            default:
                // 其他键松开不需要处理
                break;
        }
        return;
    }

    // 通码

    bool is_control = true; // 按下的键是否是控制键(ctrl, shift, alt 和 caps_lock)

    switch (scancode) {
        case ctrl_l_make:
        case ctrl_r_make:
            ctrl_down = true; // 按下ctrl
            break;
        case shift_l_make:
        case shift_r_make:
            shift_down = true;
            break;
        case alt_l_make:
        case alt_r_make:
            alt_down = true;
            break;
        case caps_lock_make: 
            // 每次按下caps_lock都会使大小写状态取反
            // 即开启大写时，按下caps_lock则关闭大写; 关闭大写时，按下caps_lock则开启大写
            caps_lock_on = !caps_lock_on;
            break;
        default:
            is_control = false; // 不是控制键
            break;
    }

    if (is_control)
        return;

    // 非控制键

    bool shift_act = false; // shift键是否起作用

    if ((scancode >= 0x10 && scancode <= 0x19)
        || (scancode >= 0x1e && scancode <= 0x26)
        || (scancode >= 0x2c && scancode <= 0x32)) {
        // 按下键为字母
        // 0x10~0x19 对应键 Q~P (键盘第一排字母)
        // 0x1e~0x26 对应键 A~L (键盘第二排字母)
        // 0x2c~0x32 对应键 Z~M (键盘第三排字母)
        if (shift_down && caps_lock_on) {
            // 按下shift键同时开启caps_lock键，对字母来说作用抵消，输出小写
            shift_act = false;
        }
        else if (shift_down || caps_lock_on) {
            // shift和caps_lock中一个起作用，大写
            shift_act = true;
        }
        else {
            // shift和caps_lock都不用，小写
            shift_act = false;
        }
    }
    else {
        // 对于非字母来说，按下shift键即shift起作用
        shift_act = shift_down;
    }

    char ch = shift_table[scancode & 0xff][shift_act];
    put_char(ch);
    ioqueue_putchar(&ioqueue, ch);
}

void keyboard_init() {
    put_str("keyboard_init start\n");
    status_init();
    ioqueue_init(&ioqueue);
    register_handler(0x21, intr_keyboard_handler);
    put_str("keyboard_int finished\n");
}

