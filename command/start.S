[bits 32]

extern main
section .text
global _start

_start:
   push ebx ; 压入argv
   push ecx ; 压入argc
   call main
