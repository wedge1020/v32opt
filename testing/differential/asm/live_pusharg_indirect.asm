; Liveness gap G5, variant (c): "push r1 / call r5 / pop r1": the call is
; indirect, so the callee cannot be inspected and the push may be an argument.
; Correct RAM: [0] = 77.
    call _main
    hlt
_id:
    mov r0, [sp+1]
    ret
_f:
    push r1
    mov r5, _id
    call r5
    pop r1
    ret
_main:
    mov r1, 77
    call _f
    mov [0], r0
    ret
