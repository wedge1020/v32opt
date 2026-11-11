; Loop guard: R1 is loop-carried (written at the end of the body, read at the top
; of the next iteration) and dead after the loop. The last write is dead only on
; the exit path, never on the back edge.
; R1: 1 -> r2 += r1 ; r1 = r2 + 3. Four iterations.
; Correct RAM: [0] = 29 (sum), [1] = 4 (count).
    call _main
    hlt
_main:
    mov r1, 1
    mov r2, 0
    mov r3, 0
_loop:
    iadd r2, r1         ; read of the carried value
    mov r1, r2
    iadd r1, 3          ; carried to the next iteration
    iadd r3, 1
    mov r4, r3
    ilt r4, 4
    jt r4, _loop
    mov [0], r2
    mov [1], r3
    mov r1, 0           ; R1 dead after the loop
    ret
