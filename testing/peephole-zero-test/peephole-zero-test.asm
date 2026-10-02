; ===================================================================
; TEST: peephole-zero-test - compare-with-zero + conditional jump
; Run with: ./v32opt peephole-zero-test.asm -fpeephole-zero-test -v
;
; JT jumps when its register is non-zero, JF when it is zero, so
;     MOV Ra, Rb / IEQ Ra, 0 / JT Ra, L     is just     JF Rb, L
; provided Ra is never read afterwards on EITHER side of the branch.
; ===================================================================
    CALL __function_main
    HLT

; ===================================================================
; ✅ SECTION 1: Form A (test a copy) -- all four compare/jump pairs
; ===================================================================
__function_test_ieq_jt:
    MOV R0, R4          ; MATCH(1) removed
    IEQ R0, 0           ; MATCH(2) removed
    JT R0, __a1_zero    ; becomes: JF R4, __a1_zero
    MOV R0, 1
    RET
__a1_zero:
    MOV R0, 2
    RET

__function_test_ieq_jf:
    MOV R0, R4          ; MATCH(3) removed
    IEQ R0, 0           ; MATCH(4) removed
    JF R0, __a2_nonzero ; becomes: JT R4, __a2_nonzero
    MOV R0, 1
    RET
__a2_nonzero:
    MOV R0, 2
    RET

__function_test_ine_jt:
    MOV R0, R4          ; MATCH(5) removed
    INE R0, 0           ; MATCH(6) removed
    JT R0, __a3_nonzero ; becomes: JT R4, __a3_nonzero
    MOV R0, 1
    RET
__a3_nonzero:
    MOV R0, 2
    RET

__function_test_ine_jf:
    MOV R0, R4          ; MATCH(7) removed
    INE R0, 0           ; MATCH(8) removed
    JF R0, __a4_zero    ; becomes: JF R4, __a4_zero
    MOV R0, 1
    RET
__a4_zero:
    MOV R0, 2
    RET

; ===================================================================
; ✅ SECTION 2: Form B (test in place) -- register dead afterwards
; ===================================================================
__function_test_in_place:
    IEQ R5, 0           ; MATCH(9) removed
    JT R5, __b1_zero    ; becomes: JF R5, __b1_zero
    MOV R5, 7
    MOV R0, R5
    RET
__b1_zero:
    MOV R5, 9
    MOV R0, R5
    RET

; ===================================================================
; ✅ SECTION 3: Dead through a CALL -- the callee overwrites R0 before
; reading it, so the scratch value cannot be observed.
; ===================================================================
__function_test_dead_through_call:
    MOV R0, R4          ; MATCH(10) removed
    IEQ R0, 0           ; MATCH(11) removed
    JT R0, __c1_slow    ; becomes: JF R4, __c1_slow
    MOV R0, [R4]
    RET
__c1_slow:
    PUSH R4
    CALL __helper_overwrites_r0
    IADD SP, 1
    RET

__helper_overwrites_r0:
    MOV R0, [SP+1]
    IADD R0, 1
    RET

; ===================================================================
; ✅ SECTION 4: Zero given as an integer %define
; ===================================================================
%define NULL_PTR 0
__function_test_define_zero:
    MOV R0, R4          ; MATCH(12) removed
    IEQ R0, NULL_PTR    ; MATCH(13) removed
    JT R0, __d1_null    ; becomes: JF R4, __d1_null
    MOV R0, 1
    RET
__d1_null:
    MOV R0, 2
    RET

; ===================================================================
; ❌ SECTION 5: must KEEP -- comparison result is still used
; ===================================================================
__function_keep_live_fallthrough:
    MOV R0, R4          ; KEEP
    IEQ R0, 0           ; KEEP
    JT R0, __k1_zero    ; KEEP: R0 (0 here) is returned below
    RET
__k1_zero:
    MOV R0, 2
    RET

__function_keep_live_at_target:
    MOV R0, R4          ; KEEP
    IEQ R0, 0           ; KEEP
    JT R0, __k2_zero    ; KEEP: target reads R0 (1 there)
    MOV R0, 5
    RET
__k2_zero:
    IADD R0, 10
    RET

__function_keep_callee_reads:
    MOV R0, R4          ; KEEP
    IEQ R0, 0           ; KEEP
    JT R0, __k3_slow    ; KEEP: the callee reads R0 as an argument
    MOV R0, 5
    RET
__k3_slow:
    CALL __helper_reads_r0
    RET

__helper_reads_r0:
    IADD R0, 100
    RET

__function_keep_in_place_live:
    IEQ R5, 0           ; KEEP
    JT R5, __k4_zero    ; KEEP: R5 is stored on the fall-through side
    MOV [R6], R5
    MOV R0, 0
    RET
__k4_zero:
    MOV R0, 1
    RET

; ===================================================================
; ❌ SECTION 6: must KEEP -- not this pattern
; ===================================================================
__function_keep_nonzero_compare:
    MOV R0, R4          ; KEEP
    IEQ R0, 3           ; KEEP: compared with 3, not 0
    JT R0, __k5_hit
    MOV R0, 1
    RET
__k5_hit:
    MOV R0, 2
    RET

__function_keep_float_compare:
    MOV R0, R4          ; KEEP
    FEQ R0, 0.0         ; KEEP: -0.0 == 0.0 for FEQ, but not for JF
    JT R0, __k6_hit
    MOV R0, 1
    RET
__k6_hit:
    MOV R0, 2
    RET

__function_keep_label_between:
    MOV R0, R4          ; KEEP
    IEQ R0, 0           ; KEEP
__k7_entry:             ; a second way in: R0 is not the comparison result
    JT R0, __k7_hit     ; KEEP
    MOV R0, 1
    RET
__k7_hit:
    MOV R0, 2
    RET

__function_keep_computed_target:
    MOV R0, R4          ; KEEP
    IEQ R0, 0           ; KEEP
    JT R0, R7           ; KEEP: computed target, cannot be followed
    MOV R0, 1
    RET

__function_main:
    CALL __function_test_ieq_jt
    CALL __function_test_ieq_jf
    CALL __function_test_ine_jt
    CALL __function_test_ine_jf
    CALL __function_test_in_place
    CALL __function_test_dead_through_call
    CALL __function_test_define_zero
    CALL __function_keep_live_fallthrough
    CALL __function_keep_live_at_target
    CALL __function_keep_callee_reads
    CALL __function_keep_in_place_live
    CALL __function_keep_nonzero_compare
    CALL __function_keep_float_compare
    CALL __function_keep_label_between
    CALL __function_keep_computed_target
    JMP __k7_entry
