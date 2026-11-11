.file "kr32os/kernel/user-asm.S"

.equ VECTOR_TABLE_BASE, 0x00050000
.equ SYSCALL_FIRST_VECTOR, 32
.equ SYSCALL_TABLE_OFFSET, 128
.equ SYSCALL_VECTOR_COUNT, 224
.equ SYSCALL_EXIT_OFFSET, 256
.equ SYSCALL_WRITE_OFFSET, 260
.equ SYSCALL_READ_OFFSET, 264
.equ SYSCALL_YIELD_OFFSET, 268
.equ SYSCALL_GETPID_OFFSET, 272
.equ SYSCALL_BRK_OFFSET, 276
.equ SYSCALL_READ_VECTOR, 66
.equ SYSCALL_YIELD_VECTOR, 67
.equ SYSCALL_GETPID_VECTOR, 68
.equ SYSCALL_BRK_VECTOR, 69
.equ SYSCALL_SCRATCH_PHYS, 0x0006F000
.equ SYSCALL_VECTOR_ADDR, 0x0006F004
.equ KERNEL_STACK_TOP, 0x00100000
.equ USER_TRAP_STACK_TOP, 0x000E0000
.equ USER_MODE_FLAGS, 0x10
.equ SAVED_REG_BYTES, 64
.equ SAVED_R0, 0
.equ SAVED_R1, 4
.equ SAVED_R2, 8
.equ SAVED_R3, 12
.equ SAVED_R4, 16
.equ SAVED_R5, 20
.equ SAVED_R6, 24
.equ SAVED_R7, 28
.equ SAVED_R8, 32
.equ SAVED_R9, 36
.equ SAVED_R10, 40
.equ SAVED_R11, 44
.equ SAVED_R12, 48
.equ SAVED_R13, 52
.equ SAVED_R14, 56
.equ SAVED_R15, 60
.equ IRET_ENTRY, 0
.equ IRET_FLAGS, 4
.equ IRET_STACK, 8
.equ IRET_RESERVED, 12

; Kernel/user context bridge and syscall entry stubs.
    .section .bss
    .align 4
__kernel_saved_sp:
    .space 4
__kernel_saved_regs:
    .space SAVED_REG_BYTES

    .text
    .global initrd_address
initrd_address:
    LEA R0, [__initrd_start]
    RET

    .global user_install_syscalls
user_install_syscalls:
    ; Every unused software vector traps via BRK instead of jumping to zero.
    ; The implemented user syscall entries overwrite their slots below.
    MOV R3, #SYSCALL_FIRST_VECTOR
    MOV R4, #VECTOR_TABLE_BASE
    ADD R4, R4, #SYSCALL_TABLE_OFFSET ; vector 32 * 4
    MOV R5, #SYSCALL_VECTOR_COUNT     ; vectors 32..255
    LEA R6, [syscall_unimplemented_stub]
.Lsyscall_fill:
    STORE R6, [R4]
    ADD R4, R4, #4
    SUB R5, R5, #1
    JNZ .Lsyscall_fill

    MOV R1, #VECTOR_TABLE_BASE
    LEA R2, [syscall_exit_stub]
    STORE R2, [R1 + SYSCALL_EXIT_OFFSET]
    LEA R2, [syscall_write_stub]
    STORE R2, [R1 + SYSCALL_WRITE_OFFSET]
    LEA R2, [syscall_read_stub]
    STORE R2, [R1 + SYSCALL_READ_OFFSET]
    LEA R2, [syscall_yield_stub]
    STORE R2, [R1 + SYSCALL_YIELD_OFFSET]
    LEA R2, [syscall_getpid_stub]
    STORE R2, [R1 + SYSCALL_GETPID_OFFSET]
    LEA R2, [syscall_brk_stub]
    STORE R2, [R1 + SYSCALL_BRK_OFFSET]
    RET

    .global kernel_enter_user
