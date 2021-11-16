[bits 32]

extern main
extern printf
extern exit

section .text
global _start

_start:
    push ebx ; 压入argv
    push ecx ; 压入arg
    call main
    add esp, 8
   
   ; gcc规定main的返回值存储在eax中
    push eax
    call exit
