; The first block of the program is also the target of the back edge. Its only CFG
; predecessor sets r1 = 5, but on entry (from the boot) r1 is whatever it was: "mov r2, r1"
; must not become "mov r2, 5". r6 collects r2 of both passes.
_first:
    mov r2, r1
    iadd r6, r2
    mov r1, 5
    iadd r4, 1
    mov r0, r4
    ilt r0, 2
    jt r0, _first
    mov [30], r6
    mov [31], r4
    hlt
