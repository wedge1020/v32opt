; ===================================================================
; TEST: constant_folding - Global Constant Propagation
; Run with: ./v32opt test_constant_folding.asm -fopt_constant_folding -v
; ===================================================================

; ===================================================================
; ✅ SCENARIO 1: Simple Constant Propagation
; ===================================================================
__function_simple_prop:
    PUSH BP
    MOV BP, SP
    MOV R1, 42
    MOV R2, R1          ; Should become: MOV R2, 0x2A (42 in hex)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 2: Chain Propagation
; ===================================================================
__function_chain_prop:
    PUSH BP
    MOV BP, SP
    MOV R1, 10
    MOV R2, R1          ; → MOV R2, 0xA
    MOV R3, R2          ; → MOV R3, 0xA
    MOV R4, R3          ; → MOV R4, 0xA
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 3: Arithmetic Propagation
; ===================================================================
__function_arith_prop:
    PUSH BP
    MOV BP, SP
    MOV R1, 5
    IADD R1, 3          ; R1 = 8
    MOV R2, R1          ; Should become: MOV R2, 0x8
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 4: Conditional Branch Propagation
; Constants propagate through JT/JF
; ===================================================================
__function_branch_prop:
    PUSH BP
    MOV BP, SP
    MOV R1, 1
    JT R1, skip        ; R1=1, so this jumps
    MOV R1, 0
skip:
    MOV R2, R1          ; Should become: MOV R2, 0x1 (R1=1 at skip)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 5: Multiple Constants
; ===================================================================
__function_multi_const:
    PUSH BP
    MOV BP, SP
    MOV R1, 10
    MOV R2, 20
    MOV R3, R1          ; → MOV R3, 0xA
    MOV R4, R2          ; → MOV R4, 0x14
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 6: CALL Invalidation
; CALL clobbers all registers
; ===================================================================
__function_call_invalidate:
    PUSH BP
    MOV BP, SP
    MOV R1, 42
    CALL some_function  ; R1 becomes VAL_BOTTOM
    MOV R2, R1          ; Should NOT be folded (R1 unknown after CALL)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 7: Across Blocks
; ===================================================================
__function_across_blocks:
    PUSH BP
    MOV BP, SP
    MOV R1, 100
    JMP block2
block2:
    MOV R2, R1          ; Should become: MOV R2, 0x64
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 8: Complex Propagation
; ===================================================================
__function_complex:
    PUSH BP
    MOV BP, SP
    MOV R1, 5
    MOV R2, R1          ; → MOV R2, 0x5
    IADD R2, 3          ; R2 = 8
    MOV R3, R2          ; → MOV R3, 0x8
    ISUB R3, 2          ; R3 = 6
    MOV R4, R3          ; → MOV R4, 0x6
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 9: Non-Constant Source
; ===================================================================
__function_non_const:
    PUSH BP
    MOV BP, SP
    IN R1, 0x10         ; R1 is not constant
    MOV R2, R1          ; Should NOT be folded
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 10: Loop with Constant
; ===================================================================
__for_1_start:
    MOV R1, 10
    MOV R2, R1          ; → MOV R2, 0xA
    IADD R1, 1
    JT R1, __for_1_start
__for_1_exit:
    RET

; ===================================================================
; ✅ SCENARIO 11: Conditional with Constants
; ===================================================================
__function_conditional_const:
    PUSH BP
    MOV BP, SP
    MOV R1, 5
    MOV R2, 10
    IEQ R3, R1          ; R3 = (5 == 10) = 0
    MOV R4, R3          ; Should become: MOV R4, 0x0
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 12: Zero and Negative Constants
; ===================================================================
__function_zero_neg:
    PUSH BP
    MOV BP, SP
    MOV R1, 0
    MOV R2, R1          ; → MOV R2, 0x0
    MOV R3, -5
    MOV R4, R3          ; → MOV R4, 0xFFFFFFFF (or implementation-defined)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 13: Large Constants
; ===================================================================
__function_large_const:
    PUSH BP
    MOV BP, SP
    MOV R1, 16777216    ; 0x1000000
    MOV R2, R1          ; → MOV R2, 0x1000000
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 14: Mixed Constants and Variables
; ===================================================================
__function_mixed:
    PUSH BP
    MOV BP, SP
    MOV R1, 10
    IN R2, 0x20         ; R2 is not constant
    MOV R3, R1          ; → MOV R3, 0xA
    MOV R4, R2          ; Should NOT be folded
    MOV SP, BP
    POP BP
    RET
