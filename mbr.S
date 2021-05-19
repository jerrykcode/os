;主引导程序
;------------------------------------------------------------
%include "boot.inc"

SECTION MBR vstart=0x7c00
    mov ax, cs
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov sp, 0x7c00
    mov ax, 0xb800
    mov gs, ax

; 清屏利用 0x06 号功能,上卷全部行, 则可清屏 。
; -----------------------------------------------------------
;INT 0x10 功能号:0x06 功能描述:上卷窗口
;------------------------------------------------------
;输入:
;AH 功能号= 0x06
;AL = 上卷的行数(如果为 0,表示全部)
;BH = 上卷行属性
;(CL,CH) = 窗口左上角的(X,Y)位置
;(DL,DH) = 窗口右下角的(X,Y)位置
;无返回值:
    mov ax, 0x600
    mov bx, 0x700
    mov cx, 0
    mov dx, 0x184f

    int 0x10

; 获取光标位置

    mov ah, 3
    mov bh, 0

    int 0x10

; 输出

    mov byte [gs:0x00], 'H'
    mov byte [gs:0x01], 0xA4

    mov byte [gs:0x02], 'E'
    mov byte [gs:0x03], 0xA4

    mov byte [gs:0x04], 'L'
    mov byte [gs:0x05], 0xA4

    mov byte [gs:0x06], 'L'
    mov byte [gs:0x07], 0xA4

    mov byte [gs:0x08], 'O'
    mov byte [gs:0x09], 0xA4

    mov byte [gs:0x0A], ' '
    mov byte [gs:0x0B], 0xA4

    mov byte [gs:0x0C], 'M'
    mov byte [gs:0x0D], 0xA4
    
    mov byte [gs:0x0E], 'B'
    mov byte [gs:0x0F], 0xA4


    mov byte [gs:0x10], 'R'
    mov byte [gs:0x11], 0xA4

; 加载内核加载器loader

    mov eax, LOADER_START_SECTOR    ; loader扇区地址 
    mov bx, LOADER_BASE_ADDR        ; loader载入内存位置
    mov cx, 4                       ; 待读入扇区数
    call rd_disk_m_16               ; 调用读取扇区函数

    jmp LOADER_BASE_ADDR            ; 跳转到loader代码


; --------------------------------------------------------
; 读取硬盘扇区
; 参数:
; eax: LBA扇区号
; bx:  载入内存位置
; cx:  待读入扇区数

rd_disk_m_16:

    mov esi, eax    ; 备份eax
    mov di, cx      ; 备份cx
    
    mov dx, 0x1f2   ; sector count
    mov al, cl
    out dx, al
    
    mov eax, esi    ; 还原eax

    mov dx, 0x1f3   ; LBA low
    out dx, al

    mov dx, 0x1f4   ; LBA mid
    mov cl, 8
    shr eax, cl     ; 右移8位
    out dx, al

    mov dx, 0x1f5   ; LBA high
    shr eax, cl     ; 右移8位
    out dx, al

    mov dx, 0x1f6   ; Device
    shr eax, cl     ; 右移8位
    and al, 0x0f    ; 保留al的低4位
    or al, 0xe0     ; 设置al的高4位为1110b
    out dx, al

    mov dx, 0x1f7   ; Command    
    mov al, 0x20
    out dx, al

; 检测硬盘状态
.not_ready:
    nop
    in al, dx
    and al, 0x08
    cmp al, 0x08
    jnz .not_ready

; 载入内存

    mov ax, di      ; 扇区数
    mov dx, 0x100
    mul dx          ; 扇区数*256, 循环次数
    mov cx, ax
    
    mov dx, 0x1f0   ; Data
.read:
    in ax, dx
    mov [bx], ax
    add bx, 2
    loop .read

    ret             ; 返回
    
    times 510 - ($ - $$) db 0

    db 0x55, 0xaa
