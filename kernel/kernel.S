[bits 32]
%define ERROR_CODE  nop
%define ZERO        push 0

extern put_str
extern intr_handler_table

section .data
intr_msg db "interrupt occur!", 0xa, 0

%macro INTR_VECTOR 2 ; 写入中断向量
section .text
intr%1entry:
    %2

    push ds
    push es
    push fs
    push gs
    pushad

    push intr_msg
    call put_str
    add esp, 4

    push %1
    call [intr_handler_table + %1*4]
    add esp, 4

    mov al, 0x20
    out 0xa0, al
    out 0x20, al

    popad
    pop gs
    pop fs
    pop es
    pop ds

    add esp, 4
    iret
section .data
    dd intr%1entry
%endmacro

global intr_entry_table
intr_entry_table:
INTR_VECTOR 0x00, ZERO
INTR_VECTOR 0x01, ZERO
INTR_VECTOR 0x02, ZERO
INTR_VECTOR 0x03, ZERO
INTR_VECTOR 0x04, ZERO
INTR_VECTOR 0x05, ZERO
INTR_VECTOR 0x06, ZERO
INTR_VECTOR 0x07, ZERO
INTR_VECTOR 0x08, ERROR_CODE
INTR_VECTOR 0x09, ZERO
INTR_VECTOR 0x0a, ERROR_CODE
INTR_VECTOR 0x0b, ERROR_CODE
INTR_VECTOR 0x0c, ZERO
INTR_VECTOR 0x0d, ERROR_CODE
INTR_VECTOR 0x0e, ERROR_CODE
INTR_VECTOR 0x0f, ZERO
INTR_VECTOR 0x10, ZERO
INTR_VECTOR 0x11, ERROR_CODE
INTR_VECTOR 0x12, ZERO
INTR_VECTOR 0x13, ZERO
INTR_VECTOR 0x14, ZERO
INTR_VECTOR 0x15, ZERO
INTR_VECTOR 0x16, ZERO
INTR_VECTOR 0x17, ZERO
INTR_VECTOR 0x18, ERROR_CODE
INTR_VECTOR 0x19, ZERO
INTR_VECTOR 0x1a, ERROR_CODE
INTR_VECTOR 0x1b, ERROR_CODE
INTR_VECTOR 0x1c, ZERO
INTR_VECTOR 0x1d, ERROR_CODE
INTR_VECTOR 0x1e, ERROR_CODE
INTR_VECTOR 0x1f, ZERO
INTR_VECTOR 0x20, ZERO
