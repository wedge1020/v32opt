; Liveness gap G2, variant (d): a conditional "jt" from _a into the middle of
; _b's body. On that path the write R1 = 5 only reaches _b's RET, whose callers
; ignore R1; on the other path _a writes R1 = 6 itself. Both paths are tested.
; Correct RAM: [0] = 5, [1] = 5 (R2 = 1: through _b), [2] = 0, [3] = 6 (R2 = 0),
; [4] = 5 (direct call of _b: R0 = 2 + 3).
    call _main
    hlt
_a:                     ; R2 selects the path
    mov r1, 5
    mov r0, 2
    jt r2, _b_body
    mov r0, 0
    mov r1, 6
    ret
_b:
    mov r0, 2
_b_body:
    iadd r0, 3
    ret
_main:
    mov r2, 1
    call _a
    mov [0], r0
    mov [1], r1
    mov r2, 0
    call _a
    mov [2], r0
    mov [3], r1
    call _b
    mov [4], r0
    ret
