    call _main
    hlt
_g:
    push bp
    mov bp, sp
    push r1
    mov r1, 5
    jmp _g_out
_g_back:
    pop r1
    mov sp, bp
    pop bp
    ret
_g_out:
    mov r0, [bp-1]
    jmp _g_back
_main:
    mov r1, 77
    call _g
    mov [0], r0
    ret
