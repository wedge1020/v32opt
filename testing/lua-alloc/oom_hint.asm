    ;@hints 1 abi=caller-save lang=lua
    CALL  __user
    HLT

;; no __malloc_owned label here: only the 'nonzero' hint can drop the first check
__user:
    MOV   R0, 9
    PUSH  R0
    CALL  __malloc
    IADD  SP, 1
    MOV   R4, R0
    IEQ   R4, 0
    JT    R4, __alloc_failed       ; CHECK-H (dead: __malloc is 'nonzero'; any target)
    MOV   [R0], R4
    MOV   R0, 3
    PUSH  R0
    CALL  __other
    IADD  SP, 1
    MOV   R5, R0
    IEQ   R5, 0
    JT    R5, __alloc_failed       ; CHECK-N (live: __other has no hint)
    MOV   [R0], R5
    RET

    ;@fn __malloc clobbers=R0-R3,R6 nonzero
__malloc:
    MOV   R0, [0]
    IADD  R0, 1
    MOV   [0], R0
    RET
    ;@fn __other clobbers=R0
__other:
    MOV   R0, [1]
    RET
__alloc_failed:
    HLT
