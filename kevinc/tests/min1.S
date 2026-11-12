; kevinc: C89 to KR32 assembly
; stack-machine code generation, KR32 ILP32 ABI
    .text


    .global main
main:
    PUSH R7
    ADJSP #-8
    LEA R7, [SP]
    MOV R0, #1
    STORE R0, [R7 + 0]
    LOAD R0, [R7 + 0]
    JMP .Lreturn_0
.Lreturn_0:
    ADJSP #8
    POP R7
    RET
    .data
