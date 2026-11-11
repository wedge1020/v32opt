; Liveness gap G3 (doc/liveness-audit.md), variant (a, seed): _f starts with
; "push r1" which looks like a callee save, but between the push and the pop it
; READS the caller's R1 (a register argument). The caller's "mov r1, 41" is live.
; Correct RAM: [0] = 42.
    call _main
    hlt
_f:
    push r1
    mov r0, r1
    iadd r0, 1
    mov r1, 0
    pop r1
    ret
_main:
    mov r1, 41
    call _f
    mov [0], r0
    ret
