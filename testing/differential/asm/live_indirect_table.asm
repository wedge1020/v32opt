; Liveness gap G1, variant (b): two functions reached through a jump table
; (pointer _f1, _f2) and called in a loop with "call r5". Neither has a direct
; caller. Each returns R0 and a second result in R1.
; Correct RAM: [0] = 11, [1] = 21 (R1 of _f1, _f2), [2] = 10, [3] = 20 (R0).
    call _main
    hlt
_f1:                    ; R0 = 10, R1 = 11
    mov r0, 10
    mov r1, 11
    ret
_f2:                    ; R0 = 20, R1 = 21
    mov r0, 20
    mov r1, 21
    ret
_main:
    mov r7, 0           ; i
_loop:
    mov r6, _tab
    iadd r6, r7
    mov r5, [r6]
    call r5
    mov r8, r7
    mov [r8], r1        ; [i] = R1
    iadd r8, 2
    mov [r8], r0        ; [i+2] = R0
    iadd r7, 1
    mov r9, r7
    ilt r9, 2
    jt r9, _loop
    ret
_tab:
    pointer _f1, _f2
