; ===================================================================
; TEST: peephole-reduce - Strength Reduction
; Run with: ./v32opt peephole-reduce.asm -fpeephole-reduce -v
; ===================================================================

; ===================================================================
; ✅ SECTION 1: IMUL Strength Reduction
; ===================================================================

; --- Multiply by 0 → MOV 0 ---
__function_test_imul_by_zero:
    PUSH BP
    MOV BP, SP
    IMUL R1, 0         ; MATCH(1) Should become: MOV R1, 0
    MOV SP, BP
    POP BP
    RET

; --- Multiply by 0 on multiple registers ---
__function_test_imul_by_zero_multi:
    PUSH BP
    MOV BP, SP
    IMUL R2, 0         ; MATCH(2) Should become: MOV R2, 0
    IMUL R3, 0         ; MATCH(3) Should become: MOV R3, 0
    MOV SP, BP
    POP BP
    RET

; --- Multiply by 1 → Remove ---
__function_test_imul_by_one:
    PUSH BP
    MOV BP, SP
    IMUL R1, 1         ; MATCH(4) Should be removed
    MOV SP, BP
    POP BP
    RET

; --- Multiply by 2 → IADD r, r ---
__function_test_imul_by_two:
    PUSH BP
    MOV BP, SP
    IMUL R1, 2         ; MATCH(5) Should become: IADD R1, R1
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SECTION 2: IDIV Strength Reduction
; ===================================================================

; --- Divide by 1 → Remove ---
__function_test_idiv_by_one:
    PUSH BP
    MOV BP, SP
    IDIV R1, 1         ; MATCH(6) Should be removed
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SECTION 3: Guard Conditions (must KEEP)
; ===================================================================

; --- Non-reducible IMUL values ---
__function_test_imul_no_reduce:
    PUSH BP
    MOV BP, SP
    IMUL R1, 3         ; KEEP(1) No reduction pattern
    IMUL R2, -5        ; KEEP(2) Negative value
    MOV SP, BP
    POP BP
    RET

; --- Float IMUL (SHOULD NOT reduce) ---
__function_test_imul_float:
    PUSH BP
    MOV BP, SP
    FMUL R1, 2.0       ; KEEP(3) Float immediate
    MOV SP, BP
    POP BP
    RET

; --- Register operand IMUL (SHOULD NOT reduce) ---
__function_test_imul_reg:
    PUSH BP
    MOV BP, SP
    IMUL R1, R2        ; KEEP(4) Register source
    MOV SP, BP
    POP BP
    RET

; --- Non-reducible IDIV values ---
__function_test_idiv_no_reduce:
    PUSH BP
    MOV BP, SP
    IDIV R1, 2         ; KEEP(5) No reduction pattern
    IDIV R2, R3        ; KEEP(6) Register source
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SECTION 4: Combined Reduction Scenarios
; ===================================================================

__function_test_reduce_combined:
    PUSH BP
    MOV BP, SP
    IMUL R1, 0         ; MATCH(7) Should become: MOV R1, 0
    IMUL R2, 1         ; MATCH(8) Should be removed
    IMUL R3, 2         ; MATCH(9) Should become: IADD R3, R3
    IDIV R4, 1         ; MATCH(10) Should be removed
    MOV SP, BP
    POP BP
    RET

__function_test_reduce_mixed:
    PUSH BP
    MOV BP, SP
    IMUL R1, 0         ; MATCH(11) Should become: MOV R1, 0
    IMUL R2, 3         ; KEEP(7) No reduction
    IMUL R3, 1         ; MATCH(12) Should be removed
    IDIV R4, 2         ; KEEP(8) No reduction
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; TEST: peephole-reduce - Floating-Point & Modulus Extensions
; ===================================================================

; ✅ FLOATING-POINT MULTIPLY BY ZERO
__function_test_fmul_zero:
    PUSH BP
    MOV BP, SP
    FMUL R1, 0.0         ; MATCH(13) → MOV R1, 0.0
    MOV SP, BP
    POP BP
    RET

; ✅ FLOATING-POINT MULTIPLY BY ONE
__function_test_fmul_one:
    PUSH BP
    MOV BP, SP
    FMUL R1, 1.0         ; MATCH(14) - Removed
    MOV SP, BP
    POP BP
    RET

; ✅ FLOATING-POINT DIVIDE BY ONE
__function_test_fdiv_one:
    PUSH BP
    MOV BP, SP
    FDIV R1, 1.0         ; MATCH(15) - Removed
    MOV SP, BP
    POP BP
    RET

; ✅ INTEGER MODULUS BY ONE
__function_test_imod_one:
    PUSH BP
    MOV BP, SP
    IMOD R1, 1           ; MATCH(16) → MOV R1, 0
    MOV SP, BP
    POP BP
