#include "init.h"
#include "interrupt.h"
#include "print.h"
#include "timer.h"
#include "memory.h"
#include "thread.h"

void init_all() {
    intr_init();
    timer_init();
    mem_init();
    thread_environment_init();
    put_str("all init done\n");
}
