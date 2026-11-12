; kevinc: C89 to KR32 assembly
; stack-machine code generation, KR32 ILP32 ABI
    .text


    .global sum
sum:
    PUSH R7
    ADJSP #-16
    LEA R7, [SP]
    STORE R0, [R7 + 0]
    MOV R0, #0
    STORE R0, [R7 + 4]
    LEA R0, [R7 + 8]
    PUSH R0
    MOV R0, #0
    POP R1
    STORE R0, [R1]
.Lfor_1:
    LOAD R0, [R7 + 8]
    PUSH R0
    LOAD R0, [R7 + 0]
    POP R1
    SLT R0, R1, R0
    CMP R0, #0
    JZ .Lfor_end_3
    LOAD R0, [R7 + 8]
    PUSH R0
    MOV R0, #2
    POP R1
    XOR R0, R1, R0
    SLTU R0, R0, #1
    CMP R0, #0
    JZ .Lif_else_4
    JMP .Lfor_continue_2
    JMP .Lif_end_5
.Lif_else_4:
.Lif_end_5:
    LOAD R0, [R7 + 8]
    PUSH R0
    MOV R0, #5
    POP R1
    XOR R0, R1, R0
    SLTU R0, R0, #1
    CMP R0, #0
    JZ .Lif_else_6
    JMP .Lfor_end_3
    JMP .Lif_end_7
.Lif_else_6:
.Lif_end_7:
    LEA R0, [R7 + 4]
    PUSH R0
    LOAD R0, [R0]
    PUSH R0
    LOAD R0, [R7 + 8]
    POP R1
    ADD R0, R1, R0
    POP R1
    STORE R0, [R1]
.Lfor_continue_2:
    LEA R0, [R7 + 8]
    PUSH R0
    LOAD R0, [R0]
    PUSH R0
    ADD R0, R0, #1
    POP R2
    POP R1
    STORE R0, [R1]
    MOV R0, R2
    JMP .Lfor_1
.Lfor_end_3:
    LOAD R0, [R7 + 4]
    JMP .Lreturn_0
.Lreturn_0:
    ADJSP #16
    POP R7
    RET

    .global nothing
nothing:
    PUSH R7
    LEA R7, [SP]
    JMP .Lreturn_8
.Lreturn_8:
    POP R7
    RET

    .global main
main:
    PUSH R7
    LEA R7, [SP]
    CALL nothing
    ADJSP #-8
    MOV R0, #10
    STORE R0, [SP]
    LOAD R0, [SP + 0]
    CALL sum
    ADJSP #8
    JMP .Lreturn_9
.Lreturn_9:
    POP R7
    RET
    .data
