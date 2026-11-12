; kevinc: C89 to KR32 assembly
; stack-machine code generation, KR32 ILP32 ABI
    .text


    .global main
main:
    PUSH R7
    LEA R7, [SP]
    MOV R0, #9
    JMP .Lreturn_0
.Lreturn_0:
    POP R7
    RET
    .data
