#ifndef INTERRUPT_H__
#define INTERRUPT_H__
#include "stdint.h"
typedef void *intr_handler;
/* 中断有关初始化工作 */
void intr_init();

enum intr_status {  // 中断状态
    INTR_OFF,       // 中断关闭状态
    INTR_ON         // 中断开启状态
};

enum intr_status intr_get_status();
enum intr_status intr_set_status(enum intr_status status);
enum intr_status intr_enable();
enum intr_status intr_disable();
void register_handler(uint8_t vec_no, intr_handler handler);
#endif
