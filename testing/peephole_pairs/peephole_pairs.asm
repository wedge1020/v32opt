; ===================================================================
; TEST: peephole_pairs - All Scenarios
; Run with: ./v32opt test_pairs.asm -fopt_peephole_pairs -v
; ===================================================================

; ===================================================================
; ✅ PATTERN 1: IEQ/INE + CIB (SHOULD REMOVE CIB)
; ===================================================================

; --- Basic IEQ + CIB ---
__function_test_ieq_cib:
    PUSH BP
    MOV BP, SP
    IEQ R1, R2
    CIB R1
    MOV SP, BP
    POP BP
    RET

; --- Basic INE + CIB ---
__function_test_ine_cib:
    PUSH BP
    MOV BP, SP
    INE R1, R2
    CIB R1
    MOV SP, BP
    POP BP
    RET

; --- Multiple IEQ + CIB ---
__function_test_ieq_cib_multiple:
    PUSH BP
    MOV BP, SP
    IEQ R1, R2
    CIB R1
    IEQ R3, R4
    CIB R3
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ PATTERN 1 GUARD: IEQ/INE + CIB before JT/JF (MUST NOT REMOVE)
; ===================================================================

; --- IEQ + CIB before JT ---
__function_test_ieq_cib_jt:
    PUSH BP
    MOV BP, SP
    IEQ R1, R2
    CIB R1
    JT R1, target
target:
    MOV SP, BP
    POP BP
    RET

; --- IEQ + CIB before JF ---
__function_test_ieq_cib_jf:
    PUSH BP
    MOV BP, SP
    IEQ R1, R2
    CIB R1
    JF R1, target
target:
    MOV SP, BP
    POP BP
    RET

; --- INE + CIB before JT ---
__function_test_ine_cib_jt:
    PUSH BP
    MOV BP, SP
    INE R1, R2
    CIB R1
    JT R1, target
target:
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ PATTERN 2: Self-Inverting Pairs (SHOULD REMOVE BOTH)
; ===================================================================

; --- BNOT x; BNOT x ---
__function_test_bnot_pair:
    PUSH BP
    MOV BP, SP
    BNOT R1
    BNOT R1
    MOV SP, BP
    POP BP
    RET

; --- INEG x; INEG x ---
__function_test_ineg_pair:
    PUSH BP
    MOV BP, SP
    INEG R1
    INEG R1
    MOV SP, BP
    POP BP
    RET

; --- NOT x; NOT x ---
__function_test_not_pair:
    PUSH BP
    MOV BP, SP
    NOT R1
    NOT R1
    MOV SP, BP
    POP BP
    RET

; --- Different registers (MUST NOT REMOVE) ---
__function_test_bnot_diff:
    PUSH BP
    MOV BP, SP
    BNOT R1
    BNOT R2
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ PATTERN 3: XOR Pairs (SHOULD REMOVE BOTH)
; ===================================================================

; --- XOR r1, r2; XOR r1, r2 ---
__function_test_xor_reg_pair:
    PUSH BP
    MOV BP, SP
    XOR R1, R2
    XOR R1, R2
    MOV SP, BP
    POP BP
    RET

; --- XOR r1, 42; XOR r1, 42 ---
__function_test_xor_imm_pair:
    PUSH BP
    MOV BP, SP
    XOR R1, 42
    XOR R1, 42
    MOV SP, BP
    POP BP
    RET

; --- XOR r1, -5; XOR r1, -5 ---
__function_test_xor_neg_pair:
    PUSH BP
    MOV BP, SP
    XOR R1, -5
    XOR R1, -5
    MOV SP, BP
    POP BP
    RET

; --- Different registers (MUST NOT REMOVE) ---
__function_test_xor_diff_reg:
    PUSH BP
    MOV BP, SP
    XOR R1, R2
    XOR R3, R2
    MOV SP, BP
    POP BP
    RET

; --- Different immediates (MUST NOT REMOVE) ---
__function_test_xor_diff_imm:
    PUSH BP
    MOV BP, SP
    XOR R1, 42
    XOR R1, 43
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ PATTERN 4: PUSH/POP Pairs (SHOULD REMOVE BOTH)
; ===================================================================

; --- PUSH r; POP r ---
__function_test_push_pop:
    PUSH BP
    MOV BP, SP
    PUSH R1
    POP R1
    MOV SP, BP
    POP BP
    RET

; --- PUSH r; POP r with comments ---
__function_test_push_pop_comments:
    PUSH BP
    MOV BP, SP
    PUSH R1
    ; comment
    POP R1
    MOV SP, BP
    POP BP
    RET

; --- Multiple PUSH/POP ---
__function_test_push_pop_multiple:
    PUSH BP
    MOV BP, SP
    PUSH R1
    POP R1
    PUSH R2
    POP R2
    MOV SP, BP
    POP BP
    RET

; --- Different registers (MUST NOT REMOVE) ---
__function_test_push_pop_diff:
    PUSH BP
    MOV BP, SP
    PUSH R1
    POP R2
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ COMPLEX: Combined Patterns
; ===================================================================

; --- Multiple patterns in one function ---
__function_test_combined:
    PUSH BP
    MOV BP, SP
    IEQ R1, R2        ; Pattern 1: IEQ + CIB
    CIB R1
    BNOT R3           ; Pattern 2: Self-inverting
    BNOT R3
    XOR R4, R5        ; Pattern 3: XOR pair
    XOR R4, R5
    PUSH R6           ; Pattern 4: PUSH/POP
    POP R6
    MOV SP, BP
    POP BP
    RET
