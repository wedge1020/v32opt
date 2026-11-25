; A register "save" whose saved word is read as data: the callee keeps its
; register argument R1 in its frame ("push r1 ; [bp-1]") and reads it from
; there after reusing R1, then restores R1. The push/pop pair does give the
; caller's R1 back, but the word in between is the argument itself, so R1 IS
; an input of _g and "mov r1, 77" in _main is not dead. (v32lua's
; __tic80_circ_run does this; v32opt took the pair for a plain save and
; dropped the caller's "MOV R1, [BP-14]", which broke TIC-80 circ().)
; Correct RAM: [0] = 77.
    call _main
    hlt
_g:
    push bp
    mov bp, sp
    push r1
    mov r1, 5
    mov r0, [bp-1]
    pop r1
    mov sp, bp
    pop bp
    ret
_main:
    mov r1, 77
    call _g
    mov [0], r0
    ret
