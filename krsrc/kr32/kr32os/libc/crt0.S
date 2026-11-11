.equ SYSCALL_EXIT, 64

    .text
    .global _start
_start:
    CALL main
    SYSCALL #SYSCALL_EXIT
    HALT
