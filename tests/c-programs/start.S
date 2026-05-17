    .section .text.init
    .globl _start
_start:
    la sp, _stack_top
    call main
    li a7, 0          # ECALL to stop simulation
    ecall
    j .

    .section .bss
    .align 4
    .space 4096
_stack_top:
