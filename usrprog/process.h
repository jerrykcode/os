#ifndef USRPROG_PROCESS_H__
#define USRPROG_PROCESS_H__

#define DEFAULT_PRIORITY 31
#define USR_VADDR_START 0x8048000

// 0xc0000000以上是内核空间
#define USR_STACK3_VADDR (0xc0000000 - 0x1000)

#include "thread.h"

void process_execute(void *filename, char *name);
void process_active(struct task_st *thread);
uint32_t *create_usr_page_table();
void delete_usr_page_table(uint32_t *page_table_vaddr);

#endif
