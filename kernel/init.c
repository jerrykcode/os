#include "init.h"
#include "interrupt.h"
#include "print.h"
#include "timer.h"

void init_all() {
    intr_init();
    timer_init();
    put_str("all init done\n");
}
