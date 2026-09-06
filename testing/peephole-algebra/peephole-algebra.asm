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
    MOV R1, R1  ; MATCH(1)
    MOV R2, R2  ; MATCH(2)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 2: MOV r, s (MUST NOT REMOVE - different registers)
; ===================================================================
__function_test_mov_diff:
    PUSH BP
    MOV BP, SP
    MOV R1, R2  ; KEEP(1)
    MOV R3, R4  ; KEEP(2)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 3: IADD/ISUB r, 0 (SHOULD REMOVE - identity)
; ===================================================================
__function_test_add_zero:
    PUSH BP
    MOV BP, SP
    IADD R1, 0  ; MATCH(3)
    ISUB R2, 0  ; MATCH(4)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 4: IADD/ISUB r, non-zero (MUST NOT REMOVE)
; ===================================================================
__function_test_add_nonzero:
    PUSH BP
    MOV BP, SP
    IADD R1, 5   ; KEEP(3)
    ISUB R2, -10   ; KEEP(4)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 5: IMUL r, 2 (SHOULD REPLACE with IADD r, r)
; ===================================================================
__function_test_mul2:
    PUSH BP
    MOV BP, SP
    IMUL R1, 2  ; MATCH(5)
    IMUL R2, 2  ; MATCH(6)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 6: IMUL r, non-2/non-0/non-1 (MUST NOT REPLACE)
; ===================================================================
__function_test_mul_other:
    PUSH BP
    MOV BP, SP
    IMUL R1, 3  ; KEEP(5)
    IMUL R2, -2  ; KEEP(6)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 7: IDIV r, 1 (SHOULD REMOVE - identity)
; ===================================================================
__function_test_div1:
    PUSH BP
    MOV BP, SP
    IDIV R1, 1   ; MATCH(7)
    IDIV R2, 1   ; MATCH(8)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 8: IDIV r, non-1 (MUST NOT REMOVE)
; ===================================================================
__function_test_div_other:
    PUSH BP
    MOV BP, SP
    IDIV R1, 2   ; KEEP(7)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 9: IMUL r, 0 (SHOULD REPLACE with MOV r, 0)
; ===================================================================
__function_test_mul0:
    PUSH BP
    MOV BP, SP
    IMUL R1, 0   ; MATCH(9)
    IMUL R2, 0   ; MATCH(10)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 10: IMUL r, 1 (SHOULD REMOVE - identity)
; ===================================================================
__function_test_mul1:
    PUSH BP
    MOV BP, SP
    IMUL R1, 1   ; MATCH(11)
    IMUL R2, 1   ; MATCH(12)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 11: Mixed (SHOULD OPTIMIZE SELECTIVELY)
; ===================================================================
__function_test_mixed:
    PUSH BP
    MOV BP, SP
    MOV R1, R1      ; MATCH(13) Remove (self-move)
    IADD R2, 0      ; MATCH(14) Remove (identity)
    IMUL R3, 2      ; MATCH(15) Replace with IADD R3, R3
    IDIV R4, 1      ; MATCH(16) Remove (identity)
    IMUL R5, 0      ; MATCH(17) Replace with MOV R5, 0
    IMUL R6, 1      ; MATCH(18) Remove (identity)
    MOV R7, R8      ; KEEP(8) (different registers)
    IADD R9, 5      ; KEEP(9) (non-zero)
    IMUL R10, 3     ; KEEP(10) (non-2/0/1)
    IDIV R11, 2     ; KEEP(11) (non-1)
    MOV SP, BP
    POP BP
    RET
