; ===================================================================
; TEST: peephole-immediate-prop - All Patterns
; Run with: ./v32opt peephole-immediate-prop.asm -fpeephole-immediate-prop -v
; ===================================================================

; ===================================================================
; ✅ PATTERN 1: Identity Math Elimination
; ===================================================================

; --- IADD r, 0 ---
__function_test_iadd_zero:
    PUSH BP
    MOV BP, SP
    IADD R1, 0          ; MATCH(1): Should be removed
    MOV SP, BP
    POP BP
    RET

; --- ISUB r, 0 ---
__function_test_isub_zero:
    PUSH BP
    MOV BP, SP
    ISUB R1, 0          ; MATCH(2): Should be removed
    MOV SP, BP
    POP BP
    RET

; --- IMUL r, 1 ---
__function_test_imul_one:
    PUSH BP
    MOV BP, SP
    IMUL R1, 1          ; MATCH(3): Should be removed
    MOV SP, BP
    POP BP
    RET

; --- IDIV r, 1 ---
__function_test_idiv_one:
    PUSH BP
    MOV BP, SP
    IDIV R1, 1          ; MATCH(4): Should be removed
    MOV SP, BP
    POP BP
    RET

; --- Multiple identity ops ---
__function_test_identity_multiple:
    PUSH BP
    MOV BP, SP
    IADD R1, 0          ; MATCH(5): Removed
    ISUB R2, 0          ; MATCH(6): Removed
    IMUL R3, 1          ; MATCH(7): Removed
    IDIV R4, 1          ; MATCH(8): Removed
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ PATTERN 1 GUARD: Non-Identity
; ===================================================================

; --- IADD r, non-zero ---
__function_test_non_identity_add:
    PUSH BP
    MOV BP, SP
    IADD R1, 5          ; KEEP(1): Not identity - unchanged
    MOV SP, BP
    POP BP
    RET

; --- IMUL r, non-1 ---
__function_test_non_identity_mul:
    PUSH BP
    MOV BP, SP
    IMUL R1, 2          ; KEEP(2): Not identity - unchanged
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ PATTERN 2: Constant Folding (MOV + ALU)
; ===================================================================

; --- MOV + IADD ---
__function_test_fold_add:
    PUSH BP
    MOV BP, SP
    MOV R1, 5
    IADD R1, 3          ; MATCH(9): Should become: MOV R1, 8
    MOV SP, BP
    POP BP
    RET

; --- MOV + ISUB ---
__function_test_fold_sub:
    PUSH BP
    MOV BP, SP
    MOV R1, 10
    ISUB R1, 4          ; MATCH(10): Should become: MOV R1, 6
    MOV SP, BP
    POP BP
    RET

; --- MOV + IMUL ---
__function_test_fold_mul:
    PUSH BP
    MOV BP, SP
    MOV R1, 5
    IMUL R1, 3          ; MATCH(11): Should become: MOV R1, 15
    MOV SP, BP
    POP BP
    RET

; --- With comments ---
__function_test_fold_comments:
    PUSH BP
    MOV BP, SP
    MOV R1, 5
    ; comment
    IADD R1, 3          ; MATCH(12): Should become: MOV R1, 8
    MOV SP, BP
    POP BP
    RET

; --- Multiple folds ---
__function_test_fold_multiple:
    PUSH BP
    MOV BP, SP
    MOV R1, 2
    IADD R1, 3          ; MATCH(13): → MOV R1, 5
    MOV R2, 10
    ISUB R2, 4          ; MATCH(14): → MOV R2, 6
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ PATTERN 2 GUARD: Different Registers
; ===================================================================

; --- MOV R1, x; IADD R2, y ---
__function_test_fold_diff_reg:
    PUSH BP
    MOV BP, SP
    MOV R1, 5
    IADD R2, 3          ; KEEP(3): Different register - unchanged
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ PATTERN 2 GUARD: Not ALU
; ===================================================================

; --- MOV R1, x; MOV R1, y ---
__function_test_fold_not_alu:
    PUSH BP
    MOV BP, SP
    MOV R1, 5
    MOV R1, 3           ; KEEP(4): Not ALU - unchanged
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ PATTERN 2 GUARD: POW/ATAN2
; ===================================================================

