#include "print.h"
#include "init.h"
#include "memory.h"
#include "asm.h"
#include "debug.h"

int main() {
    put_char('h');
    put_char('e');
    put_char('l');
    put_char('l');
    put_char('o');
    put_char('\n');
    put_char('k');
    put_char('e');
    put_char('r');
    put_char('n');
    put_char('e');
    put_char('l');
    put_char('1');
    put_char('2');
    put_char('\b');
    put_char('3');

    put_str("\nThis is a char string\n");

    put_int_hex(0);
    put_char('\n');
    
    put_int_hex(255);
    put_char('\n');

    put_int_hex(0x12345678);
    put_char('\n');
    
    put_int_hex(0x00000000);

    init_all();

    // 测试malloc_kernel_page函数
    for (int i = 0; i < 3; i++) {
        void *addr = malloc_kernel_page(3);
        put_str("\n malloc_kernel_page start vaddr: 0x");
        put_int_hex((uint32_t)addr);
        put_char('\n');
    }
    
//    asm volatile("sti");
//    ASSERT(1==2);
    while (1) {}
    return (0);
}
