%include "boot.inc"

section loader vstart=LOADER_BASE_ADDR
LOADER_STACK_TOP    equ LOADER_BASE_ADDR

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

; total_mem_bytes用于保存内存容量，以字节为单位
total_mem_bytes dd 0

; gdt指针

gdt_ptr dw GDT_LIMIT
        dd GDT_BASE

; ARDS结构，20字节
ards_buf times 20 db 0


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

; ---------------- 检测内存容量

; 尝试使用BIOS中断0x15子功能0xe820获取内存容量
	xor ebx, ebx		; 首次调用时ebx置0
	mov edx, 0x534d4150 ; edx固定签名标记
	mov di, ards_buf    ; ARDS结构存储位置
    xor esi, esi        ; esi存储最大内存容量，初始化为0
.e820_mem_get_loop:
    mov eax, 0x0000e820 ; 更新eax为子功能号
    mov ecx, 20         ; ARDS结构20字节
    int 0x15
    jc .e820_failed_then_try_e801   ;若cf位为1则发生了错误，尝试0xe801
    mov ecx, ards_buf
    mov eax, [ecx]      ; base_add_low
    add eax, [ecx + 8]  ; length_low
    cmp esi, eax        ; 比较大小
    jge .continue_e820_mem_get_loop
    mov esi, eax        ; 更新esi
.continue_e820_mem_get_loop:
    cmp ebx, 0          ;ebx为0说明已经是最后一个，结束循环
    jnz .e820_mem_get_loop
; .e280_mem_get_loop结束
    mov edx, esi        ; 内存容量存储到edx中
    jmp .mem_get_ok     ; e820成功获取内存容量，跳转

; 尝试使用0x15子功能0xe801获取内存容量, 最多可以获取4G
.e820_failed_then_try_e801:
    mov ax, 0xe801
    int 0x15
    jc .e801_failed_then_try_e88
; 计算低15MB内存
    mov cx, 0x400
    mul cx              ; ax中存储低15MB内存容量，以KB为单位，所以要乘以0x400 (1024)
    and eax, 0x0000ffff ; 乘法结果低16位存储在ax中
    shl edx, 16         ; 乘法结果高16位存储在dx中
    or  edx, eax        ; 拼出完整乘法结果
    add edx, 0x100000   ; ax只是15MB，故要加1MB
    mov esi, edx
; 计算16MB以上的内存
    xor eax, eax
    mov ax, bx
    mov ecx, 0x10000    ; 64KB
    mul ecx             ; 32位乘法，默认的被乘数是eax，积为64位，高32位存入edx，低32位存入eax
    add esi, eax        ; 由于此方法只能测出4G以内的内存，故32位eax足够了，edx肯定为0，只加eax便可
    mov edx, esi
    jmp .mem_get_ok

; 尝试使用0x15子功能0x88获取内存容量，最多可获取64MB
.e801_failed_then_try_e88:
    mov ah, 0x88
    int 0x15
    jc .error_hlt
    mov cx, 0x400
    mul cx
    and eax, 0x0000ffff
    shl edx, 16
    or  edx, eax
    add edx, 0x100000

.mem_get_ok:
    mov [total_mem_bytes], edx

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
.error_hlt:
    hlt

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
