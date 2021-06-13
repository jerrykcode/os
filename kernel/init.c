#include "init.h"
#include "interrupt.h"
#include "print.h"

void init_all() {
    intr_init();
    put_str("all init done\n");
}
