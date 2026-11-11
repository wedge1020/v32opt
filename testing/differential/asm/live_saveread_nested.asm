; Liveness gap G3, variant (c): _outer saves R1 with push/pop around a call to
; _inner, and _inner reads R1 as a register argument. The save is real (R1 is
; restored) but the value is also read in between, by the nested callee. Nothing
; after the call reads R1, so only the nested read keeps "mov r1, 5" alive.
; Correct RAM: [0] = 15 (R1 + 10).
    call _main
    hlt
_inner:                 ; R0 = R1 + 10
    mov r0, r1
    iadd r0, 10
    ret
_outer:
    push r1
    call _inner
    pop r1
    ret
_main:
    mov r1, 5
    call _outer
    mov [0], r0
    ret
