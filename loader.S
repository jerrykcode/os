%include "boot.inc"

section loader vstart=LOADER_BASE_ADDR
LOADER_STACK_TOP    equ LOADER_BASE_ADDR
jmp loader_start

; 构建 gdt 及其内部的描述符

GDT_BASE:
    dd  0x00000000
    dd  0x00000000
    ; gdt第0个段无效i，填充0

CODE_DESC:
    dd  DESC_CODE_LOW4  ; 低4字节
    dd  DESC_CODE_HIGH4 ; 高4字节

DATA_STACK_DESC:
    dd  DESC_DATA_LOW4
    dd  DESC_DATA_HIGH4

VIDEO_DESC:
    dd  DESC_VIDEO_LOW4
    dd  DESC_VIDEO_HIGH4

GDT_SIZE    equ $ - GDT_BASE
GDT_LIMIT   equ GDT_SIZE - 1

times 60 dq 0   ; 预留60个描述符的空位

; 选择子
SELECTOR_CODE   equ (0x0001 << 3) + TI_GDT + RPL0
SELECTOR_DATA   equ (0x0002 << 3) + TI_GDT + RPL0
SELECTOR_VIDEO  equ (0x0003 << 3) + TI_GDT + RPL0

; gdt指针

gdt_ptr dw GDT_LIMIT
        dd GDT_BASE



loader_start:

; 在实模式下输出 HELLO LOADER
    mov byte [gs:0x1A], 'H'
    mov byte [gs:0x1B], 0xA9

    mov byte [gs:0x1C], 'E'
    mov byte [gs:0x1D], 0xA9

    mov byte [gs:0x1E], 'L'
    mov byte [gs:0x1F], 0xA9

    mov byte [gs:0x20], 'L'
    mov byte [gs:0x21], 0xA9

    mov byte [gs:0x22], 'O'
    mov byte [gs:0x23], 0xA9

    mov byte [gs:0x24], ' '
    mov byte [gs:0x25], 0xA9

    mov byte [gs:0x26], 'L'
    mov byte [gs:0x27], 0xA9
    
    mov byte [gs:0x28], 'O'
    mov byte [gs:0x29], 0xA9


    mov byte [gs:0x2A], 'A'
    mov byte [gs:0x2B], 0xA9
    
    mov byte [gs:0x2C], 'D'
    mov byte [gs:0x2D], 0xA9

    mov byte [gs:0x2E], 'E'
    mov byte [gs:0x2F], 0xA9

    mov byte [gs:0x30], 'R'
    mov byte [gs:0x31], 0xA9

; 进入保护模式

; --------------- 打开 A20
    in al, 0x92
    or al, 0000_0010b
    out 0x92, al

; --------------- 加载gdt
    lgdt [gdt_ptr]

; --------------- cr0 第0位置1
    mov eax, cr0
    or eax, 0x00000001
    mov cr0, eax
     
    jmp dword SELECTOR_CODE:p_mode_start ; 刷新流水线

[bits 32]
p_mode_start:
    mov ax, SELECTOR_DATA
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, LOADER_STACK_TOP
    mov ax, SELECTOR_VIDEO
    mov gs, ax
    
    mov byte [gs:160], 'P'

    jmp $
