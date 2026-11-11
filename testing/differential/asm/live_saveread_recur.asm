; Liveness gap G3, variant (d): a recursive function with its argument in R1,
; saved by push/pop. The depth is bounded by R3; R1 is decremented between push
; and pop (the value the callee passes down) and read again after the pop.
; _sum: R0 = R1 + (R1-1) + ... for R3 levels (the pushed values).
; Correct RAM: [0] = 10, [1] = 4.
    call _main
    hlt
_sum:
    push r1
    jf r3, _base
    isub r3, 1
    isub r1, 1          ; modifies the saved register between push and pop
    call _sum
    pop r1
    iadd r0, r1
    ret
_base:
    pop r1
    mov r0, 0
    ret
_main:
    mov r1, 4
    mov r3, 4
    call _sum
    mov [0], r0
    ret
