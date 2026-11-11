; Liveness gap G4 (doc/liveness-audit.md), variant (a, seed): _g contains
; "push r1", but it passes R1 as a stack argument and does not save it. R1 = 9
; at the RET is a real output; it must not be hidden from the output set.
; Correct RAM: [0] = 7 (_id returns its stack argument), [1] = 9.
    call _main
    hlt
_id:
    mov r0, [sp+1]
    ret
_g:                     ; pushes R1 as stack argument, then returns a 2nd result in R1
    mov r1, 7
    push r1
    call _id
    iadd sp, 1
    mov r1, 9
    ret
_main:
    call _g
    mov [0], r0
    mov [1], r1
    ret
