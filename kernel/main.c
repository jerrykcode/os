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
//    app_install("/app_no_arg", 5644, 300);
    app_install("/app_arg", 5952, 360);

    process_execute(init, "init");   
 
//    process_execute(usrprog_1, "usr1");
//    process_execute(usrprog_2, "usr2");

    asm volatile("sti");

//    thread_start("thread_A", 31, thread_1, " arg A "); 
//    thread_start("thread_B", 31, thread_2, " thread B ");
//    thread_start("thread_C", 31, thread_2, " thread C ");

/*    
    char buf[MAX_PATH_LEN] = {0};

    int32_t fd = sys_open("/file0", O_CREATE|O_RW);
    printf("    open file with fd:%d\n", fd);
    sys_write(fd, "hello, world!\n", 14);
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


    sys_open("/dir0/test", O_CREATE | O_RW);
    sys_open("/dir0/zzf", O_CREATE | O_RW);

    sys_mkdir("/dir0/d1/");
    sys_mkdir("/dir0/d1/d2/");
    sys_mkdir("/dir0/d1/zzf/");

    memset(buf, 0, MAX_PATH_LEN);
    sys_getcwd(buf, MAX_PATH_LEN);

    printf("current working directory: %s\n", buf);

    struct dir_st *dir = sys_opendir("/dir0");
    printf("open dir /dir0 %s\n", dir ? "success" : "fail");
    struct dir_entry_st *entry;
    while (entry = sys_readdir(dir)) {
        printf(" {%s}\n", entry->filename);
    }
    sys_rewinddir(dir);
    while (entry = sys_readdir(dir))
        printf(" {%s}\n", entry->filename);
    sys_closedir(dir);
    printf("close dir!");

    printf("change dir %s\n", sys_chdir("/dir0/d1/d2") == 0 ? "success" : "fail");
    memset(buf, 0, MAX_PATH_LEN);
    sys_getcwd(buf, MAX_PATH_LEN);
    printf("current working directory: %s\n", buf);

    printf("change dir %s\n", sys_chdir("/dir0/") == 0 ? "success" : "fail");
    memset(buf, 0, MAX_PATH_LEN);
    sys_getcwd(buf, MAX_PATH_LEN);
    printf("current working directory: %s\n", buf);

    printf("change dir %s\n", sys_chdir("/dir0/d1/zzf") == 0 ? "success" : "fail");
    memset(buf, 0, MAX_PATH_LEN);
    sys_getcwd(buf, MAX_PATH_LEN);
    printf("current working directory: %s\n", buf);

*/

    while (1) {
        //console_put_str("Main ");
    }
    return (0);
}

void app_install(const char *app_name, uint32_t file_size, uint32_t lba) {
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
/*    pid_t usr1_pid = getpid();
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
*/
    struct dir_st *dir = opendir("/dir0");
    printf("open dir /dir0 %s\n", dir ? "success" : "fail");
    struct dir_entry_st *entry;
    while (entry = readdir(dir)) {
        printf(" {%s}\n", entry->filename);
    }
    rewinddir(dir);
    while (entry = readdir(dir))
        printf(" {%s}\n", entry->filename);
    closedir(dir);
    printf("close dir!");

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
