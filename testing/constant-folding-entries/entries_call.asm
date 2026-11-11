; The block after a CALL starts with R0-R13 unknown: _clobber changes r6 (and r7), so
; none of "mov r2, r6", "mov r3, r7" and "mov r4, r5" may be folded into an immediate.
    call _main
    hlt
_clobber:
    mov r6, 99
    mov r7, 98
    mov r5, 97
    ret
_main:
    mov r6, 5
    mov r7, 6
    mov r5, 7
    call _clobber
    mov r2, r6
    mov r3, r7
    mov r4, r5
    mov [10], r2
    mov [11], r3
    mov [12], r4
    ret
