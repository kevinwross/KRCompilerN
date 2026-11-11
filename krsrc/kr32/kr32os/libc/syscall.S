    .equ SYSCALL_EXIT, 64
.equ SYSCALL_WRITE, 65
.equ SYSCALL_READ, 66
.equ SYSCALL_YIELD, 67
.equ SYSCALL_GETPID, 68
.equ SYSCALL_BRK, 69

    .text
    .global sys_exit
sys_exit:
    SYSCALL #SYSCALL_EXIT
    RET

    .global sys_write
sys_write:
    SYSCALL #SYSCALL_WRITE
    RET

    .global sys_read
sys_read:
    SYSCALL #SYSCALL_READ
    RET

    .global sys_yield
sys_yield:
    SYSCALL #SYSCALL_YIELD
    RET

    .global sys_getpid
sys_getpid:
    SYSCALL #SYSCALL_GETPID
    RET

    .global sys_brk
sys_brk:
    SYSCALL #SYSCALL_BRK
    RET
