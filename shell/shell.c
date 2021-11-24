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
static int32_t parse_cmd(char *cmd) {
    memset(argv, 0, MAX_ARGC);
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

static void println(char *str) { // 直接printf("...\n");就编译报错。很奇怪哦
    printf("%s%c", str, '\n');
}

static void cmd_execute(int argc) {
    if (strcmp(argv[0], "help") == 0) {
        println("buildin commands:");
        println("    ls: list file or directory information");
        println("    cd: change current working directory");
        println("    mkdir: create a new directory");
        println("    rm: remove file or empty directory");
        println("    pwd: print current working directory");
        println("    ps: print process information");
        println("    clear: clear screen");
        println("    help: display this help");
        println("shortcut key:");
        println("    ctrl + l: clear screen");
        println("    ctrl + u: clear input");
    } else if (strcmp(argv[0], "ls") == 0) {
        buildin_ls(argc, argv);
    }
    else if (strcmp(argv[0], "cd") == 0) {
        buildin_cd(argc, argv);
    }
    else if (strcmp(argv[0], "mkdir") == 0) {
        buildin_mkdir(argc, argv);
    }
    else if (strcmp(argv[0], "rm") == 0) {
        buildin_rm(argc, argv);
    }
    else if (strcmp(argv[0], "pwd") == 0) {
        buildin_pwd(argc, argv);
    }
    else if (strcmp(argv[0], "ps") == 0) {
        buildin_ps(argc, argv);
    }
    else if (strcmp(argv[0], "clear") == 0) {
        buildin_clear(argc, argv);
    }
    else {
        // 外部命令
        char path[MAX_PATH_LEN] = {0};
        make_clear_abs_path(argv[0], path);
        argv[0] = path;
        struct stat_st file_stat;
        memset(&file_stat, 0, sizeof(struct stat_st));
        if (stat(argv[0], &file_stat) == -1) {
            printf("can not access %s\n", argv[0]);
            return;
        }

        pid_t pid = fork();
        if (pid) {
            int32_t status;
            pid_t cpid = wait(&status);
            if (cpid != -1)
                printf("process with pid %d exit with status: %d", cpid, status);
        }
        else {
            execv(argv[0], argv);
        }
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
        int32_t argc = parse_cmd(cmd);
        if (strchr(cmd, '|') == NULL) { // 不存在'|'，不需要管道
            cmd_execute(argc);
        }
        else { // 需要管道
            char *each_cmd = cmd;
            char *pipe_char;
            int32_t pipefd[2] = {0};
            pipe(pipefd);
            fd_redirect(stdout_fd, pipefd[1]);

            // 执行第一个命令
            pipe_char = strchr(each_cmd, '|'); //pipe_char指向第一个'|'字符
            *pipe_char = 0; // each_cmd的第一个'|'字符被替换成了'\0'
            argc = parse_cmd(each_cmd);
            cmd_execute(argc);
            // 到这里，第一个命令已执行完，输出到了pipefd[1]

            fd_redirect(stdin_fd, pipefd[0]);
            each_cmd = pipe_char + 1; // each_cmd指向下一个命令开始的地方

            while ((pipe_char = strchr(each_cmd, '|')) != NULL) {
                *pipe_char = 0;
                argc = parse_cmd(each_cmd);
                cmd_execute(argc);
                each_cmd = pipe_char + 1;
            }

            // 到这里，只剩最后一个命令了
            // 最后一个命令的输出回到标准输出
            fd_redirect(stdout_fd, stdout_fd);
            argc = parse_cmd(each_cmd);
            cmd_execute(argc);

            fd_redirect(stdin_fd, stdin_fd);
            close(pipefd[0]);
            close(pipefd[1]);
        }
        
        printf("\n");
    }
}
