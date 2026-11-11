    call _main
    hlt
_two:
    push bp
    mov bp, sp
    mov r0, [bp+3]
    mov sp, bp
    pop bp
    ret
_f:
    push r1
    mov r2, 0
    push r2
    call _two
    iadd sp, 1
    pop r1
    ret
_main:
    mov r1, 77
    call _f
    mov [0], r0
    ret
