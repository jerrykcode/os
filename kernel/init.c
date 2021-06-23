#include "init.h"
#include "interrupt.h"
#include "print.h"
#include "timer.h"
#include "memory.h"

void init_all() {
    intr_init();
    timer_init();
    mem_init();
    put_str("all init done\n");
}