; --- MOV + POW ---
__function_test_fold_pow:
    PUSH BP
    MOV BP, SP
    MOV R1, 2
    POW R2, R1          ; KEEP(5): Should NOT fold (POW guard)
    MOV SP, BP
    POP BP
    RET

; --- MOV + ATAN2 ---
__function_test_fold_atan2:
    PUSH BP
    MOV BP, SP
    MOV R1, 5
    ATAN2 R2, R1        ; KEEP(6): Should NOT fold (ATAN2 guard)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ PATTERN 3: Sequential Math Combining
; ===================================================================

; --- IADD + IADD ---
__function_test_combine_add_add:
    PUSH BP
    MOV BP, SP
    IADD R1, 5
    IADD R1, 3          ; MATCH(15): Should become: IADD R1, 8
    MOV SP, BP
    POP BP
    RET

; --- IADD + ISUB ---
__function_test_combine_add_sub:
    PUSH BP
    MOV BP, SP
    IADD R1, 10
    ISUB R1, 4          ; MATCH(16): Should become: IADD R1, 6
    MOV SP, BP
    POP BP
    RET

; --- ISUB + IADD ---
__function_test_combine_sub_add:
    PUSH BP
    MOV BP, SP
    ISUB R1, 5
    IADD R1, 2          ; MATCH(17): Should become: ISUB R1, 3
    MOV SP, BP
    POP BP
    RET

; --- ISUB + ISUB ---
__function_test_combine_sub_sub:
    PUSH BP
    MOV BP, SP
    ISUB R1, 5
    ISUB R1, 3          ; MATCH(18): Should become: ISUB R1, 8
    MOV SP, BP
    POP BP
    RET

; --- With comments ---
__function_test_combine_comments:
    PUSH BP
    MOV BP, SP
    IADD R1, 5
    ; comment
    IADD R1, 3          ; MATCH(19): Should become: IADD R1, 8
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ PATTERN 3 GUARD: Different Registers
; ===================================================================

; --- IADD R1, x; IADD R2, y ---
__function_test_combine_diff_reg:
    PUSH BP
    MOV BP, SP
    IADD R1, 5
    IADD R2, 3          ; KEEP(7): Different register - unchanged
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ PATTERN 3 GUARD: Non-Consecutive
; ===================================================================

; --- Code between operations ---
__function_test_combine_non_consec:
    PUSH BP
    MOV BP, SP
    IADD R1, 5
    MOV R2, 10          ; Breaks consecutiveness
    IADD R1, 3          ; KEEP(8): Not consecutive - unchanged
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ PATTERN 3: Three in a Row
; ===================================================================

; --- IADD + IADD + IADD ---
__function_test_combine_three:
    PUSH BP
    MOV BP, SP
    IADD R1, 2
    IADD R1, 3          ; MATCH(20): Should become: IADD R1, 5
    IADD R1, 1          ; MATCH(21): Then combine with previous: IADD R1, 6
    MOV SP, BP
    POP BP
    RET

; --- Mixed operations ---
__function_test_combine_three_mixed:
    PUSH BP
    MOV BP, SP
    IADD R1, 10
    ISUB R1, 3          ; MATCH(22): Should become: IADD R1, 7
    IADD R1, 2          ; MATCH(23): Then combine: IADD R1, 9
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ COMPLEX: All Patterns Combined
; ===================================================================

__function_test_complex:
    PUSH BP
    MOV BP, SP
    ; Pattern 1: Identity
    IADD R1, 0          ; MATCH(24): Removed
    ; Pattern 2: Constant folding
    MOV R2, 5
    IADD R2, 3          ; MATCH(25): → MOV R2, 8
    ; Pattern 3: Sequential combining
    IADD R3, 10
    ISUB R3, 4          ; → IADD R3, 6
    ; Pattern 1: More identity
    IMUL R4, 1          ; MATCH(26): Removed
    ; Pattern 2: More folding
    MOV R5, 7
    IMUL R5, 2          ; MATCH(27): → MOV R5, 14
    MOV SP, BP
    POP BP
    RET
