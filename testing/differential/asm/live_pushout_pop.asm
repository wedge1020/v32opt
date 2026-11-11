; Liveness gap G4, variant (b): R1 is pushed and then popped into a DIFFERENT
; register (R2): not a save. R1 = 8 and R2 = 3 are both outputs of _h.
; Correct RAM: [0] = 8, [1] = 3.
    call _main
    hlt
_h:
    mov r1, 3
    push r1
    mov r1, 8
    pop r2
    ret
_main:
    call _h
    mov [0], r1
    mov [1], r2
    ret
