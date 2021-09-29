#include "shell.h"
#include "stdio.h"
#include "syscall.h"
#include "string.h"
#include "file.h"

#define CMD_LEN 128

char cwd[64] = {0};
char usrname[16] = {0};
char hostname[16] = {0};

char cmd[CMD_LEN] = {0};

static void print_prompt() {
    printf("[%s@%s:%s]$", usrname, hostname, cwd);
}

static void read_cmd() {
    char *pos = cmd;
    while (read(stdin_fd, pos, 1) != -1) {
        //putchar(*pos);
        switch (*pos) {
            case '\n':
            case '\r':
                *pos = 0;
                return;
            case '\b':
                if (pos) pos--;
                break;
            default:
                pos++;
        }
        if (pos - cmd >= CMD_LEN)
            break;
    }
}

void shell() {
    cwd[0] = '/';
    strcpy(usrname, "zzf");
    strcpy(hostname, "localhost");
    while (1) {
        print_prompt();
        memset(cmd, 0, CMD_LEN);
        read_cmd();
        if (cmd[0] == 0)
            continue;
        // 处理cmd
    }
}
