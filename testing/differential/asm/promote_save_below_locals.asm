    call __function_main
    hlt
__function_f:
    push bp
    mov bp, sp
    push r1
    isub sp, 2
    mov r0, 11
    mov [bp-2], r0
    mov r0, 0
    mov [bp-3], r0
    mov r1, 3
__for_1_start:
    mov r4, [bp-3]
    iadd r4, 7
    mov [bp-3], r4
    isub r1, 1
    mov r4, r1
    igt r4, 0
    jt r4, __for_1_start
    mov r0, [bp-3]
    mov r2, [bp-2]
    imul r2, 1000
    iadd r0, r2
__function_f_return:
    iadd sp, 2
    pop r1
    mov sp, bp
    pop bp
    ret
__function_main:
    push bp
    mov bp, sp
    mov r1, 5
    mov r5, 99
    call __function_f
    mov [0], r0
    mov [1], r1
    mov [2], r5
    mov sp, bp
    pop bp
    ret
