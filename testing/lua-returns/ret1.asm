; lua-returns, unit shape: a v32lua-style function reached only through "CALL R1".
; With ';@fn __function_f returns=1' (or 0) nothing reads R2/R3 after its RET, so "MOV R3, 5" is dead;
; with returns=2 or no hint it is a returned value and must stay.
%define  RET_COUNT  0x00000100
    JMP   __start
__function_f:
    PUSH  BP
    MOV   BP, SP
    MOV   R0, 7
    MOV   R2, 4
    MOV   R3, 5
    MOV   R1, 1
    MOV   [RET_COUNT], R1
__f_return:
    MOV   SP, BP
    POP   BP
    RET
__start:
    MOV   R1, __function_f
    CALL  R1
    HLT
