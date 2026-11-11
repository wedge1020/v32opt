; Liveness gap G4, variant (c): swap R1 and R2 through the stack, then adjust R1.
; Both registers are pushed and popped, but into each other, so neither is a save
; and both are outputs; the final "iadd r1, 10" must not be deleted.
; Correct RAM: [0] = 12 (R1), [1] = 1 (R2).
    call _main
    hlt
_swap:
    mov r1, 1
    mov r2, 2
    push r1
    push r2
    pop r1
    pop r2
    iadd r1, 10
    ret
_main:
    call _swap
    mov [0], r1
    mov [1], r2
    ret
