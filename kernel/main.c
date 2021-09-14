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
#include "fs.h"
#include "string.h"

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

    int32_t fd = sys_open("/file0", O_RW);
    printf("    open file with fd:%d\n", fd);
    sys_write(fd, "hello, world!\n", 14);
    char buf[32] = {0};
    sys_read(fd, buf, 14);
    printf("read form fd:%d: %s", fd, buf);
    sys_lseek(fd, 7, SEEK_SET);
    memset(buf, 0, 32);
    sys_read(fd, buf, 7);
    printf("read :%s", buf);
    sys_lseek(fd, 13, SEEK_CUR);
    sys_read(fd, buf, 15);
    printf("read :%s", buf);
    sys_lseek(fd, -15, SEEK_END);
    sys_read(fd, buf, 15);
    printf("read: %s", buf);
    sys_close(fd);
    printf("    close file with fd:%d\n", fd);

    printf("/file0 delete %s!\n", sys_unlink("/file0") == 0 ? "success" : "fail");

    printf("\nmkdir /dir0 %s\n", sys_mkdir("/dir0/") == 0 ? "success" : "fail");
    fd = sys_open("/dir0/f", O_CREATE | O_RW);
    printf("open(create) file /dir0/f with fd:%d\n", fd);
    sys_write(fd, "hello, world!\n", 14);
    sys_read(fd, buf, 14);
    printf("read: %s", buf);
    sys_close(fd);
    printf("    close file with fd:%d\n", fd);

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
        sleep(5000); // 暂停5秒
        printf("I am :%s; My pid: 0x%x(dec %d)%c. #%d alloc memory with virtual addr: 0x%x; 0x%x; 0x%x\n", "user_prog_2", usr2_pid, usr2_pid, '\n', \
           i, (uint32_t)vaddr0, (uint32_t)vaddr1, (uint32_t)vaddr2);
        free(vaddr0);
        free(vaddr1);
        free(vaddr2);
    }
    while (1) {
    }
}
