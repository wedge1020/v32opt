; Liveness across branches, loops and a jump table.
    call _main
    hlt

_main:
    mov r1, 5
    mov r2, 0
    mov r3, 7
_loop:
    iadd r2, r1
    isub r1, 1
    mov r4, r1
    ine r4, 0
    jt r4, _loop                 ; R4: dead after the branch
    mov [0], r2
    mov r3, 1                   ; dead on the path that overwrites it
    jf r2, _skipset
    mov r3, 2
_skipset:
    mov [1], r3
    mov r5, 3
    jt r2, _readit
    mov r5, 4
    jmp _done
_readit:
    mov [2], r5                 ; R5 live on this path only
_done:
    mov r6, _tab                 ; jump table: targets are not visible to liveness
    iadd r6, 1
    mov r7, [r6]
    mov r8, 55
    jmp r7
_tab:
    pointer _t0
    pointer _t1
_t0:
    mov [3], r8
    jmp _end
_t1:
    mov r8, 66
    mov [3], r8
_end:
    mov r9, r2
    iadd r9, 1
    mov r2, r9
    mov [4], r2
    ret
