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

;    push intr_msg
;    call put_str
;    add esp, 4

    ; 发送中断结束命令EOI
    mov al, 0x20
    out 0xa0, al
    out 0x20, al

    push %1
    call [intr_handler_table + %1*4]
    add esp, 4

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
INTR_VECTOR 0x20, ZERO	;时钟中断对应的入口
INTR_VECTOR 0x21, ZERO	;键盘中断对应的入口
INTR_VECTOR 0x22, ZERO	;级联用的
INTR_VECTOR 0x23, ZERO	;串口2对应的入口
INTR_VECTOR 0x24, ZERO	;串口1对应的入口
INTR_VECTOR 0x25, ZERO	;并口2对应的入口
INTR_VECTOR 0x26, ZERO	;软盘对应的入口
INTR_VECTOR 0x27, ZERO	;并口1对应的入口
INTR_VECTOR 0x28, ZERO	;实时时钟对应的入口
INTR_VECTOR 0x29, ZERO	;重定向
INTR_VECTOR 0x2a, ZERO	;保留
INTR_VECTOR 0x2b, ZERO	;保留
INTR_VECTOR 0x2c, ZERO	;ps/2鼠标
INTR_VECTOR 0x2d, ZERO	;fpu浮点单元异常
INTR_VECTOR 0x2e, ZERO	;硬盘
INTR_VECTOR 0x2f, ZERO	;保留

; 单独的退出中断函数
section .text
global intr_exit
intr_exit:
    add esp, 4
    popad
    pop gs
    pop fs
    pop es
    pop ds
    add esp, 4
    iret

; 0x80中断
; 系统调用
extern syscall_table
global syscall_handler
syscall_handler:
    ZERO                ; 压入0，统一格式
    push ds
    push es
    push fs
    push gs
    pushad
    push 0x80           ; 压入中断号

    ; 压入系统调用参数
    push edx
    push ecx
    push ebx
    ; 调用系统调用函数
    ; eax存储了系统调用序号
    call [syscall_table + eax * 4]

    add esp, 12
    ; eax存储的是系统调用的返回值
    ; 将eax的值存储在栈中之前push eax的位置
    ; 这样之后pop出eax时，就使得eax的值是系统调用的返回值
    mov [esp + 4 + 7*4], eax

    jmp intr_exit
