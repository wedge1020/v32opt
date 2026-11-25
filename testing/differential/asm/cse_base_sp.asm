    call __function_main
    hlt
__function_main:
    push bp
    mov bp, sp
    mov r0, 5
    push r0
    mov r0, 6
    push r0
    mov r6, 100
    mov r1, [sp+1]
    iadd r1, r6
    iadd sp, 1
    mov r3, [sp+1]
    iadd r3, r6
    iadd sp, 1
    mov [0], r1
    mov [1], r3
    mov sp, bp
    pop bp
    ret
