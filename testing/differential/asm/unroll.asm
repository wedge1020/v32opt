; Counted loops with a constant trip count (loop-unroll): continue/break
; inside, a live loop test register, counting down, nesting, and loops that
; must NOT be unrolled (counter written in the body, a CALL, too many trips).
; Results go to RAM[0..]; run.py compares the whole image.
    call _main
    hlt

_bump:                           ; r1 += 1 (a callee that clobbers r4!)
    mov r4, 1000
    iadd r1, 1
    ret

_main:
    ; A: continue (i==2) and break (i==5); r0 and r4 are read afterwards
    mov r4, 0
    mov r5, 0
_a_start:
    mov r0, r4
    ilt r0, 8
    jf r0, _a_end
    mov r1, r4
    ieq r1, 2
    jf r1, _a_c1
    jmp _a_cont
_a_c1:
    mov r1, r4
    ieq r1, 5
    jf r1, _a_nobreak
    jmp _a_end
_a_nobreak:
    iadd r5, r4
_a_cont:
    iadd r4, 1
    jmp _a_start
_a_end:
    mov [0], r5
    mov [1], r4
    mov [2], r0

    ; B: counting down by 3: 10, 7, 4, 1
    mov r4, 10
    mov r6, 0
_b_start:
    mov r0, r4
    igt r0, 0
    jf r0, _b_end
    imul r6, 2
    iadd r6, r4
    isub r4, 3
    jmp _b_start
_b_end:
    mov [3], r6
    mov [4], r4
    mov [5], r0

    ; C: nested 3 x 3, the inner loop unrolls first
    mov r4, 0
_c_outer:
    mov r0, r4
    ilt r0, 3
    jf r0, _c_oend
    mov r5, 0
_c_inner:
    mov r0, r5
    ilt r0, 3
    jf r0, _c_iend
    mov r1, r4
    imul r1, 3
    iadd r1, r5
    mov r2, r1
    iadd r1, 10
    imul r2, r2
    mov [r1], r2
    iadd r5, 1
    jmp _c_inner
_c_iend:
    iadd r4, 1
    jmp _c_outer
_c_oend:
    mov [6], r4
    mov [7], r5

    ; D: the body writes the counter -> not unrolled (runs i = 0, 2, 4)
    mov r4, 0
    mov r6, 0
_d_start:
    mov r0, r4
    ilt r0, 5
    jf r0, _d_end
    iadd r6, 1
    iadd r4, 1
    iadd r4, 1
    jmp _d_start
_d_end:
    mov [8], r6

    ; E: a CALL in the body (the callee trashes r4) -> not unrolled
    mov r4, 0
    mov r1, 0
_e_start:
    mov r0, r4
    ilt r0, 3
    jf r0, _e_end
    call _bump
    iadd r4, 1
    jmp _e_start
_e_end:
    mov [9], r1

    ; F: 16 trips (unrolled) and 17 trips (left alone)
    mov r4, 0
    mov r6, 0
_f_start:
    mov r0, r4
    ilt r0, 16
    jf r0, _f_end
    iadd r6, r4
    iadd r4, 1
    jmp _f_start
_f_end:
    mov [30], r6
    mov r4, 0
    mov r6, 0
_g_start:
    mov r0, r4
    ilt r0, 17
    jf r0, _g_end
    iadd r6, r4
    iadd r4, 1
    jmp _g_start
_g_end:
    mov [31], r6
    ret
