; kevinc: C89 to KR32 assembly
; stack-machine code generation, KR32 ILP32 ABI
    .text


    .global compare
compare:
    PUSH R7
    ADJSP #-8
    LEA R7, [SP]
    STORE R0, [R7 + 0]
    STORE R1, [R7 + 4]
    LOAD R0, [R7 + 0]
    PUSH R0
    LOAD R0, [R7 + 4]
    POP R1
    XOR R0, R1, R0
    SLTU R0, R0, #1
    CMP R0, #0
    JZ .Lcond_else_1
    MOV R0, #0
    JMP .Lcond_end_2
.Lcond_else_1:
    MOV R0, #1
.Lcond_end_2:
    JMP .Lreturn_0
.Lreturn_0:
    ADJSP #8
    POP R7
    RET

    .global mix
mix:
    PUSH R7
    ADJSP #-8
    LEA R7, [SP]
    STORE R0, [R7 + 0]
    STORE R1, [R7 + 4]
    LOAD R0, [R7 + 0]
    PUSH R0
    MOV R0, #2
    POP R1
    SHL R0, R1, R0
    PUSH R0
    LOAD R0, [R7 + 4]
    PUSH R0
    MOV R0, #1
    POP R1
    SHR R0, R1, R0
    PUSH R0
    LOAD R0, [R7 + 0]
    PUSH R0
    LOAD R0, [R7 + 4]
    POP R1
    AND R0, R1, R0
    POP R1
    XOR R0, R1, R0
    POP R1
    OR R0, R1, R0
    JMP .Lreturn_3
.Lreturn_3:
    ADJSP #8
    POP R7
    RET

    .global pick
pick:
    PUSH R7
    ADJSP #-16
    LEA R7, [SP]
    STORE R0, [R7 + 0]
    STORE R1, [R7 + 4]
    STORE R2, [R7 + 8]
    LOAD R0, [R7 + 0]
    CMP R0, #0
    JZ .Lcond_else_5
    LOAD R0, [R7 + 4]
    JMP .Lcond_end_6
.Lcond_else_5:
    LOAD R0, [R7 + 8]
.Lcond_end_6:
    JMP .Lreturn_4
.Lreturn_4:
    ADJSP #16
    POP R7
    RET

    .global main
main:
    PUSH R7
    ADJSP #-8
    LEA R7, [SP]
    LEA R0, [nodes]
    PUSH R0
    MOV R0, #0
    MUL R0, R0, #8
    POP R1
    ADD R0, R1, R0
    STORE R0, [R7 + 0]
    LOAD R0, [R7 + 0]
    PUSH R0
    MOV R0, #10
    POP R1
    STORE R0, [R1]
    LOAD R0, [R7 + 0]
    ADD R0, R0, #4
    PUSH R0
    LEA R0, [nodes]
    PUSH R0
    MOV R0, #1
    MUL R0, R0, #8
    POP R1
    ADD R0, R1, R0
    POP R1
    STORE R0, [R1]
    LOAD R0, [R7 + 0]
    ADD R0, R0, #4
    LOAD R0, [R0]
    PUSH R0
    MOV R0, #20
    POP R1
    STORE R0, [R1]
    LEA R0, [R7 + 4]
    PUSH R0
    LOAD R0, [R7 + 0]
    LOAD R0, [R0]
    PUSH R0
    MOV R0, #5
    POP R1
    SLT R0, R0, R1
    CMP R0, #0
    JZ .Land_false_10
    LOAD R0, [R7 + 0]
    ADD R0, R0, #4
    LOAD R0, [R0]
    PUSH R0
    MOV R0, #0
    POP R1
    XOR R0, R1, R0
    SLTU R0, R0, #1
    XOR R0, R0, #1
    CMP R0, #0
    JZ .Land_false_10
    MOV R0, #1
    JMP .Land_end_11
.Land_false_10:
    MOV R0, #0
.Land_end_11:
    CMP R0, #0
    JZ .Lcond_else_8
    LOAD R0, [R7 + 0]
    LOAD R0, [R0]
    JMP .Lcond_end_9
.Lcond_else_8:
    LOAD R0, [R7 + 0]
    ADD R0, R0, #4
    LOAD R0, [R0]
    LOAD R0, [R0]
.Lcond_end_9:
    POP R1
    STORE R0, [R1]
    LEA R0, [R7 + 4]
    PUSH R0
    LOAD R0, [R0]
    PUSH R0
    MOV R0, #8
    POP R1
    ADD R0, R1, R0
    POP R1
    STORE R0, [R1]
    LEA R0, [R7 + 4]
    PUSH R0
    LOAD R0, [R0]
    PUSH R0
    MOV R0, #4
    POP R1
    ADD R0, R1, R0
    POP R1
    STORE R0, [R1]
    LEA R0, [R7 + 4]
    PUSH R0
    LOAD R0, [R0]
    PUSH R0
    ADJSP #-8
    MOV R0, #4
    STORE R0, [SP]
    ADJSP #-8
    MOV R0, #3
    STORE R0, [SP]
    LOAD R0, [SP + 0]
    LOAD R1, [SP + 8]
    CALL mix
    ADJSP #16
    POP R1
    XOR R0, R1, R0
    POP R1
    STORE R0, [R1]
    ADJSP #-8
    LOAD R0, [R7 + 4]
    NEG R0, R0
    STORE R0, [SP]
    ADJSP #-8
    LOAD R0, [R7 + 4]
    STORE R0, [SP]
    ADJSP #-8
    LOAD R0, [R7 + 4]
    PUSH R0
    MOV R0, #1
    POP R1
    AND R0, R1, R0
    STORE R0, [SP]
    LOAD R0, [SP + 0]
    LOAD R1, [SP + 8]
    LOAD R2, [SP + 16]
    CALL pick
    ADJSP #24
    PUSH R0
    ADJSP #-8
    LEA R0, [nodes]
    PUSH R0
    MOV R0, #1
    MUL R0, R0, #8
    POP R1
    ADD R0, R1, R0
    STORE R0, [SP]
    ADJSP #-8
    LEA R0, [nodes]
    PUSH R0
    MOV R0, #0
    MUL R0, R0, #8
    POP R1
    ADD R0, R1, R0
    STORE R0, [SP]
    LOAD R0, [SP + 0]
    LOAD R1, [SP + 8]
    CALL compare
    ADJSP #16
    POP R1
    ADD R0, R1, R0
    JMP .Lreturn_7
.Lreturn_7:
    ADJSP #8
    POP R7
    RET
    .data
    .align 4
nodes:
    .space 16
