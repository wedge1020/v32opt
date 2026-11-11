; Control: pinning must not be too broad. _l is only the target of a plain JT and is entered
; by the jump and by falling through, both with r6 = 5, so "mov r2, r6" there still folds
; into "mov r2, 5" (as does the one in the same block as "mov r6, 5").
    call _main
    hlt
_main:
    mov r6, 5
    mov r0, [20]
    jt r0, _l
    mov r4, 1
_l:
    mov r2, r6
    mov [14], r2
    mov r3, r6
    mov [15], r3
    ret
