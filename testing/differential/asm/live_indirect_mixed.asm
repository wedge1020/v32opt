; Liveness gap G1, variant (c): _f is called BOTH directly (_direct_user ignores
; R1 and nothing after the call reads it) and indirectly (_indirect_user reads
; R1). The direct call sites alone must not decide that R1 is dead at _f's RET.
; The two callers are separate functions so that the indirect call is not on any
; path from the direct call.
; Correct RAM: [0] = 1 (R0, direct call), [1] = 1 (R0, indirect), [2] = 2 (R1).
    call _main
    hlt
_f:                     ; R0 = 1, R1 = 2
    mov r0, 1
    mov r1, 2
    ret
_direct_user:
    call _f             ; R1 ignored
    mov [0], r0
    ret
_indirect_user:
    mov r5, _f
    call r5             ; R1 read
    mov [1], r0
    mov [2], r1
    ret
_main:
    call _direct_user
    call _indirect_user
    ret
