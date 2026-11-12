; kevinc: C89 to KR32 assembly
; stack-machine code generation, KR32 ILP32 ABI
    .text


    .global add
add:
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

    .global subtract
subtract:
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

    .global main
main:
    PUSH R7
    ADJSP #-8
    LEA R7, [SP]
    LEA R0, [operation]
    PUSH R0
    LEA R0, [add]
    POP R1
    STORE R0, [R1]
    LEA R0, [R7 + 0]
    PUSH R0
    LEA R0, [operation]
    LOAD R0, [R0]
    ADJSP #-4
    STORE R0, [SP]
    ADJSP #-8
    MOV R0, #3
    STORE R0, [SP]
    ADJSP #-8
    MOV R0, #7
    STORE R0, [SP]
    LOAD R0, [SP + 0]
    LOAD R1, [SP + 8]
    LOAD R4, [SP + 16]
    CALL R4
    ADJSP #20
    POP R1
    STORE R0, [R1]
    LEA R0, [operation]
    PUSH R0
    LEA R0, [subtract]
    POP R1
    STORE R0, [R1]
    LEA R0, [R7 + 0]
    PUSH R0
    LOAD R0, [R7 + 0]
    PUSH R0
    LEA R0, [operation]
    LOAD R0, [R0]
    ADJSP #-4
    STORE R0, [SP]
    ADJSP #-8
    MOV R0, #3
    STORE R0, [SP]
    ADJSP #-8
    MOV R0, #7
    STORE R0, [SP]
    LOAD R0, [SP + 0]
    LOAD R1, [SP + 8]
    LOAD R4, [SP + 16]
    CALL R4
    ADJSP #20
    POP R1
    ADD R0, R1, R0
    POP R1
    STORE R0, [R1]
    LOAD R0, [R7 + 0]
    JMP .Lreturn_2
.Lreturn_2:
    ADJSP #8
    POP R7
    RET
    .data
    .align 4
    .global operation
operation:
    .space 4
