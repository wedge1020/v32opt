; Liveness gap G2, variant (c): _a falls through into _b (no jmp/ret before the
; label). _b is a direct-call target, _a's caller reads R1, _b's caller does not.
; Correct RAM: [0] = 7, [1] = 5 (via _a), [2] = 11 (via _b, R0 = 10 + 1).
    call _main
    hlt
_a:                     ; R1 = 5, R0 = 6, falls through to _b
    mov r1, 5
    mov r0, 6
_b:                     ; R0 += 1, returns
    iadd r0, 1
    ret
_main:
    call _a
    mov [0], r0
    mov [1], r1
    mov r0, 10
    call _b
    mov [2], r0
    ret
