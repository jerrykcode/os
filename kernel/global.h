#ifndef GLOBAL_H__
#define GLOBAL_H__

#define RPL0    0
#define RPL1    1
#define RPL2    2
#define RPL3    3

#define TI_GDT  0
#define TI_LDT  1

#define SELECTOR_K_CODE     ((1 << 3) + (TI_GDT << 2) + RPL0)

/* 中断描述符属性 */
#define INTR_DESC_P     1
#define INTR_DESC_DPL0  0
#define INTR_DESC_DPL3  3
#define INTR_DESC_32_TYPE   0xE
#define INTR_DESC_16_TYPE   0x6
#define INTR_DESC_ATTR_DPL0 ((INTR_DESC_P << 7) + (INTR_DESC_DPL0 << 5) + INTR_DESC_32_TYPE)
#define INTR_DESC_ATTR_DPL3 ((INTR_DESC_P << 7) + (INTR_DESC_DPL3 << 5) + INTR_DESC_32_TYPE)

#endif
