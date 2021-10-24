#include "buildin_cmd.h"
#include "string.h"
#include "syscall.h"
#include "stddef.h"
#include "stdbool.h"
#include "stdio.h"
#include "fs.h"
#include "dir.h"

/* 将路径中的.及..替换为真实路径
   参数为绝对路径 */
static void wash_path(const char *original_path, char *final_path) {
    if (original_path[0] != '/') // 不是绝对路径
        return;
    int original_path_len = strlen(original_path);
    int final_path_len = 0;
    int start = 0, end; // 之后的while循环每次从original_path中解析出形如"/xxxxxx"的一层路径，
                         //start记录'/'的下标，end记录最后一个'x'的下标
    char *last_slash;
    while (1) {
        for (end = start; end + 1 < original_path_len; end++) // start位于'/'位置，寻找下一个'/'
            if (original_path[end + 1] == '/') {
                break;
            }
        if (end == start)
            break;
        // [start, end]区间对应形如"/xxxxxx"的一层路径
        if (original_path[start + 1] == '.') {
            if (start + 1 == end) { // "." 即当前目录本身，那么这一层"/."可以直接忽略
                goto NEXT;
            }
            if (start + 2 == end && original_path[end] == '.') { // "..", 需要回到上一层目录
                last_slash = strrchr(final_path, '/'); // 在final_path中寻找最后一个'/'
                if (last_slash != NULL) {
                    *last_slash = 0;
                    final_path_len = strlen(final_path);
                }
                goto NEXT;
            }
        }
        // 将路径加到final_path
        for (int i = start; i <= end; i++)
            final_path[final_path_len++] = original_path[i];
        final_path[final_path_len] = 0;
NEXT:
        start = end + 1;
    } // while
    if (final_path[0] == 0) {
        final_path[0] = '/';
        final_path[1] = 0;
    }
}

void make_clear_abs_path(const char *original_path, char *final_path) {
    char *original_path_abs = original_path;
    if (original_path[0] != '/') { // original_path是相对路径
        char buf[MAX_PATH_LEN << 1] = {0};
        getcwd(buf, MAX_PATH_LEN);
        if (strcmp(buf, "/") != 0) 
            strcat(buf, "/");
        strcat(buf, original_path);
        original_path_abs = buf;
    }
    wash_path(original_path_abs, final_path);
}

void buildin_ls(int argc, char *argv[]) {
    bool long_info = false;
    bool show_all = false;
    char *path = NULL;
    for (int i = 1; i < argc; i++) { // 枚举argv
        if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: ls [OPTION]... [FILE]..."); putchar("\n");
            printf("List information about the FILEs (the current directory by default)."); printf("\n"); putchar('\n');
            printf("Mandatory arguments to long options are mandatory for short options too."); putchar('\n');
            printf("  -a, --all                  do not ignore entries starting with ."); putchar('\n');
            printf("  -l                         use a long listing format"); putchar('\n');
            return;
        }
        if (strcmp(argv[i], "--all") == 0) {
            show_all = true;
            continue;
        }
        if (argv[i][0] == '-') { // -开头的是选项
            switch (argv[i][1]) {
                case 'l': // -l
                    long_info = true;
                    break;
                case 'a': // -a
                    show_all = true;
                    break;
                default:
                    printf("ls: invalid option -- \'%c\'%cTry \'ls --help\' for more information%c", argv[i][1], '\n', '\n');
                    return;
            }
        }
        else {
            if (path == NULL)
                path = argv[i];
            else {
                printf("ls: only support listing one directory each time!"); putchar('\n');
                return;
            }
        }
    }
    char wd[MAX_PATH_LEN] = {0}; // working directory
    if (path) { // argv中传入了path
        make_clear_abs_path(path, wd);
    }
    else {
        getcwd(wd, MAX_PATH_LEN); // 使用当前路径
    }
    uint32_t wd_len = strlen(wd);

    char ftype;
    struct dir_st *pdir;
    struct dir_entry_st *pdir_entry;
    struct stat_st fstat;
    if (stat(wd, &fstat) == -1) {
        return;
    }

    switch (fstat.file_type) {
        case FT_REGULAR: // 普通文件(非目录)
            if (long_info) {
                printf("- %d %d %s", fstat.inode_id, fstat.file_size, wd); putchar('\n');
            }
            else {
                printf("%s", wd); putchar('\n');
            }
            break;
        case FT_DIRECTORY: // 目录
            pdir = opendir(wd);
            while ((pdir_entry = readdir(pdir)) != NULL) { // 枚举目录项
                if (pdir_entry->filename[0] == '.' && !show_all)
                    continue;
                if (long_info) { // 需要详细信息
                    ftype = 'd';
                    if (pdir_entry->f_type == FT_REGULAR)
                        ftype = '-';
                    wd[wd_len] = '/';
                    wd[wd_len + 1] = 0;
                    strcat(wd, pdir_entry->filename);
                    stat(wd, &fstat);
                    printf("%c %d %d %s", ftype, pdir_entry->inode_id, fstat.file_size, pdir_entry->filename); putchar('\n');
                }
                else {
                    printf("%s ", pdir_entry->filename);
                }
            }
            if (!long_info)
                putchar('\b'); // 删除末尾空格
            closedir(pdir);
        default: break;
    } //switch
}
