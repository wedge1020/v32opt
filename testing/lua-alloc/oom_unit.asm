    ;@hints 1 abi=caller-save lang=lua
    CALL  __user
    HLT

;; caller: the shapes of v32lua's runtime
__user:
    MOV   R0, 9
    PUSH  R0
    CALL  __malloc
    IADD  SP, 1
    MOV   R4, R0
    IEQ   R4, 0
    JT    R4, __oom_handler        ; CHECK-A (dead: __malloc never returns 0)
    MOV   [R0], R4
    PUSH  R2
    MOV   R0, 5
    PUSH  R0
    CALL  __malloc_owned
    IADD  SP, 1
    POP   R2
    MOV   R5, R0
    MOV   R5, [R5]
    IEQ   R5, 0
    JT    R5, __oom_handler        ; CHECK-B (live: R5 is a word loaded from the block, not the pointer)
    MOV   [R0], R5
    RET

    ;@fn __malloc_owned clobbers=R0-R3,R6
__malloc_owned:
__malloc:
    PUSH  BP
    MOV   BP, SP
    MOV   R1, [BP+2]
    MOV   R0, [0]
    MOV   R2, R0
    IADD  R2, R1
    MOV   R3, SP
    ISUB  R3, 1024
    MOV   R6, R2
    IGE   R6, R3
    JT    R6, __oom_handler        ; CHECK-M (__malloc's own: must stay)
    MOV   [0], R2
    MOV   SP, BP
    POP   BP
    RET
__oom_handler:
    HLT
