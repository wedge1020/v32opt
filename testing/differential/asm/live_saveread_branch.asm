; Liveness gap G3, variant (b): the read of the saved register R1 happens on one
; branch of a "jt" only. The caller's R1 is an argument when R2 != 0.
; Correct RAM: [0] = 42 (R2 = 1: R0 = R1 + 1), [1] = 0 (R2 = 0).
    call _main
    hlt
_f:                     ; R0 = R2 ? R1 + 1 : 0, R1 preserved
    push r1
    jt r2, _read
    mov r0, 0
    jmp _done
_read:
    mov r0, r1
    iadd r0, 1
_done:
    mov r1, 0
    pop r1
    ret
_main:
    mov r2, 1
    mov r1, 41
    call _f
    mov [0], r0
    mov r2, 0
    mov r1, 99          ; really dead: the R2 = 0 path never reads R1
    call _f
    mov [1], r0
    ret
