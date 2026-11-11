; Liveness gap G1, variant (a): the callee address makes a round trip through a
; RAM word before "call r5", so the address is not visible as an immediate in
; the same basic block. The scratch word [100] is cleared again at the end
; (code addresses change with the code size and must not show in the RAM image).
; _pair returns R0 = 1, R1 = 2; the caller reads both.
; Correct RAM: [0] = 1, [1] = 2.
    call _main
    hlt
_pair:                  ; only called through a register: R0 = 1, R1 = 2
    mov r0, 1
    mov r1, 2
    ret
_main:
    mov r5, _pair
    mov [100], r5
    mov r5, 0
    mov r5, [100]
    call r5
    mov [0], r0
    mov [1], r1
    mov r6, 0
    mov [100], r6       ; scrub the code address
    ret
