    call _main
    hlt
_g:
    mov r1, 5
    mov r0, 1
    ret
_f:
    call _g
    ret
_main:
    call _f
    mov [0], r0
    mov [1], r1
    ret
