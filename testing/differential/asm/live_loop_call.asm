; Loop guard: a call in the loop body whose callee reads R1 (set in every
; iteration) and R2 (set once before the loop).
; _scale: R0 = R1 * 3 + R2. Loop for i = 0..3 accumulates R0 in R8.
; Correct RAM: [0] = 418 (3*(0+1+2+3) + 4*100), [1] = 4.
    call _main
    hlt
_scale:
    mov r0, r1
    imul r0, 3
    iadd r0, r2
    ret
_main:
    mov r2, 100         ; read only by the callee
    mov r7, 0
    mov r8, 0
_loop:
    mov r1, r7          ; read only by the callee
    call _scale
    iadd r8, r0
    iadd r7, 1
    mov r9, r7
    ilt r9, 4
    jt r9, _loop
    mov [0], r8
    mov [1], r7
    ret
