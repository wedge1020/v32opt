; Liveness gap G5, variant (b): the callee sets up a frame (push bp / mov bp, sp)
; and reads its stack argument as [bp+2]. Nothing after the call reads R1, so
; only the stack argument keeps "mov r1, 77" alive.
; Correct RAM: [0] = 77.
    call _main
    hlt
_id:
    push bp
    mov bp, sp
    mov r0, [bp+2]
    pop bp
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
