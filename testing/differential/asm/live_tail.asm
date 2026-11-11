; Liveness gap G2 (doc/liveness-audit.md), variant (a, seed): _a ends with a tail
; "jmp _b" and _b is also a direct-call target. The RET sits textually in _b, so
; it is charged to _b's callers only; _a's caller reads R1 and the write to R1
; in _b is live for it. The call of _b ignores R1.
; Correct RAM: [0] = 4, [1] = 5 (via _a), [2] = 4 (via _b).
    call _main
    hlt
_a:
    mov r0, 3
    jmp _b
_b:
    mov r1, 5
    mov r0, 4
    ret
_main:
    call _a
    mov [0], r0
    mov [1], r1
    call _b
    mov [2], r0
    ret
