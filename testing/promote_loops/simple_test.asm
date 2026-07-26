; ===================================================================
; MINIMAL TEST: Should trigger promote_loops
; ===================================================================
__for_1_start:
    MOV [BP-4], R1      ; Write to slot
    MOV R2, [BP-4]      ; Read from slot (total uses = 2)
    JMP __for_1_start   ; Unconditional back-edge
__for_1_exit:
    RET
