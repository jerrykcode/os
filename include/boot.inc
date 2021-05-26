;---------------------- loader和kernel -------------------------

LOADER_BASE_ADDR equ 0x900  ; loader载入内存的位置
LOADER_START_SECTOR equ 0x2 ; loader在硬盘上的逻辑扇区地址

KERNEL_START_SECTOR equ 0x9 ; kernel.bin在硬盘上的逻辑扇区地址
KERNEL_BIN_BASE_ADDR equ 0x70000 ; kernel载入内存的位置

; kernel.bin ELF格式中的属性
KERNEL_E_PHOFF_ADDR      equ KERNEL_BIN_BASE_ADDR + 28
KERNEL_E_PHENTSIZE_ADDR  equ KERNEL_BIN_BASE_ADDR + 42
KERNEL_E_PHNUM_ADDR      equ KERNEL_BIN_BASE_ADDR + 44

KERNEL_ENTRY_POINT  equ 0xc0001500 ;

PAGE_DIR_TABLE_POS  equ 0x100000

;----------------------- gdt 描述符属性 ------------------------
DESC_G_4K           equ 10000000_00000000_00000000b ; 23位
DESC_D_32           equ  1000000_00000000_00000000b ; 22
DESC_L              equ   000000_00000000_00000000b ; 21
DESC_AVL            equ    00000_00000000_00000000b ; 20

DESC_LIMIT_CODE2    equ     1111_00000000_00000000b ; 19~16
DESC_LIMIT_DATA2    equ DESC_LIMIT_CODE2            ; 19~16
DESC_LIMIT_VIDEO2   equ     0000_00000000_00000000b ; 19~16

DESC_P              equ          10000000_00000000b ; 15

DESC_DPL_0          equ           0000000_00000000b ; 14~13
DESC_DPL_1          equ           0100000_00000000b ; 14~13
DESC_DPL_2          equ           1000000_00000000b ; 14~13
DESC_DPL_3          equ           1100000_00000000b ; 14~13

DESC_S_CODE         equ             10000_00000000b ; 12
DESC_S_DATA         equ             DESC_S_CODE     ; 12
DESC_S_SYS          equ             00000_00000000b ; 12

DESC_TYPE_CODE      equ              1000_00000000b ; 11~8
                                    ; x = 1, c = 0, r = 0, a = 0
DESC_TYPE_DATA      equ              0010_00000000b ; 11~8
                                    ; x = 0, e = 0, w = 1, a = 0
; -----------------------
; 段描述符 高32位(4字节) :

; 代码段描述符低4字节与高4字节
DESC_CODE_LOW4      equ 0x0000ffff  ; 段基址为0，段界限(0~15位部分)ffff
DESC_CODE_HIGH4     equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + \
                        DESC_L + DESC_AVL + DESC_LIMIT_CODE2 + \
                        DESC_P + DESC_DPL_0 + DESC_S_CODE + \
                        DESC_TYPE_CODE + 0x00
                    ; 平坦模型，段基址(在高32位中第31~24及7~0位)为0

DESC_DATA_LOW4      equ 0x0000ffff
DESC_DATA_HIGH4     equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + \
                        DESC_L + DESC_AVL + DESC_LIMIT_DATA2 + \
                        DESC_P + DESC_DPL_0 + DESC_S_DATA + \
                        DESC_TYPE_DATA + 0x00

DESC_VIDEO_LOW4     equ 0x8000_0007
DESC_VIDEO_HIGH4    equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + \
                        DESC_L + DESC_AVL + DESC_LIMIT_VIDEO2 + \
                        DESC_P + DESC_DPL_0 + DESC_S_DATA + \
                        DESC_TYPE_DATA + 0x0b

; ---------------------------- 选择子属性 ----------------------------
RPL0                equ 00b
RPL1                equ 01b
RPL2                equ 10b
RPL3                equ 11b
TI_GDT              equ 000b
TI_LDT              equ 100b

; ----------------------------- 页表属性 -----------------------------
PG_P                equ 1b
PG_RW_R             equ 00b
PG_RW_W             equ 10b
PG_US_S             equ 000b
PG_US_U             equ 100b

; ----------------------------- program type 定义 --------------------
PT_NULL             equ 0
