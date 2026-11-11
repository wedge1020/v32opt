; Liveness gap G2, variant (b): two functions share one epilogue label that ends
; in "ret". The epilogue sits after _f2, so it is charged to _f2's callers (which
; ignore R1); _f1's caller reads R1.
; Correct RAM: [0] = 1, [1] = 2 (via _f1), [2] = 3 (via _f2).
    call _main
    hlt
_f1:                    ; R0 = 1, R1 = 2, then the shared epilogue
    mov r0, 1
    mov r1, 2
    jmp _epi
_f2:                    ; R0 = 3, R1 = 4, then the shared epilogue
    mov r0, 3
    mov r1, 4
_epi:
    ret
_main:
    call _f1
    mov [0], r0
    mov [1], r1
    call _f2
    mov [2], r0
    ret
