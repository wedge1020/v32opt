; Liveness gap G5 (doc/liveness-audit.md), variant (a, seed): "push r1 / call
; _id / pop r1" looks like a save of R1 around a call, but the pushed word is
; _id's stack argument ([sp+1]), so the caller's R1 = 77 is read through memory.
; Correct RAM: [0] = 77.
    call _main
    hlt
_id:
    mov r0, [sp+1]
    ret
_f:
    push r1
    call _id
    pop r1
    ret
_main:
    mov r1, 77
    call _f
    mov [0], r0
    ret
