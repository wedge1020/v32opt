; zero-test-comment: peephole-zero-test rewrites "MOV R1, R2 / IEQ R1, 0 / JT R1, L" into "JF R2, L"; the
; jump's trailing comment must survive the rewrite.
    JMP   __start
__function_f:
    MOV   R2, [BP + 2]
    MOV   R1, R2
    IEQ   R1, 0
    JT    R1, __f_end ; SRC-NOTE keep me
    MOV   R0, 1
__f_end:
    RET
__start:
    PUSH  R3
    CALL  __function_f
    IADD  SP, 1
    HLT
