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

; ---------------------------------------------
; 在实模式下输出 HELLO LOADER
; ---------------------------------------------
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

; ---------------------------------------------
; 检测内存容量
; ---------------------------------------------


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

; ---------------------------------------------
; 进入保护模式
; ---------------------------------------------

; 打开 A20
    in al, 0x92
    or al, 0000_0010b
    out 0x92, al

; 加载gdt
    lgdt [gdt_ptr]

; cr0 第0位置1
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

; ---------------------------------------------
; 加载内核
; ---------------------------------------------
    mov eax, KERNEL_START_SECTOR    ; kernel.bin所在的扇区号
    mov ebx, KERNEL_BIN_BASE_ADDR   ; kernel载入的内存位置
    mov ecx, 200                    ; 读入的扇区数

    call rd_disk_m_32

; ---------------------------------------------
; 创建页目录及页表
; ---------------------------------------------
    call setup_page

    sgdt [gdt_ptr]
    mov ebx, [gdt_ptr + 2]
    or dword [ebx + 0x18 + 4], 0xc0000000
    add dword [gdt_ptr + 2], 0xc0000000
    add esp, 0xc0000000

; 把页目录地址赋值给cr3
    mov eax, PAGE_DIR_TABLE_POS
    mov cr3, eax

; 打开cr0的pg位(第31位)
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

; 开启分页后，用新的gdt地址重新加载
    lgdt [gdt_ptr]

    mov byte [gs:162], 'V'

; ---------------------------------------------
; 初始化内核
; ---------------------------------------------
    jmp SELECTOR_CODE:enter_kernel
enter_kernel:
    call kernel_init
    mov esp, 0xc009f000
    jmp KERNEL_ENTRY_POINT

; ---------------------------------------------
; 以下是 rd_disk_m_32, setup_page 以及 kernel_init 函数的实现
; ---------------------------------------------

; ------------------ 32位读取硬盘函数
; 参数: eax LBA扇区号
;       ebx 数据写入内存的位置
;       ecx 读入的扇区数

rd_disk_m_32:

    mov esi, eax    ; 备份eax
    mov di, cx      ; 备份cx

; 设置读入的扇区数
    mov dx, 0x1f2
    mov al, cl
    out dx, al      ; 将al(读入的扇区数)存入0x1f2

    mov eax, esi    ; 还原eax

; 将LBA地址存入0x1f3~0x1f6
    mov dx, 0x1f3
    out dx, al

    mov dx, 0x1f4
    mov cl, 8
    shr eax, cl
    out dx, al

    mov dx, 0x1f5
    shr eax, cl
    out dx, al

    mov dx, 0x1f6
    shr eax, cl
    and al, 0x0f    ; 取al的低4位，即LBA地址的第24~27位
    or al, 0xe0     
    out dx, al

; 将读命令0x20 写入0x1f7
    mov dx, 0x1f7
    mov al, 0x20
    out dx, al

; 检测硬盘状态
.not_ready:
    nop
    in al, dx
    and al, 0x08
    cmp al, 0x08
    jnz .not_ready

; 从0x1f0读数据
    mov ax, di
    mov dx, 0x100
    mul dx
    mov cx, ax
    mov dx, 0x1f0
.read_loop:
    in ax, dx
    mov [ebx], ax
    add ebx, 2
    loop .read_loop
    ret


; ------------------ 创建页目录及页表函数

setup_page:

; 为页目录清空空间(4096字节)
    mov cx, 0x1000
    mov esi, 0
.clear_page_dir:
    mov byte [PAGE_DIR_TABLE_POS + esi], 0
    inc esi
    loop .clear_page_dir

; 创建页目录项PDE
    mov eax, PAGE_DIR_TABLE_POS
    add eax, 0x1000
    mov ebx, eax                            ; ebx记录首个页表的位置

    or eax, PG_US_U | PG_RW_W | PG_P
    mov [PAGE_DIR_TABLE_POS], eax           ; 页目录表中的首个页目录项，指向eax
    mov [PAGE_DIR_TABLE_POS + 0xc00], eax   ; 0xc00以上的目录项用于内核空间
    sub eax, 0x1000                         ; 此时eax为页目录表的基址
    mov [PAGE_DIR_TABLE_POS + 0xffc], eax   ; 页目录表中最后一个页目录指向eax, 即页目录表本身的基址
   
; 创建首个页表PTE(地址PAGE_DIR_TABLE_POS + 0x1000)
    mov cx, 256                             ; 每页4KB, 256页表达1MB低端内存
    mov edx, PG_US_U | PG_RW_W | PG_P
.create_pte:
    mov [ebx], edx
    add ebx, 4
    add edx, 0x1000
    loop .create_pte

; 创建内核其他页表的PDE
    mov cx, 254
    mov ebx, PAGE_DIR_TABLE_POS
    mov edx, ebx
    add ebx, 0xc04
    add edx, 0x2000
    or edx, PG_US_U | PG_RW_W | PG_P
.create_pde:
    mov [ebx], edx
    add ebx, 4
    add edx, 0x1000
    loop .create_pde

    ret

; --------------------- 初始化内核函数
; 将kernel.bin中的segement拷贝到编译的地址

kernel_init:
    xor eax, eax
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx

    mov ebx, [KERNEL_E_PHOFF_ADDR]      ; kernel.bin的e_phoff, 表示首个program header在文件中的偏移量
    add ebx, KERNEL_BIN_BASE_ADDR       ; 加上文件起始地址
    mov edx, [KERNEL_E_PHENTSIZE_ADDR]  ; e_phentsize, 表示program header大小
    mov ecx, [KERNEL_E_PHNUM_ADDR]      ; e_phnum, 表示program header的数量
.each_segment:
    cmp byte [ebx], PT_NULL
    je .prepare_next_segment
    ; 为调用mem_cpy函数准备参数
    push dword [ebx + 16]               ; program header中偏移16字节存储的是p_filesz
    mov eax, [ebx + 4]                  ; 偏移4字节的地方是p_offset
    add eax, KERNEL_BIN_BASE_ADDR       ; eax的值为p_offset + kernel.bin文件起始地址
    push eax                            ; 压入eax
    push dword [ebx + 8]                ; program header中偏移8字节存储的是p_vaddr
    ; 此时栈中已压入了mem_cpy的3个参数 size, 源地址，目的地址
    call mem_cpy
    add esp, 12                         ; 清理栈

.prepare_next_segment:
    add ebx, edx
    loop .each_segment
    ret

; ----------------- mem_cpy函数

mem_cpy:
    cld
    push ebp            ; 备份ebp
    push ecx            ; 备份ecx
    mov ebp, esp
    mov edi, [ebp + 12] ; 目的地址
    mov esi, [ebp + 16] ; 源地址
    mov ecx, [ebp + 20] ; size

    rep movsb

    pop ecx             ; 还原ecx
    pop ebp             ; 还原ebp
    ret
