; kevinc: C89 to KR32 assembly
; stack-machine code generation, KR32 ILP32 ABI
    .text


    .global counter
counter:
    PUSH R7
    ADJSP #-8
    LEA R7, [SP]
    STORE R0, [R7 + 0]
    MOV R0, #0
    STORE R0, [R7 + 4]
    LEA R0, [__static_calls_0]
    PUSH R0
    LEA R0, [__static_calls_0]
    LOAD R0, [R0]
    PUSH R0
    MOV R0, #1
    POP R1
    ADD R0, R1, R0
    POP R1
    STORE R0, [R1]
.Ldo_1:
    LEA R0, [R7 + 4]
    PUSH R0
    LOAD R0, [R0]
    PUSH R0
    ADD R0, R0, #1
    POP R2
    POP R1
    STORE R0, [R1]
    MOV R0, R2
.Ldo_continue_2:
    LOAD R0, [R7 + 4]
    PUSH R0
    LOAD R0, [R7 + 0]
    POP R1
    SLT R0, R1, R0
    CMP R0, #0
    JNZ .Ldo_1
.Ldo_end_3:
    JMP .Luser_1_finish
    LEA R0, [R7 + 4]
    PUSH R0
    MOV R0, #99
    POP R1
    STORE R0, [R1]
.Luser_1_finish:
    LOAD R0, [R7 + 4]
    PUSH R0
    LEA R0, [__static_calls_0]
    LOAD R0, [R0]
    POP R1
    ADD R0, R1, R0
    JMP .Lreturn_0
.Lreturn_0:
    ADJSP #8
    POP R7
    RET

    .global main
main:
    PUSH R7
    LEA R7, [SP]
    ADJSP #-8
    MOV R0, #3
    STORE R0, [SP]
    LOAD R0, [SP + 0]
    CALL counter
    ADJSP #8
    PUSH R0
    ADJSP #-8
    MOV R0, #4
    STORE R0, [SP]
    LOAD R0, [SP + 0]
    CALL counter
    ADJSP #8
    POP R1
    ADD R0, R1, R0
    JMP .Lreturn_4
.Lreturn_4:
    POP R7
    RET
    .data
    .align 4
__static_calls_0:
    .word 0x00000000
