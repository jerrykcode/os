#ifndef EXEC_H__
#define EXEC_H__

#include "stdint.h"
int32_t sys_execv(const char *path, const char *argv[]);

#endif
