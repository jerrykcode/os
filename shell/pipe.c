#include "pipe.h"
#include "fs.h"
#include "file.h"
#include "memory.h"
#include "ioqueue.h"
#include "thread.h"

bool is_pipe(uint32_t local_fd) {
    uint32_t global_fd = fd_local2global(local_fd);
    return file_table[global_fd].fd_flag == FILE_PIPE_FLAG;
}

int32_t sys_pipe(int32_t pipefd[2]) {
    int32_t global_fd = get_free_slot_in_global();
    if (global_fd == -1)
        return -1;

    file_table[global_fd].fd_inode = malloc_kernel_page(1);
    if (file_table[global_fd].fd_inode == NULL) {
        return -1;
    }

    ioqueue_init((struct ioqueue_st *)file_table[global_fd].fd_inode);

    file_table[global_fd].fd_flag = FILE_PIPE_FLAG;

    file_table[global_fd].fd_pos = 2;

    pipefd[0] = pcb_fd_install(global_fd);
    pipefd[1] = pcb_fd_install(global_fd);

    return 0;
}

uint32_t pipe_read(int32_t fd, void *buf, uint32_t count) {
    uint32_t global_fd = fd_local2global(fd);
    struct ioqueue_st *ioqueue = (struct ioqueue_st *)file_table[global_fd].fd_inode;
    char *chbuf = (char *)buf;

    uint32_t ioq_len = ioqueue_length(ioqueue);
    if (ioq_len < count)
        count = ioq_len;

    for (int i = 0; i < count; i++) {
        chbuf[i] = ioqueue_getchar(ioqueue);
    }

    return count;
}

uint32_t pipe_write(int32_t fd, void *buf, uint32_t count) {
    uint32_t global_fd = fd_local2global(fd);
    struct ioqueue_st *ioqueue = (struct ioqueue_st *)file_table[global_fd].fd_inode;
    char *chbuf = (char *)buf;

    uint32_t ioq_sizeleft = IOQUEUE_SIZE - ioqueue_length(ioqueue) - 1;
    if (ioq_sizeleft < count)
        count = ioq_sizeleft;

    for (int i = 0; i < count; i++) {
        ioqueue_putchar(ioqueue, chbuf[i]);
    }

    return count;
}

/* 文件重定向 */
void sys_fd_redirect(int32_t old_localfd, int32_t new_localfd) {
    struct task_st *cur = current_thread();
    if (old_localfd < 3) {
        cur->fd_table[old_localfd] = new_localfd;      
    }
    else {
        int32_t new_globalfd = cur->fd_table[new_localfd];
        cur->fd_table[old_localfd] = new_globalfd;
    }
}
