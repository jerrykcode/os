#ifndef PRINT_H__
#define PRINT_H__
#include "stdint.h"
void put_char(uint8_t c);
void put_str(uint8_t *str);
void put_int_hex(uint32_t number);
void set_cursor(uint32_t cursor_pos);
void cls_screen();
#endif
