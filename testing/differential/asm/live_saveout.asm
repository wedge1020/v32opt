    call _main
    hlt
_f:
    push r1
    mov r1, 3
    mov r0, r1
    pop r1
    mov r1, 9
    ret
_main:
    call _f
    mov [0], r0
    mov [1], r1
    ret
