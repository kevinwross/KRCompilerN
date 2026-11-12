; kevinc: C89 to KR32 assembly
; stack-machine code generation, KR32 ILP32 ABI
    .text


    .global return_pointer
return_pointer:
    PUSH R7
    ADJSP #-8
    LEA R7, [SP]
    STORE R0, [R7 + 0]
    LOAD R0, [R7 + 0]
    JMP .Lreturn_0
.Lreturn_0:
    ADJSP #8
    POP R7
    RET

    .global apply
apply:
    PUSH R7
    ADJSP #-8
    LEA R7, [SP]
    STORE R0, [R7 + 0]
    STORE R1, [R7 + 4]
    LOAD R0, [R7 + 0]
    ADJSP #-4
    STORE R0, [SP]
    ADJSP #-8
    LOAD R0, [R7 + 4]
    STORE R0, [SP]
    LOAD R0, [SP + 0]
    LOAD R4, [SP + 8]
    CALL R4
    ADJSP #12
    JMP .Lreturn_1
.Lreturn_1:
    ADJSP #8
    POP R7
    RET

    .global double_it
double_it:
    PUSH R7
    ADJSP #-8
    LEA R7, [SP]
    STORE R0, [R7 + 0]
    LOAD R0, [R7 + 0]
    PUSH R0
    MOV R0, #2
    POP R1
    MUL R0, R1, R0
    JMP .Lreturn_2
.Lreturn_2:
    ADJSP #8
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
    MOV R0, #1
    STORE R0, [R7 + 0]
    MOV R0, #2
    STORE R0, [R7 + 4]
    MOV R0, #3
    STORE R0, [R7 + 8]
    LEA R0, [R7 + 0]
    STORE R0, [R7 + 12]
    MOV R0, #6
    SHL R0, R0, #16
    SAR R0, R0, #16
    STORE16 R0, [R7 + 24]
    SHL R0, R0, #16
    SAR R0, R0, #16
    MOV R0, #7
    AND R0, R0, #255
    STORE8 R0, [R7 + 26]
    AND R0, R0, #255
    MOV R0, #8
    STORE R0, [R7 + 28]
    LEA R0, [R7 + 16]
    PUSH R0
    MOV R0, #4
    POP R1
    STORE R0, [R1]
    LEA R0, [R7 + 20]
    PUSH R0
    MOV R0, #5
    POP R1
    STORE R0, [R1]
    LOAD R0, [R7 + 12]
    PUSH R0
    MOV R0, #1
    MUL R0, R0, #4
    POP R1
    ADD R0, R1, R0
    LOAD R0, [R0]
    PUSH R0
    LEA R0, [R7 + 16]
    LOAD R0, [R0]
    POP R1
    ADD R0, R1, R0
    PUSH R0
    LEA R0, [R7 + 20]
    LOAD R0, [R0]
    POP R1
    ADD R0, R1, R0
    PUSH R0
    LOAD16S R0, [R7 + 24]
    POP R1
    ADD R0, R1, R0
    PUSH R0
    LOAD8U R0, [R7 + 26]
    POP R1
    ADD R0, R1, R0
    PUSH R0
    LOAD R0, [R7 + 28]
    POP R1
    ADD R0, R1, R0
    PUSH R0
    MOV R0, #-3
    POP R1
    ADD R0, R1, R0
    PUSH R0
    ADJSP #-8
    LEA R0, [R7 + 0]
    PUSH R0
    MOV R0, #0
    MUL R0, R0, #4
    POP R1
    ADD R0, R1, R0
    STORE R0, [SP]
    LOAD R0, [SP + 0]
    CALL return_pointer
    ADJSP #8
    PUSH R0
    MOV R0, #0
    MUL R0, R0, #4
    POP R1
    ADD R0, R1, R0
    LOAD R0, [R0]
    POP R1
    ADD R0, R1, R0
    PUSH R0
    ADJSP #-8
    MOV R0, #10
    STORE R0, [SP]
    ADJSP #-8
    LEA R0, [double_it]
    STORE R0, [SP]
    LOAD R0, [SP + 0]
    LOAD R1, [SP + 8]
    CALL apply
    ADJSP #16
    POP R1
    ADD R0, R1, R0
    JMP .Lreturn_3
.Lreturn_3:
    ADJSP #32
    POP R7
    RET
    .data
    .align 4
    .global name
name:
    .word .Lstr_0
    .align 4
pointer_array:
    .space 8
    .align 4
.Lstr_0:
    .byte 107, 101, 118, 105, 110, 99, 0
