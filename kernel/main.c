#include "print.h"
#include "init.h"
#include "memory.h"
#include "asm.h"
#include "thread.h"
#include "debug.h"
#include "console.h"
#include "keyboard.h"
#include "process.h"
#include "syscall.h"
#include "syscall-init.h"
#include "stdio.h"

void thread_1(void *);
void thread_2(void *);
void usrprog_1(void);
void usrprog_2(void);

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
    
    process_execute(usrprog_1, "usr1");
    process_execute(usrprog_2, "usr2");

    asm volatile("sti");

    thread_start("thread_A", 31, thread_1, " arg A "); 
    thread_start("thread_B", 31, thread_2, " thread B ");
    thread_start("thread_C", 31, thread_2, " thread C ");

    while (1) {
        //console_put_str("Main ");
    }
    return (0);
}

void thread_1(void *arg) {
    char *str = arg;
    void *vaddr = sys_malloc(63);
    printf("thread 0x%d alloc memory with virtual addr:0x%x\n", sys_getpid(), (uint32_t)vaddr);
    while (1) {
        console_put_str(str);
        char ch = ioqueue_getchar(&ioqueue);
        console_put_char(ch);
        for (int i = 0; i < 100000; i++) ;
    }
}

void thread_2(void *arg) {
    char *str = arg;
    console_put_str(str);
    console_put_str("pid : 0x");
    console_put_int_hex(sys_getpid());
    console_put_char('\n');
    void *vaddr0 = sys_malloc(256);
    void *vaddr1 = sys_malloc(255);
    void *vaddr2 = sys_malloc(254);
    printf("thread 0x%d alloc memory with virtual addr:0x%x; 0x%x; 0x%x\n", sys_getpid(), (uint32_t)vaddr0, (uint32_t)vaddr1, (uint32_t)vaddr2);
    sys_free(vaddr0);
    sys_free(vaddr1);
    sys_free(vaddr2);
    while (1) {
    }
}

void usrprog_1() {
    pid_t usr1_pid = getpid();
    for (int i = 0; i < 3; i++) {
        void *vaddr0 = malloc(256);
        void *vaddr1 = malloc(255);
        void *vaddr2 = malloc(254);
        printf("I am :%s; My pid: 0x%x(dec %d)%c. #%d alloc memory with virtual addr: 0x%x; 0x%x; 0x%x\n", "user_prog_1", usr1_pid, usr1_pid, '\n', \
           i, (uint32_t)vaddr0, (uint32_t)vaddr1, (uint32_t)vaddr2);
        free(vaddr0);
        free(vaddr1);
        free(vaddr2);
    }
    while (1) {
    }
}

void usrprog_2() {
    pid_t usr2_pid = getpid();    
    for (int i = 0; i < 3; i++) {
        void *vaddr0 = malloc(256);
        void *vaddr1 = malloc(255);
        void *vaddr2 = malloc(254);
        printf("I am :%s; My pid: 0x%x(dec %d)%c. #%d alloc memory with virtual addr: 0x%x; 0x%x; 0x%x\n", "user_prog_2", usr2_pid, usr2_pid, '\n', \
           i, (uint32_t)vaddr0, (uint32_t)vaddr1, (uint32_t)vaddr2);
        free(vaddr0);
        free(vaddr1);
        free(vaddr2);
    }
    while (1) {
    }
}
