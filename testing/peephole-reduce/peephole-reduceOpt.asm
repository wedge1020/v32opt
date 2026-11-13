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
    MOV R1, 0
    MOV SP, BP
    POP BP
    RET

; --- Multiply by 0 on multiple registers ---
__function_test_imul_by_zero_multi:
    PUSH BP
    MOV BP, SP
    MOV R2, 0
    MOV R3, 0
    MOV SP, BP
    POP BP
    RET

; --- Multiply by 1 → Remove ---
__function_test_imul_by_one:
    PUSH BP
    MOV BP, SP
    MOV SP, BP
    POP BP
    RET

; --- Multiply by 2 → IADD r, r ---
__function_test_imul_by_two:
    PUSH BP
    MOV BP, SP
    IADD R1, R1
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
    MOV R1, 0
    IADD R3, R3
    MOV SP, BP
    POP BP
    RET

__function_test_reduce_mixed:
    PUSH BP
    MOV BP, SP
    MOV R1, 0
    IMUL R2, 3         ; KEEP(7) No reduction
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
    FMUL R1, 0.0         ; KEEP(13) NOT an identity: Inf/NaN * 0.0 = NaN, -x*0.0 = -0.0
    MOV SP, BP
    POP BP
    RET

; ✅ FLOATING-POINT MULTIPLY BY ONE
__function_test_fmul_one:
    PUSH BP
    MOV BP, SP
    MOV SP, BP
    POP BP
    RET

; ✅ FLOATING-POINT DIVIDE BY ONE
__function_test_fdiv_one:
    PUSH BP
    MOV BP, SP
    MOV SP, BP
    POP BP
    RET

; ✅ INTEGER MODULUS BY ONE
__function_test_imod_one:
    PUSH BP
    MOV BP, SP
    MOV R1, 0
    MOV SP, BP
    POP BP
