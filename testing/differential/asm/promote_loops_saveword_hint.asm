;@hints 1 abi=caller-save
    call __function_main
    hlt
;@fn __g clobbers=R0
__g:
    push bp
    mov bp, sp
    push r5
    mov r0, 9
    jmp __g_out
__g_back:
    pop r5
    mov r0, 0
    mov sp, bp
    pop bp
    ret
__g_out:
    mov [bp-1], r0
    jmp __g_back
;@fn __function_main leaf escapes=none
__function_main:
    push bp
    mov bp, sp
    isub sp, 2
    mov r0, 0
    mov [bp-1], r0
    mov r0, 0
    mov [bp-2], r0
__for_1_start:
    mov r0, [bp-2]
    ilt r0, 4
    jf r0, __for_1_end
    mov r1, [bp-1]
    iadd r1, 10
    mov [bp-1], r1
    mov r2, r1
    mov [10], r2
    mov [11], r1
    mov r3, r1
    iadd r3, 3
    mov [13], r3
    mov r4, r1
    iadd r4, 4
    mov [14], r4
    mov r6, r1
    iadd r6, 6
    mov [16], r6
    mov r7, r1
    iadd r7, 7
    mov [17], r7
    mov r8, r1
    iadd r8, 8
    mov [18], r8
    mov r9, r1
    iadd r9, 9
    mov [19], r9
    mov r10, r1
    iadd r10, 10
    mov [20], r10
    call __g
    mov r0, [bp-2]
    iadd r0, 1
    mov [bp-2], r0
    jmp __for_1_start
__for_1_end:
    mov r0, [bp-1]
    mov [0], r0
    mov sp, bp
    pop bp
    ret