kernel_enter_user:
    ; Save all kernel registers, then the kernel stack (which holds the
    ; return address of the C call).
    MOV R15, #__kernel_saved_regs
    STORE R0, [R15 + SAVED_R0]
    STORE R1, [R15 + SAVED_R1]
    STORE R2, [R15 + SAVED_R2]
    STORE R3, [R15 + SAVED_R3]
    STORE R4, [R15 + SAVED_R4]
    STORE R5, [R15 + SAVED_R5]
    STORE R6, [R15 + SAVED_R6]
    STORE R7, [R15 + SAVED_R7]
    STORE R8, [R15 + SAVED_R8]
    STORE R9, [R15 + SAVED_R9]
    STORE R10, [R15 + SAVED_R10]
    STORE R11, [R15 + SAVED_R11]
    STORE R12, [R15 + SAVED_R12]
    STORE R13, [R15 + SAVED_R13]
    STORE R14, [R15 + SAVED_R14]
    STORE R15, [R15 + SAVED_R15]
    LEA R12, [SP]
    STORE R12, [__kernel_saved_sp]

    ; User->kernel traps use a dedicated kernel stack so they cannot overwrite
    ; the saved shell context.
    MOV R12, #USER_TRAP_STACK_TOP
    WRCR CR6, R12

    ; R0 = user entry, R1 = user stack. Build the IRET frame.
    ADJSP #-16
    STORE R0, [SP + IRET_ENTRY]
    MOV R2, #USER_MODE_FLAGS     ; user mode, interrupts enabled
    STORE R2, [SP + IRET_FLAGS]
    STORE R1, [SP + IRET_STACK]
    MOV R2, #0
    STORE R2, [SP + IRET_RESERVED]
    IRET

    .global kernel_leave_user
kernel_leave_user:
    ; Restore the kernel stack and registers, then return to the C caller.
    MOV R12, #KERNEL_STACK_TOP
    WRCR CR6, R12
    MOV R15, #__kernel_saved_regs
    LOAD R0, [R15 + SAVED_R0]
    LOAD R2, [R15 + SAVED_R2]
    LOAD R3, [R15 + SAVED_R3]
    LOAD R4, [R15 + SAVED_R4]
    LOAD R5, [R15 + SAVED_R5]
    LOAD R6, [R15 + SAVED_R6]
    LOAD R7, [R15 + SAVED_R7]
    LOAD R8, [R15 + SAVED_R8]
    LOAD R9, [R15 + SAVED_R9]
    LOAD R10, [R15 + SAVED_R10]
    LOAD R11, [R15 + SAVED_R11]
    LOAD R13, [R15 + SAVED_R13]
    LOAD R14, [R15 + SAVED_R14]
    LOAD R1, [__kernel_saved_sp]
    SETSP R1
    LOAD R1, [R15 + SAVED_R1]
    LOAD R12, [R15 + SAVED_R12]
    LOAD R15, [R15 + SAVED_R15]
    RET

    .global syscall_write_stub
syscall_write_stub:
    PUSHR
    CALL sys_write
    POPR
    IRET

    .global syscall_exit_stub
syscall_exit_stub:
    MOV R2, #SYSCALL_SCRATCH_PHYS
    STORE R0, [R2]
    JMP kernel_leave_user

syscall_read_stub:
    MOV R12, #SYSCALL_READ_VECTOR
    STORE R12, [SYSCALL_VECTOR_ADDR]
    JMP syscall_common

syscall_yield_stub:
    MOV R12, #SYSCALL_YIELD_VECTOR
    STORE R12, [SYSCALL_VECTOR_ADDR]
    JMP syscall_common

syscall_getpid_stub:
    MOV R12, #SYSCALL_GETPID_VECTOR
    STORE R12, [SYSCALL_VECTOR_ADDR]
    JMP syscall_common

syscall_brk_stub:
    MOV R12, #SYSCALL_BRK_VECTOR
    STORE R12, [SYSCALL_VECTOR_ADDR]
    JMP syscall_common

syscall_common:
    PUSHR
    CALL sys_dispatch
    POPR
    IRET

syscall_unimplemented_stub:
    BRK
