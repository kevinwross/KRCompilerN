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

twice:
    PUSH R7
    ADJSP #-8
    LEA R7, [SP]
    STORE R0, [R7 + 0]
    LOAD R0, [R7 + 0]
    PUSH R0
    MOV R0, #2
    POP R1
    MUL R0, R1, R0
    JMP .Lreturn_1
.Lreturn_1:
    ADJSP #8
    POP R7
    RET

    .global main
main:
    PUSH R7
    ADJSP #-16
    LEA R7, [SP]
    MOV R0, #3
    STORE R0, [R7 + 0]
    MOV R0, #4
    STORE R0, [R7 + 4]
    ADJSP #-8
    LOAD R0, [R7 + 4]
    STORE R0, [SP]
    ADJSP #-8
    LOAD R0, [R7 + 0]
    STORE R0, [SP]
    LOAD R0, [SP + 0]
    LOAD R1, [SP + 8]
    CALL add
    ADJSP #16
    STORE R0, [R7 + 8]
    LOAD R0, [R7 + 8]
    PUSH R0
    MOV R0, #5
    POP R1
    SLT R0, R0, R1
    CMP R0, #0
    JZ .Lif_else_3
    LEA R0, [R7 + 8]
    PUSH R0
    ADJSP #-8
    LOAD R0, [R7 + 8]
    STORE R0, [SP]
    LOAD R0, [SP + 0]
    CALL twice
    ADJSP #8
    POP R1
    STORE R0, [R1]
    JMP .Lif_end_4
.Lif_else_3:
    LEA R0, [R7 + 8]
    PUSH R0
    MOV R0, #0
    POP R1
    STORE R0, [R1]
.Lif_end_4:
.Lwhile_5:
    LOAD R0, [R7 + 0]
    PUSH R0
    MOV R0, #10
    POP R1
    SLT R0, R1, R0
    CMP R0, #0
    JZ .Lwhile_end_6
    LEA R0, [R7 + 0]
    PUSH R0
    LOAD R0, [R0]
    PUSH R0
    ADD R0, R0, #1
    POP R2
    POP R1
    STORE R0, [R1]
    MOV R0, R2
    JMP .Lwhile_5
.Lwhile_end_6:
    LEA R0, [R7 + 4]
    PUSH R0
    MOV R0, #0
    POP R1
    STORE R0, [R1]
.Lfor_7:
    LOAD R0, [R7 + 4]
    PUSH R0
    MOV R0, #3
    POP R1
    SLT R0, R1, R0
    CMP R0, #0
    JZ .Lfor_end_9
    LEA R0, [R7 + 8]
    PUSH R0
    LOAD R0, [R7 + 8]
    PUSH R0
    LOAD R0, [R7 + 4]
    POP R1
    ADD R0, R1, R0
    POP R1
    STORE R0, [R1]
.Lfor_continue_8:
    LEA R0, [R7 + 4]
    PUSH R0
    LOAD R0, [R7 + 4]
    PUSH R0
    MOV R0, #1
    POP R1
    ADD R0, R1, R0
    POP R1
    STORE R0, [R1]
    JMP .Lfor_7
.Lfor_end_9:
    LOAD R0, [R7 + 8]
    JMP .Lreturn_2
.Lreturn_2:
    ADJSP #16
    POP R7
    RET
    .data
    .align 4
    .global global_counter
global_counter:
    .word 0x0000002A
    .align 4
internal_value:
    .word 0x00000007
