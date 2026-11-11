.file "kr32os/kernel/traps-asm.S"

.equ VECTOR_TABLE_BASE, 0x00050000
.equ VECTOR_INVALID_OPCODE, 4
.equ VECTOR_DIVIDE_ZERO, 8
.equ VECTOR_BREAKPOINT, 12
.equ VECTOR_PAGE_FAULT, 16
.equ VECTOR_GENERAL_PROTECTION, 20
.equ VECTOR_ALIGNMENT, 24
.equ VECTOR_DOUBLE_FAULT, 28
.equ VECTOR_TIMER, 64
.equ TRAP_INVALID_OPCODE, 1
.equ TRAP_DIVIDE_ZERO, 2
.equ TRAP_BREAKPOINT, 3
.equ TRAP_PAGE_FAULT, 4
.equ TRAP_GENERAL_PROTECTION, 5
.equ TRAP_ALIGNMENT, 6
.equ TRAP_DOUBLE_FAULT, 7

; KR32OS M3 trap entry stubs and vector installation.
    .text

    .global traps_install
traps_install:
    MOV R1, #VECTOR_TABLE_BASE
    LEA R2, [trap_invalid_opcode]
    STORE R2, [R1 + VECTOR_INVALID_OPCODE]
    LEA R2, [trap_divide_zero]
    STORE R2, [R1 + VECTOR_DIVIDE_ZERO]
    LEA R2, [trap_breakpoint]
    STORE R2, [R1 + VECTOR_BREAKPOINT]
    LEA R2, [trap_page_fault]
    STORE R2, [R1 + VECTOR_PAGE_FAULT]
    LEA R2, [trap_general_protection]
    STORE R2, [R1 + VECTOR_GENERAL_PROTECTION]
    LEA R2, [trap_alignment]
    STORE R2, [R1 + VECTOR_ALIGNMENT]
    LEA R2, [trap_double_fault]
    STORE R2, [R1 + VECTOR_DOUBLE_FAULT]
    LEA R2, [trap_timer]
    STORE R2, [R1 + VECTOR_TIMER]
    RET

    .global kernel_enable_interrupts
kernel_enable_interrupts:
    MOV R1, #1
    WRCR CR0, R1
    RET

trap_invalid_opcode:
    MOV R0, #TRAP_INVALID_OPCODE
    LEA R1, [SP]
    CALL trap_dispatch
    MOV R0, #1
    HALT

trap_divide_zero:
    MOV R0, #TRAP_DIVIDE_ZERO
    LEA R1, [SP]
    CALL trap_dispatch
    MOV R0, #1
    HALT

trap_breakpoint:
    MOV R0, #TRAP_BREAKPOINT
    LEA R1, [SP]
    CALL trap_dispatch
    MOV R0, #1
    HALT

trap_page_fault:
    MOV R0, #TRAP_PAGE_FAULT
    LEA R1, [SP]
    CALL trap_dispatch
    MOV R0, #1
    HALT

trap_general_protection:
    MOV R0, #TRAP_GENERAL_PROTECTION
    LEA R1, [SP]
    CALL trap_dispatch
    MOV R0, #1
    HALT

trap_alignment:
    MOV R0, #TRAP_ALIGNMENT
    LEA R1, [SP]
    CALL trap_dispatch
    MOV R0, #1
    HALT

trap_double_fault:
    MOV R0, #TRAP_DOUBLE_FAULT
    LEA R1, [SP]
    CALL trap_dispatch
    MOV R0, #1
    HALT

trap_timer:
    PUSH R0
    PUSHR
    CALL timer_tick
    POPR
    POP R0
    IRET
