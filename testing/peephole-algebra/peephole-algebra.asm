; ===================================================================
; TEST: peephole-algebra - All Scenarios
; Run with: ./v32opt peephole-algebra.asm -fpeephole-algebra -v
; ===================================================================

; ===================================================================
; ✅ SCENARIO 1: MOV r, r (SHOULD REMOVE - self-move)
; ===================================================================
__function_test_mov:
    PUSH BP
    MOV BP, SP
    MOV R1, R1  ; MATCH
    MOV R2, R2  ; MATCH
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 2: MOV r, s (MUST NOT REMOVE - different registers)
; ===================================================================
__function_test_mov_diff:
    PUSH BP
    MOV BP, SP
    MOV R1, R2  ; KEEP
    MOV R3, R4  ; KEEP
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 3: IADD/ISUB r, 0 (SHOULD REMOVE - identity)
; ===================================================================
__function_test_add_zero:
    PUSH BP
    MOV BP, SP
    IADD R1, 0  ; MATCH
    ISUB R2, 0  ; MATCH
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 4: IADD/ISUB r, non-zero (MUST NOT REMOVE)
; ===================================================================
__function_test_add_nonzero:
    PUSH BP
    MOV BP, SP
    IADD R1, 5   ; KEEP
    ISUB R2, -10   ; KEEP
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 5: IMUL r, 2 (SHOULD REPLACE with IADD r, r)
; ===================================================================
__function_test_mul2:
    PUSH BP
    MOV BP, SP
    IMUL R1, 2  ; MATCH
    IMUL R2, 2  ; MATCH
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 6: IMUL r, non-2/non-0/non-1 (MUST NOT REPLACE)
; ===================================================================
__function_test_mul_other:
    PUSH BP
    MOV BP, SP
    IMUL R1, 3  ; KEEP
    IMUL R2, -2  ; KEEP
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 7: IDIV r, 1 (SHOULD REMOVE - identity)
; ===================================================================
__function_test_div1:
    PUSH BP
    MOV BP, SP
    IDIV R1, 1   ; MATCH
    IDIV R2, 1   ; MATCH
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 8: IDIV r, non-1 (MUST NOT REMOVE)
; ===================================================================
__function_test_div_other:
    PUSH BP
    MOV BP, SP
    IDIV R1, 2   ; KEEP
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 9: IMUL r, 0 (SHOULD REPLACE with MOV r, 0)
; ===================================================================
__function_test_mul0:
    PUSH BP
    MOV BP, SP
    IMUL R1, 0   ; MATCH
    IMUL R2, 0   ; MATCH
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 10: IMUL r, 1 (SHOULD REMOVE - identity)
; ===================================================================
__function_test_mul1:
    PUSH BP
    MOV BP, SP
    IMUL R1, 1   ; MATCH
    IMUL R2, 1   ; MATCH
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 11: Mixed (SHOULD OPTIMIZE SELECTIVELY)
; ===================================================================
__function_test_mixed:
    PUSH BP
    MOV BP, SP
    MOV R1, R1      ; MATCH Remove (self-move)
    IADD R2, 0      ; MATCH Remove (identity)
    IMUL R3, 2      ; MATCH Replace with IADD R3, R3
    IDIV R4, 1      ; MATCH Remove (identity)
    IMUL R5, 0      ; MATCH Replace with MOV R5, 0
    IMUL R6, 1      ; MATCH Remove (identity)
    MOV R7, R8      ; KEEP (different registers)
    IADD R9, 5      ; KEEP (non-zero)
    IMUL R10, 3     ; KEEP (non-2/0/1)
    IDIV R11, 2     ; KEEP (non-1)
    MOV SP, BP
    POP BP
    RET
