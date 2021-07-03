#ifndef CONSOLE_H__
#define CONSOLE_H__

#include "stdint.h"

void console_init();
void console_put_char(uint8_t c);
void console_put_str(uint8_t *str);
void console_put_int_hex(uint32_t number);

#endif
