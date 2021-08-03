#include "stdio-kernel.h"
#include "stddef.h"
#include "stdio.h"

#define va_start(ap, v) ap = (va_list)&v    /* 将ap指向栈中首个固定参数 */
#define va_arg(ap, T) *((T *)(ap += 4))     /* 将ap指向栈中`下一个参数，并返回其值 */
#define va_end(ap) ap = NULL                /* 清除ap */

uint32_t k_printf(const char *str, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, str);
    uint32_t result = vsprintf(buf, str, ap);
    va_end(ap);
    console_put_str(buf);
    return result;
}
