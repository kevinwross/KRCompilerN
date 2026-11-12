; kevinc: C89 to KR32 assembly
; stack-machine code generation, KR32 ILP32 ABI
    .text


    .global plus
plus:
    PUSH R7
    ADJSP #-8
    LEA R7, [SP]
    STORE R0, [R7 + 0]
    STORE R1, [R7 + 4]
    LOAD R0, [R7 + 0]
    PUSH R0
    LOAD R0, [R7 + 4]
    POP R1
    ADD R0, R1, R0
    JMP .Lreturn_0
.Lreturn_0:
    ADJSP #8
    POP R7
    RET

    .global minus
minus:
    PUSH R7
    ADJSP #-8
    LEA R7, [SP]
    STORE R0, [R7 + 0]
    STORE R1, [R7 + 4]
    LOAD R0, [R7 + 0]
    PUSH R0
    LOAD R0, [R7 + 4]
    POP R1
    SUB R0, R1, R0
    JMP .Lreturn_1
.Lreturn_1:
    ADJSP #8
    POP R7
    RET

    .global choose
choose:
    PUSH R7
    ADJSP #-8
    LEA R7, [SP]
    STORE R0, [R7 + 0]
    LOAD R0, [R7 + 0]
    CMP R0, #0
    JZ .Lcond_else_3
    LEA R0, [plus]
    JMP .Lcond_end_4
.Lcond_else_3:
    LEA R0, [minus]
.Lcond_end_4:
    JMP .Lreturn_2
.Lreturn_2:
    ADJSP #8
    POP R7
    RET

    .global main
main:
    PUSH R7
    ADJSP #-16
    LEA R7, [SP]
    ADJSP #-8
    MOV R0, #1
    STORE R0, [SP]
    LOAD R0, [SP + 0]
    CALL choose
    ADJSP #8
    STORE R0, [R7 + 0]
    LEA R0, [minus]
    STORE R0, [R7 + 4]
    LEA R0, [R7 + 8]
    PUSH R0
    MOV R0, #0
    MUL R0, R0, #4
    POP R1
    ADD R0, R1, R0
    PUSH R0
    LEA R0, [plus]
    POP R1
    STORE R0, [R1]
    LEA R0, [R7 + 8]
    PUSH R0
    MOV R0, #1
    MUL R0, R0, #4
    POP R1
    ADD R0, R1, R0
    PUSH R0
    LEA R0, [minus]
    POP R1
    STORE R0, [R1]
    LOAD R0, [R7 + 0]
    ADJSP #-4
    STORE R0, [SP]
    ADJSP #-8
    MOV R0, #3
    STORE R0, [SP]
    ADJSP #-8
    MOV R0, #5
    STORE R0, [SP]
    LOAD R0, [SP + 0]
    LOAD R1, [SP + 8]
    LOAD R4, [SP + 16]
    CALL R4
    ADJSP #20
    PUSH R0
    LOAD R0, [R7 + 4]
    ADJSP #-4
    STORE R0, [SP]
    ADJSP #-8
    MOV R0, #3
    STORE R0, [SP]
    ADJSP #-8
    MOV R0, #5
    STORE R0, [SP]
    LOAD R0, [SP + 0]
    LOAD R1, [SP + 8]
    LOAD R4, [SP + 16]
    CALL R4
    ADJSP #20
    POP R1
    ADD R0, R1, R0
    PUSH R0
    LEA R0, [R7 + 8]
    PUSH R0
    MOV R0, #0
    MUL R0, R0, #4
    POP R1
    ADD R0, R1, R0
    LOAD R0, [R0]
    ADJSP #-4
    STORE R0, [SP]
    ADJSP #-8
    MOV R0, #2
    STORE R0, [SP]
    ADJSP #-8
    MOV R0, #1
    STORE R0, [SP]
    LOAD R0, [SP + 0]
    LOAD R1, [SP + 8]
    LOAD R4, [SP + 16]
    CALL R4
    ADJSP #20
    POP R1
    ADD R0, R1, R0
    JMP .Lreturn_5
.Lreturn_5:
    ADJSP #16
    POP R7
    RET
    .data
