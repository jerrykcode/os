[bits 32]
section .text

global switch_to
switch_to:
    ; 此时栈顶为返回地址
    push esi
    push edi
    push ebx
    push ebp

    mov ebx, [esp + 20] ; ebx更新为当前线程task_st结构的地址
    mov [ebx], esp      ; 当前栈指针存入当前线程的self_stack, struct task_st结构的首个属性为self_stack, 见thread.h
    ; 到这里寄存器的值以及当前线程栈esp已经在当前线程中保存, 以后可以还原

    mov ebx, [esp + 24] ; ebx更新为新线程task_st结构的地址
    mov esp, [ebx]      ; esp栈指针更新为新线程的self_stack
    ; 到这里esp已经指向新线程的栈!
    ; 以下pop都是在新线程栈中pop, 寄存器还原为新线程中存储的寄存器值
    pop ebp
    pop ebx
    pop edi
    pop esi

    ret                 ; 返回到新线程
