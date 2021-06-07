TI_GDT  equ 0
RPL0    equ 0
SELECTOR_VIDEO  equ (0x0003<<3) + TI_GDT + RPL0

[bits 32]
section .text

; ----------------------------------------------------
; put_char函数
; 功能 打印单个字符
; ----------------------------------------------------
global put_char
put_char:
    ; 保存寄存器 入栈
    push eax
    push ebx
    push ecx
    push edx
    push ebp
    push esi
    push edi

    ; 加载选择子
    mov ax, SELECTOR_VIDEO
    mov gs, ax

    mov ebp, esp

    ; 读取光标座标高8位
    mov dx, 0x03d4
    mov al, 0x0e
    out dx, al
    mov dx, 0x03d5
    in al, dx
    mov ah, al

    ; 读取光标座标低8位
    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5
    in al, dx

    ; 光标座标存入bx
    mov bx, ax

    mov dl, [ebp + 32]      ;获取参数
    
    ; 若为控制字符则跳转到相应的处理程序
    cmp dl, 0x08
    jz .back_space

    cmp dl, 0x0a
    jz .line_feed

    cmp dl, 0x0d
    jz .carriage_return

    ; 处理非控制字符
    ; 在光标处写入字符
    shl bx, 1
    mov byte [gs:bx], dl
    inc bx
    mov byte [gs:bx], 0x07
    shr bx, 1

    inc bx                  ; 光标后移
    cmp bx, 2000
    jl .set_cursor          ; 光标小于2000
    jmp .roll_screen        ; 光标不小于2000，需要滚屏

    ; 处理控制字符
.back_space:
    dec bx                  ; 光标前移
    ; 清空光标处字符
    shl bx, 1
    mov byte [gs:bx], 0x00
    inc bx
    mov byte [gs:bx], 0x07
    shr bx, 1

    jmp .set_cursor

.line_feed:
.carriage_return:
    ; \n 和 回车 均为换行
    xor edx, edx
    mov ax, bx
    mov cx, 80
    div cx                  ; 光标座标 / 80 ---> 商(ax)为光标所在行，余数(dx)为光标所在列
    sub bx, dx              ; 光标更新为所在行的第0列
    add bx, 80              ; 光标更新为下一行的第0列
    cmp bx, 2000
    jl .set_cursor

.roll_screen:
    ; 滚屏
    cld
    mov ecx, 960            ; 用第[1, 24]行数据覆盖[0, 23]行，传输数据量 (24*80) * 2 = 3840字节，每次mov4字节，需要3840 / 4 = 960次
    mov esi, 0xb80a0        ; 源地址: 第1行第0列
    mov edi, 0xb8000        ; 目的地址: 第0行第0列
   
    rep movsb

    ; 清空第24行(最后一行)
    mov ecx, 80
    mov ebx, 1920
.clear_last_row:
    mov byte [gs:ebx], 0x00
    inc ebx
    mov byte[gs:ebx], 0x07
    inc ebx
    loop .clear_last_row

    mov bx, 1920            ; 光标定位到第24行第0列

.set_cursor:
    ; 设置光标座标到显卡端口
    mov dx, 0x3d4
    mov al, 0x0e
    out dx, al
    mov dx, 0x3d5
    mov al, bh
    out dx, al

    mov dx, 0x3d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5
    mov al, bl
    out dx, al

    pop edi
    pop esi
    pop ebp
    pop edx
    pop ecx
    pop ebx
    pop eax

    ret


; ----------------------------------------------------
; put_str函数
; 功能 打印字符串
; ----------------------------------------------------
global put_str
put_str:
    push ebx
    push edx
    push ebp
    mov ebp, esp
    
    mov ebx, [ebp + 16]
    xor eax, eax
    xor edx, edx
.each_char:
    mov dl, [ebx]
    cmp dl, 0x00
    jz .end
    push edx
    call put_char
    add esp, 4
    inc ebx
    jmp .each_char
.end:
    pop ebp
    pop edx
    pop ebx
    ret
