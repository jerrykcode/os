#include "syscall.h"
#include "stdio.h"
#include "string.h"
#include "stddef.h"
#include "fs.h"

int main(int argc, char *argv[]) {
    if (argc > 2) {
        printf("cat: argument error!\n");
        exit(-2);
    }
    if (argc == 1) {
        char buf[512] = {0};
        for (int i = 0; i < 512; i++) {
            read(0, &buf[i], 1);
            printf("%c", buf[i]);
            if (buf[i] == '\r' || buf[i] == '\n')
                break;
        }
        printf("%s", buf);
        exit(0);
    }

    char *path = argv[1];
    char abs_path[512] = {0};
    void *buf = malloc(1024);
    if (buf == NULL) {
        printf("cat: malloc error\n");
        exit(-1);
    }

    if (path[0] == '/') {
        strcpy(abs_path, path);
    }
    else {
        getcwd(abs_path, 512);
        strcat(abs_path, path);
    }

    int fd = open(abs_path, O_RONLY);
    if (fd == -1) {
        printf("cat: open error\n");
        free(buf);
        exit(-1);
    }

    int32_t bytes_read;
    while ((bytes_read = read(fd, buf, 1024)) != -1) {
        write(1, buf, bytes_read);
    }

    close(fd);
    free(buf);
    exit(0);
}
