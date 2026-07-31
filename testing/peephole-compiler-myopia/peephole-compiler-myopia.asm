; ============================================================================
; TEST: peephole-compiler-myopia - All Scenarios
; Run with: ./v32opt peephole-compiler-myopia.asm -fpeephole-compiler-myopia -v
; ============================================================================

; ============================================================================
; ✅ SCENARIO 1: Basic store-then-load (SHOULD REMOVE)
; ============================================================================
__function_test_basic:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2   ; Store to memory
    MOV R2, [R1]   ; MATCH(1) Redundant load - remove
    MOV SP, BP
    POP BP
    RET

; ============================================================================
; ✅ SCENARIO 2: With offset (SHOULD REMOVE)
; ============================================================================
__function_test_offset:
    PUSH BP
    MOV BP, SP
    MOV [R1+4], R3
    MOV R3, [R1+4]   ; MATCH(2) Redundant load - remove
    MOV SP, BP
    POP BP
    RET

; ============================================================================
; ✅ SCENARIO 3: Different register names (SHOULD REMOVE)
; ============================================================================
__function_test_diff_regs:
    PUSH BP
    MOV BP, SP
    MOV [R5], R7   ; Store
    MOV R7, [R5]   ; MATCH(3) Redundant load - remove
    MOV SP, BP
    POP BP
    RET

; ============================================================================
; ❌ SCENARIO 4: Different source register (MUST NOT REMOVE)
; ============================================================================
__function_test_diff_src:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2   ; Store
    MOV R3, [R1]   ; KEEP(1) Different register - keep
    MOV SP, BP
    POP BP
    RET

; ============================================================================
; ❌ SCENARIO 5: Different memory location (MUST NOT REMOVE)
; ============================================================================
__function_test_diff_mem:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2   ; Store to R1
    MOV R2, [R3]   ; KEEP(2) Load from different location
    MOV SP, BP
    POP BP
    RET

; ============================================================================
; ❌ SCENARIO 6: Different offset (MUST NOT REMOVE)
; ============================================================================
__function_test_diff_offset:
    PUSH BP
    MOV BP, SP
    MOV [R1+4], R2   ; Store with offset 4
    MOV R2, [R1+8]   ; KEEP(3) Load from different offset
    MOV SP, BP
    POP BP
    RET

; ============================================================================
; ✅ SCENARIO 7: Not consecutive - intervening instruction (SHOULD REMOVE)
; ============================================================================
__function_test_non_consecutive:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2   ; Store
    IADD R3, R4    ; Intervening instruction
    MOV R2, [R1]   ; MATCH(4)
    MOV SP, BP
    POP BP
    RET

; ============================================================================
; ✅ SCENARIO 8: With blank lines/comments between (SHOULD REMOVE)
; ============================================================================
__function_test_with_blanks:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2   ; Store

    ; This is a comment
    MOV R2, [R1]   ; MATCH(5) Redundant load - remove
    MOV SP, BP
    POP BP
    RET

; ============================================================================
; ❌ SCENARIO 9: Control flow boundary between (MUST NOT REMOVE)
; ============================================================================
__function_test_control_boundary:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2   ; Store
__label_skip:
    MOV R2, [R1]   ; KEEP(4) Load after label
    MOV SP, BP
    POP BP
    RET

; ============================================================================
; ❌ SCENARIO 10: Not MOV instructions (MUST NOT REMOVE)
; ============================================================================
__function_test_non_mov:
    PUSH BP
    MOV BP, SP
    LEA R1, [R2]   ; Not a MOV store
    MOV [R2], R1   ; KEEP(5) Not preceded by MOV store
    MOV SP, BP
    POP BP
    RET

; ============================================================================
; ✅ SCENARIO 11: Mixed scenarios (SHOULD OPTIMIZE SELECTIVELY)
; ============================================================================
__function_test_mixed:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2
    MOV R2, [R1]      ; MATCH(6) Remove
    MOV [R3+4], R5
    MOV R5, [R3+4]    ; MATCH(7) Remove
    MOV [R6], R7      ; Store
    MOV R8, [R6]      ; KEEP(6) Different register - keep
    MOV [R9+8], R10   ; Store with offset
    MOV R10, [R9+12]  ; KEEP(7) Different offset - keep
    MOV [R11], R12    ; Store
    IADD R13, R14     ; Intervening instruction, not impacting
    MOV R12, [R11]    ; MATCH(8) Load not immediate
    MOV SP, BP
    POP BP
    RET
