#include "print.h"
#include "init.h"
#include "memory.h"
#include "asm.h"
#include "thread.h"
#include "debug.h"
#include "console.h"

void thread_1(void *);
void thread_2(void *);

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

    thread_start("thread_A", 31, thread_1, "arg A "); 
    thread_start("thread_B", 31, thread_2, "arg B ");
    thread_start("thread_C", 31, thread_2, "arg C ");


    asm volatile("sti");
    while (1) {
        //console_put_str("Main ");
    }
    return (0);
}

void thread_1(void *arg) {
    char *str = arg;
    while (1) {
        int x = 7;
        int y = x >> 1;
        x -= y;
        //console_put_str(str);
    }
}

void thread_2(void *arg) {
    char *str = arg;
    while (1) {
        int i = 3;
        int j= i << 2 | 1;
        i *= j;
        //console_put_str(str);
    }
}
