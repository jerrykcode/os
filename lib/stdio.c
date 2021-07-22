#include "stdio.h"
#include "stddef.h"
#include "syscall.h"
#include "string.h"

#define va_start(ap, v) ap = (va_list)&v    /* 将ap指向栈中首个固定参数 */
#define va_arg(ap, T) *((T *)(ap += 4))     /* 将ap指向栈中`下一个参数，并返回其值 */
#define va_end(ap) ap = NULL                /* 清除ap */

uint32_t printf(const char *str, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, str);
    vsprintf(buf, str, ap);
    va_end(ap);
    return write(buf);
}

/* 将val以base进制转换为字符串并写入buf, 返回字符串的长度 */
static uint32_t itoa(char *buf, uint32_t val, uint8_t base) {
    uint32_t len = 1;
    if (val >= base)
        len += itoa(buf, val / base, base);
    val %= base;
    char ch;
    if (val < 10)
        ch = val + '0';
    else
        ch = val - 10 + 'A';
    *(buf + len - 1) = ch;
    return len;
}

uint32_t vsprintf(char *str, const char *format, va_list ap) {
    char *dest = str;
    int arg_int;
    char arg_ch, *arg_str;
    while (*format) {
        if (*format != '%') {
            *dest++ = *format++;
            continue;
        }
        // *format == '%'
        format++; // 跨过'%'
        switch (*format++) { // '%'后面一个字符
            case 'c' :
                arg_ch = va_arg(ap, char);
                *dest++ = arg_ch;
                break;
            case 's' :                
                arg_str = va_arg(ap, char *);
                strcpy(dest, arg_str);
                dest += strlen(arg_str);
                break;
            case 'd' :
                arg_int = va_arg(ap, int);
                if (arg_int < 0) {
                    *dest++ = '-';
                    arg_int = 0 - arg_int;
                }
                dest += itoa(dest, arg_int, 10);
                break;
            case 'x' : 
                arg_int = va_arg(ap, int); // 获取栈中下一个参数
                dest += itoa(dest, arg_int, 16); // 写入16进制数
                break;
            default:
                break;
        }
    }
    *dest = '\0';
    return strlen(str);
}
