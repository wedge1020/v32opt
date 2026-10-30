;@hints 1 abi=caller-save
;@fn __helper_ok clobbers=R0
;@fn __helper_bad clobbers=R0,R5
%define NAN_VALUE 0x7F800000
__function_main:
    PUSH BP
    MOV BP, SP
    MOV R5, 3.0
    MOV R6, 4.0
    PUSH R5
    PUSH R6
    CALL __helper_ok
    IADD SP, 1
    POP R5
    FADD R5, R6
    MOV R1, R5
    PUSH R5
    CALL __helper_bad
    POP R5
    FADD R5, R5
    MOV R0, R5
    POP BP
    RET
__helper_ok:
    MOV R0, 1.0
    RET
__helper_bad:
    MOV R5, 9.0
    RET
