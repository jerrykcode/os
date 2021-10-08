#include "buildin_cmd.h"
#include "string.h"
#include "syscall.h"
#include "stddef.h"
#include "fs.h"

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
