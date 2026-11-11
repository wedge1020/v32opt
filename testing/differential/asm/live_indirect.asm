; Liveness gap G1 (doc/liveness-audit.md): a function that is only ever called
; through a register ("call r5") has no direct caller whose reads could keep its
; non-ABI result registers alive. Variant (a, seed): the plainest shape.
; _pair returns R0 = 1 and R1 = 2; the indirect caller reads both.
; Correct RAM: [0] = 1, [1] = 2.
    call _main
    hlt
_pair:                  ; only ever called through a register: R0 = 1, R1 = 2
    mov r0, 1
    mov r1, 2
    ret
_main:
    mov r5, _pair
    call r5
    mov [0], r0
    mov [1], r1
    ret
