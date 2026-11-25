    call __function_main
    hlt
__function_f:
    push bp
    mov bp, sp
    push r1
    mov r0, 0
    mov [bp-1], r0
    mov r2, 4
__for_1_start:
    mov r0, [bp-1]
    iadd r0, 5
    mov [bp-1], r0
    isub r2, 1
    mov r4, r2
    igt r4, 0
    jt r4, __for_1_start
__function_f_return:
    pop r1
    mov sp, bp
    pop bp
    ret
__function_main:
    push bp
    mov bp, sp
    mov r1, 1
    call __function_f
    mov [0], r1
    mov sp, bp
    pop bp
    ret
