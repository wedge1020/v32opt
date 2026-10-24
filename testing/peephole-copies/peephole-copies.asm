; ===================================================================
; TEST: peephole-copies - register copy propagation and coalescing
; Run with: ./v32opt peephole-copies.asm -fpeephole-copies -v
;
; Coalescing:   MOV Rt, Rs / ...window... / MOV Rs, Rt
;     with Rs untouched in a straight-line window and Rt dead afterwards
;     becomes the window renamed onto Rs, both copies gone.
; Propagation:  after MOV Rt, Rs the reads of Rt read Rs, up to the next
;     write of either; the copy goes when Rt is dead.
; ===================================================================
%define global_x 1

    CALL __function_main
    HLT

; ===================================================================
; ✅ SECTION 1: coalescing
; ===================================================================
__function_counter_step:
    MOV R1, R4          ; MATCH(1) copy in
    IADD R1, 1
    MOV R4, R1          ; MATCH(2) copy out; R1 is dead
    MOV R0, R4
    RET

__function_accumulate:
    MOV R0, R5          ; MATCH(3) copy in
    MOV R1, global_x
    IADD R1, R4
    MOV R1, [R1]
    IADD R0, R1
    MOV R5, R0          ; MATCH(4) copy out; R0 is overwritten below
    MOV R0, 0
    RET

__function_pointer_base:
    MOV R2, R6          ; MATCH(5)
    MOV R1, [R2+3]
    IADD R2, 1
    MOV R6, R2          ; MATCH(6)
    MOV R0, R1
    RET

; ===================================================================
; ✅ SECTION 2: propagation (the copy then dies)
; ===================================================================
__function_propagate:
    MOV R1, R6          ; MATCH(7) every read of R1 becomes a read of R6
    ILT R0, R1
    JF R0, __p_end
    MOV R0, R6
__p_end:
    RET

; ===================================================================
; ❌ SECTION 3: must be kept
; ===================================================================
__function_source_in_window:
    MOV R1, R4          ; KEEP R4 is written inside the window
    IADD R4, 1
    IADD R1, R4
    MOV R4, R1          ; KEEP
    CALL __function_reads_r4
    RET

__function_copy_live_after:
    MOV R1, R4          ; KEEP R1 is read after the copy-out
    IADD R1, 1
    MOV R4, R1          ; KEEP
    MOV R0, R1
    CALL __function_reads_r4
    RET

__function_label_in_window:
    MOV R1, R4          ; KEEP a label breaks the straight line
    IADD R1, 1
__w_label:
    IADD R1, 2
    MOV R4, R1          ; KEEP
    CALL __function_reads_r4
    RET

__function_call_in_window:
    MOV R1, R4          ; KEEP the CALL ends the window
    CALL __function_leaf
    MOV R4, R1          ; KEEP
    CALL __function_reads_r4
    RET

__function_leaf:
    MOV R0, 0
    RET

__function_reads_r4:
    MOV R0, R4
    RET

__function_rmw_not_renamed:
    MOV R1, R4          ; KEEP R1 is also modified in place below
    ILT R1, 10
    MOV R0, R1
    RET

__function_copy_still_needed:
    MOV R1, R4          ; KEEP R4 is rewritten, R1 is read after
    MOV R4, 0
    MOV R0, R1
    RET

__function_main:
    CALL __function_counter_step
    CALL __function_propagate
    RET
