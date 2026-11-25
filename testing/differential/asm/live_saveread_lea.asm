    call _main
    hlt
_g:
    push bp
    mov bp, sp
    push r1
    mov r1, 5
    lea r2, [bp-2]
    iadd r2, 1
    mov r0, [r2]
    pop r1
    mov sp, bp
    pop bp
    ret
_main:
    mov r1, 77
    call _g
    mov [0], r0
    ret
