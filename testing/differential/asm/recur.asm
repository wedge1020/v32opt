; Recursion with stack arguments and registers kept across the calls.
    call _main
    hlt

_fact:                           ; _fact(n) = n <= 1 ? 1 : n * _fact(n-1)
    push bp
    mov bp, sp
    push r1
    mov r1, [bp+2]
    mov r0, 1
    ile r0, r1
    mov r0, r1
    ile r0, 1
    jt r0, _base
    mov r0, r1
    isub r0, 1
    push r0
    call _fact
    iadd sp, 1
    imul r0, r1
    jmp _out
_base:
    mov r0, 1
_out:
    pop r1
    mov sp, bp
    pop bp
    ret

_main:
    push bp
    mov bp, sp
    mov r1, 4
    mov r2, 9
    push r1
    call _fact
    iadd sp, 1
    mov [0], r0
    mov [1], r1                 ; R1 saved by _fact
    mov [2], r2
    mov r3, r1
    iadd r3, r2
    mov r1, r3
    mov [3], r1
    mov sp, bp
    pop bp
    ret
