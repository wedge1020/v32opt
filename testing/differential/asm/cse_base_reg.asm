    call __function_main
    hlt
__function_main:
    push bp
    mov bp, sp
    mov r6, 100
    mov r0, 5
    mov [10], r0
    mov r0, 6
    mov [11], r0
    mov r2, 10
    mov r1, [r2]
    imul r1, r6
    iadd r2, 1
    mov r3, [r2]
    imul r3, r6
    mov [0], r1
    mov [1], r3
    mov sp, bp
    pop bp
    ret
