#ifndef PIPE_H__
#define PIPE_H__

#include "stdint.h"
#include "stdbool.h"

#define FILE_PIPE_FLAG 0xffff

bool is_pipe(uint32_t local_fd);
int32_t sys_pipe(int32_t pipefd[2]);
uint32_t pipe_read(int32_t fd, void *buf, uint32_t count);
uint32_t pipe_write(int32_t fd, void *buf, uint32_t count);
void sys_fd_redirect(int32_t old_localfd, int32_t new_localfd);

#endif
