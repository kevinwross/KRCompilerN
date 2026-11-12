; kevinc: C89 to KR32 assembly
; stack-machine code generation, KR32 ILP32 ABI
    .text


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
    MOV R0, #7
    STORE R0, [R7 + 0]
    MOV R0, #121
    SHL R0, R0, #24
    SAR R0, R0, #24
    STORE8 R0, [R7 + 4]
    SHL R0, R0, #24
    SAR R0, R0, #24
    MOV R0, #9
    STORE R0, [R7 + 8]
    MOV R0, #0
    STORE R0, [R7 + 12]
    MOV R0, #0
    STORE R0, [R7 + 16]
    MOV R0, #0
    STORE R0, [R7 + 20]
    MOV R0, #0
    STORE R0, [R7 + 24]
    MOV R0, #0
    STORE R0, [R7 + 12]
    MOV R0, #0
    STORE R0, [R7 + 16]
    MOV R0, #1
    STORE R0, [R7 + 12]
    MOV R0, #2
    STORE R0, [R7 + 16]
    MOV R0, #0
    STORE R0, [R7 + 20]
    MOV R0, #0
    STORE R0, [R7 + 24]
    MOV R0, #3
    STORE R0, [R7 + 20]
    MOV R0, #4
    STORE R0, [R7 + 24]
    LEA R0, [global_pair]
    LOAD R0, [R0]
    PUSH R0
    LEA R0, [global_array]
    PUSH R0
    MOV R0, #1
    MUL R0, R0, #4
    POP R1
    ADD R0, R1, R0
    LOAD R0, [R0]
    POP R1
    ADD R0, R1, R0
    PUSH R0
    LEA R0, [global_string]
    PUSH R0
    MOV R0, #0
    POP R1
    ADD R0, R1, R0
    LOAD8S R0, [R0]
    POP R1
    ADD R0, R1, R0
    PUSH R0
    LEA R0, [R7 + 0]
    ADD R0, R0, #8
    LOAD R0, [R0]
    POP R1
    ADD R0, R1, R0
    PUSH R0
    LEA R0, [R7 + 12]
    PUSH R0
    MOV R0, #1
    MUL R0, R0, #8
    POP R1
    ADD R0, R1, R0
    PUSH R0
    MOV R0, #0
    MUL R0, R0, #4
    POP R1
    ADD R0, R1, R0
    LOAD R0, [R0]
    POP R1
    ADD R0, R1, R0
    JMP .Lreturn_0
.Lreturn_0:
    ADJSP #32
    POP R7
    RET
    .data
    .align 4
    .global global_pair
global_pair:
    .word 0x00000001
    .byte 120
    .space 3
    .word 0x00000003
    .align 4
    .global global_array
global_array:
    .word 0x00000004
    .word 0x00000005
    .word 0x00000006
    .align 1
    .global global_string
global_string:
    .byte 97
    .byte 98
    .byte 99
    .byte 0
