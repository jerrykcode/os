#ifndef WAIT_EXIT_H__
#define WAIT_EXIT_H__

#include "thread.h"
#include "stdint.h"

pid_t sys_wait(int32_t *child_exit_status);
void sys_exit(int32_t exit_status);

#endif
