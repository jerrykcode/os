#include "shell.h"
#include "stdio.h"
#include "syscall.h"
#include "string.h"
#include "file.h"
#include "buildin_cmd.h"

#define CMD_LEN 128
#define MAX_ARGC 16

char cwd[64] = {0};
char usrname[16] = {0};
char hostname[16] = {0};

char cmd[CMD_LEN] = {0};
char *argv[MAX_ARGC];

static void print_prompt() {
    printf("[%s@%s:%s]$", usrname, hostname, cwd);
}

static void read_cmd() {
    char *pos = cmd;
    while (read(stdin_fd, pos, 1) != -1) {
        if ((pos == cmd) && (*pos == '\b'))
            continue; 
        putchar(*pos);
        switch (*pos) {
            case '\n':
            case '\r':
                *pos = 0;
                return;
            case '\b':
                pos--;
                break;
            case 'l' - 'a': // ctrl + l 快捷键
                *pos = 0;
                clear(); // 清屏
                print_prompt(); // 输出提示
                printf(cmd); // 重新输出刚才输入的内容
                break;
            case 'u' - 'a': // ctrl + u 快捷键
                while (pos != cmd) {
                    *(--pos) = 0;
                    putchar('\b');
                }
                *pos = 0;
                putchar('\b');
                break;
            default:
                pos++;
        }
        if (pos - cmd >= CMD_LEN)
            break;
    }
}

// 将cmd通过空格分割，解析成参数并存入argv, 返回参数个数argc
static int32_t parse_cmd() {
    int32_t argc = 0;
    char token = ' '; // 以空格为分隔符
    char *cmd_ptr = cmd;
    while (argc < MAX_ARGC) {
        while (*cmd_ptr == token) // 去掉开头的token
            cmd_ptr++;
        if (*cmd_ptr == 0)
            break;
        argv[argc++] = cmd_ptr; // 存入argv
        while (*cmd_ptr != token && *cmd_ptr != 0)
            cmd_ptr++;
        if (*cmd_ptr == 0)
            break;
        // *cmd_ptr == token
        *cmd_ptr = 0; // 改为'\0', argv中的这个字符串就结束了
        cmd_ptr++;
    }
    return argc;
}

void shell() {
    cwd[0] = '/';
    strcpy(usrname, "zzf");
    strcpy(hostname, "localhost");
    char buf[MAX_PATH_LEN]= {0};
    while (1) {
        print_prompt();
        memset(cmd, 0, CMD_LEN);
        read_cmd();
        if (cmd[0] == 0)
            continue;
        // 处理cmd
        int32_t argc = parse_cmd();
        if (strcmp(argv[0], "ls") == 0) {
            buildin_ls(argc, argv);
        }
        printf("\n");
    }
}
