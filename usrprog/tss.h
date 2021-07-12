#ifndef TSS_H__
#define TSS_H__
#include "thread.h"
void update_tss_esp(struct task_st *pthread);
void tss_init();
#endif
