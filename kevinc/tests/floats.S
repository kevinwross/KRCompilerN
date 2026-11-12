; kevinc: C89 to KR32 assembly
; stack-machine code generation, KR32 ILP32 ABI
    .text


    .global main
main:
    PUSH R7
    ADJSP #-24
    LEA R7, [SP]
    LEA R0, [.Lfp_0]
    FLOAD.W F0, [R0]
    FSTORE.W F0, [R7 + 0]
    LEA R0, [.Lfp_1]
    FLOAD.D F0, [R0]
    FSTORE.D F0, [R7 + 8]
    FLOAD.W F0, [R7 + 0]
    ADJSP #-8
    FSTORE.W F0, [SP]
    FLOAD.D F0, [R7 + 8]
    FCVT.S.D F0, F0
    FLOAD.W F1, [SP]
    ADJSP #8
    FADD.S F0, F1, F0
    FSTORE.W F0, [R7 + 16]
    FLOAD.W F0, [R7 + 16]
    FCVT.W.S R0, F0
    STORE R0, [R7 + 20]
    FLOAD.D F0, [R7 + 8]
    ADJSP #-8
    FSTORE.D F0, [SP]
    FLOAD.W F0, [R7 + 0]
    FCVT.D.S F0, F0
    FLOAD.D F1, [SP]
    ADJSP #8
    FLT.D R0, F0, F1
    CMP R0, #0
    JZ .Lif_else_1
    LEA R0, [R7 + 20]
    PUSH R0
    LOAD R0, [R0]
    PUSH R0
    MOV R0, #1
    POP R1
    ADD R0, R1, R0
    POP R1
    STORE R0, [R1]
    JMP .Lif_end_2
.Lif_else_1:
.Lif_end_2:
    LOAD R0, [R7 + 20]
    PUSH R0
    LEA R0, [global_float]
    FLOAD.W F0, [R0]
    FCVT.W.S R0, F0
    POP R1
    ADD R0, R1, R0
    JMP .Lreturn_0
.Lreturn_0:
    ADJSP #24
    POP R7
    RET
    .data
    .align 4
    .global global_float
global_float:
    .word 0x3FC00000
    .align 8
    .global global_double
global_double:
    .word 0x00000000, 0x40020000
    .align 4
.Lfp_0:
    .word 0x40200000
    .align 8
.Lfp_1:
    .word 0x00000000, 0x400A0000
