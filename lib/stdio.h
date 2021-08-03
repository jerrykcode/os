#ifndef LIB_STDIO_H__
#define LIB_STDIO_H__
#include "stdint.h"

typedef char *va_list;
uint32_t printf(const char *str, ...);
uint32_t sprintf(char *str, const char *format, ...);
uint32_t vsprintf(char *str, const char *format, va_list ap);

#endif
