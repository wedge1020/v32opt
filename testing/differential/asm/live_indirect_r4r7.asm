; Liveness gap G1, variant (d): an indirectly called function whose results are
; in R4 and R7 (besides R0), not in the usual R1. Registers above the ABI
; result set must be treated as possible outputs too.
; Correct RAM: [0] = 1, [1] = 4, [2] = 7.
    call _main
    hlt
_multi:                 ; R0 = 1, R4 = 4, R7 = 7
    mov r0, 1
    mov r4, 4
    mov r7, 7
    ret
_main:
    mov r5, _multi
    call r5
    mov [0], r0
    mov [1], r4
    mov [2], r7
    ret
