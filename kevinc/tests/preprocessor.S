; kevinc: C89 to KR32 assembly
; stack-machine code generation, KR32 ILP32 ABI
    .text


    .global main
main:
    PUSH R7
    ADJSP #-8
    LEA R7, [SP]
    MOV R0, #1
    PUSH R0
    MOV R0, #2
    POP R1
    ADD R0, R1, R0
    PUSH R0
    MOV R0, #1
    PUSH R0
    MOV R0, #2
    POP R1
    ADD R0, R1, R0
    POP R1
    MUL R0, R1, R0
    STORE R0, [R7 + 0]
    LOAD R0, [R7 + 0]
    PUSH R0
    LEA R0, [mycounter]
    LOAD R0, [R0]
    POP R1
    ADD R0, R1, R0
    JMP .Lreturn_0
.Lreturn_0:
    ADJSP #8
    POP R7
    RET
    .data
    .align 4
    .global mycounter
mycounter:
    .word 0x00000000
    .align 4
    .global message
message:
    .word .Lstr_0
    .align 4
.Lstr_0:
    .byte 104, 101, 108, 108, 111, 32, 112, 114, 101, 112, 114, 111, 99, 101, 115, 115
    .byte 111, 114, 0
