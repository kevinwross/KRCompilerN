; kevinc: C89 to KR32 assembly
; stack-machine code generation, KR32 ILP32 ABI
    .text


    .global sum_array
sum_array:
    PUSH R7
    ADJSP #-16
    LEA R7, [SP]
    STORE R0, [R7 + 0]
    STORE R1, [R7 + 4]
    MOV R0, #0
    STORE R0, [R7 + 8]
    LEA R0, [R7 + 12]
    PUSH R0
    MOV R0, #0
    POP R1
    STORE R0, [R1]
.Lfor_1:
    LOAD R0, [R7 + 12]
    PUSH R0
    LOAD R0, [R7 + 4]
    POP R1
    SLT R0, R1, R0
    CMP R0, #0
    JZ .Lfor_end_3
    LEA R0, [R7 + 8]
    PUSH R0
    LOAD R0, [R0]
    PUSH R0
    LOAD R0, [R7 + 0]
    PUSH R0
    LOAD R0, [R7 + 12]
    MUL R0, R0, #4
    POP R1
    ADD R0, R1, R0
    LOAD R0, [R0]
    POP R1
    ADD R0, R1, R0
    POP R1
    STORE R0, [R1]
.Lfor_continue_2:
    LEA R0, [R7 + 12]
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
    LOAD R0, [R7 + 8]
    JMP .Lreturn_0
.Lreturn_0:
    ADJSP #16
    POP R7
    RET

    .global main
main:
    PUSH R7
    ADJSP #-32
    LEA R7, [SP]
    MOV R0, #0
    STORE R0, [R7 + 0]
    MOV R0, #0
    STORE R0, [R7 + 4]
    MOV R0, #0
    STORE R0, [R7 + 8]
    MOV R0, #0
    STORE R0, [R7 + 12]
    MOV R0, #1
    STORE R0, [R7 + 0]
    MOV R0, #2
    STORE R0, [R7 + 4]
    MOV R0, #3
    STORE R0, [R7 + 8]
    MOV R0, #4
    STORE R0, [R7 + 12]
    LEA R0, [.Lstr_0]
    STORE R0, [R7 + 28]
    LEA R0, [R7 + 16]
    PUSH R0
    MOV R0, #3
    POP R1
    STORE R0, [R1]
    LEA R0, [R7 + 16]
    ADD R0, R0, #4
    PUSH R0
    MOV R0, #4
    POP R1
    STORE R0, [R1]
    LEA R0, [R7 + 24]
    PUSH R0
    ADJSP #-8
    MOV R0, #4
    STORE R0, [SP]
    ADJSP #-8
    LEA R0, [R7 + 0]
    STORE R0, [SP]
    LOAD R0, [SP + 0]
    LOAD R1, [SP + 8]
    CALL sum_array
    ADJSP #16
    POP R1
    STORE R0, [R1]
    LOAD R0, [R7 + 24]
    CMP R0, #10
    JZ .Lcase_6
    CMP R0, #11
    JZ .Lcase_7
    JMP .Ldefault_8
.Lcase_6:
    LEA R0, [R7 + 24]
    PUSH R0
    MOV R0, #1
    POP R1
    STORE R0, [R1]
    JMP .Lswitch_end_5
.Lcase_7:
    LEA R0, [R7 + 24]
    PUSH R0
    MOV R0, #2
    POP R1
    STORE R0, [R1]
    JMP .Lswitch_end_5
.Ldefault_8:
    LEA R0, [R7 + 24]
    PUSH R0
    MOV R0, #3
    POP R1
    STORE R0, [R1]
    JMP .Lswitch_end_5
.Lswitch_end_5:
.Lwhile_9:
    LOAD R0, [R7 + 28]
    LOAD8S R0, [R0]
    CMP R0, #0
    JZ .Lwhile_end_10
    LEA R0, [R7 + 28]
    PUSH R0
    LOAD R0, [R0]
    PUSH R0
    ADD R0, R0, #1
    POP R2
    POP R1
    STORE R0, [R1]
    MOV R0, R2
    JMP .Lwhile_9
.Lwhile_end_10:
    LEA R0, [R7 + 16]
    LOAD R0, [R0]
    PUSH R0
    LEA R0, [R7 + 16]
    LOAD R0, [R0]
    POP R1
    MUL R0, R1, R0
    PUSH R0
    LEA R0, [R7 + 16]
    ADD R0, R0, #4
    LOAD R0, [R0]
    POP R1
    ADD R0, R1, R0
    PUSH R0
    LOAD R0, [R7 + 24]
    POP R1
    ADD R0, R1, R0
    PUSH R0
    MOV R0, #6
    POP R1
    ADD R0, R1, R0
    JMP .Lreturn_4
.Lreturn_4:
    ADJSP #32
    POP R7
    RET
    .data
    .align 4
.Lstr_0:
    .byte 104, 101, 108, 108, 111, 0
