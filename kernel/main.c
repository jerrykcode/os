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
#include "dir.h"
#include "string.h"
#include "shell.h"
#include "global.h"

void app_install(const char *app_name, uint32_t file_size, uint32_t lba);
void init(void);
void thread_1(void *);
void thread_2(void *);
void usrprog_1(void);
void usrprog_2(void);

int main() {

    init_all();
    int32_t fd = sys_open("/hello", O_CREATE | O_RW);
    sys_write(fd, (void *)"hello", 6);
    sys_close(fd);
    app_install("/cat", 6588, 300);

    process_execute(init, "init");   

    asm volatile("sti");


    while (1) {
    }
    return (0);
}

void app_install(const char *app_name, uint32_t file_size, uint32_t lba) {
    struct stat_st stat;
    if (sys_stat(app_name, &stat) == -1) {
        k_printf("app install begin!\n");
        uint32_t sec_num = DIV_ROUND_UP(file_size, 512);
        void *buf = sys_malloc(file_size);
        struct disk_st *disk = &channels[0].devices[0];
        ide_read(disk, lba, sec_num, buf);
        int32_t fd = sys_open(app_name, O_CREATE | O_RW);
        if (fd != -1) {
            if (sys_write(fd, buf, file_size) == file_size) {
                k_printf("app install finished!\n");
            }
        }
        sys_free(buf);
    }
}

void init(void) {
    int fd[2];
    pipe(fd);
    pid_t ret_pid = fork();
    if (ret_pid) {
        printf("I am father, my pid is %d; ret pid: %d\n", getpid(), ret_pid);
        close(fd[0]);
        write(fd[1], "message from father: hello\n", 27);
        while (1);
    }
    else {
        close(fd[1]);
        printf("I am child, my pid is %d; ret pid: %d\n", getpid(), ret_pid);
        clear();
        char buf[32] = {0};
        read(fd[0], buf, 27);
        printf("Recive %s", buf);
        shell();
    }
}
