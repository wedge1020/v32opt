; Loop guard: a loop with two exits; R5 is live at exit A only.
; _run(R3): counts R1 up; leaves at R1 == R3 (exit A, returns R5) or at R1 == 5
; (exit B, returns R1). R5 starts at 77 and grows by 2 per iteration.
; Correct RAM: [0] = 81 (R3 = 2: exit A at the 2nd iteration), [1] = 5 (R3 = 9).
    call _main
    hlt
_run:
    mov r1, 0
    mov r5, 77
_loop:
    iadd r1, 1
    iadd r5, 2
    mov r2, r1
    ieq r2, r3
    jt r2, _exit_a
    mov r2, r1
    ieq r2, 5
    jt r2, _exit_b
    jmp _loop
_exit_a:
    mov r0, r5
    ret
_exit_b:
    mov r0, r1
    ret
_main:
    mov r3, 2
    call _run
    mov [0], r0
    mov r3, 9
    call _run
    mov [1], r0
    ret
