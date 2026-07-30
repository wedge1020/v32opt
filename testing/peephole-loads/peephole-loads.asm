; ===================================================================
; TEST: peephole-loads - Comprehensive Test Suite
; Run with: ./v32opt peephole-loads.asm -fpeephole-loads -v -g
; ===================================================================

; ===================================================================
; ✅ SCENARIO 1: Basic Consecutive Loads
; ===================================================================
__function_test_basic:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2]
    MOV R3, [R2]        ; MATCH(1): MOV R3, R1
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 2: Loads with Positive Offset
; ===================================================================
__function_test_offset:
    PUSH BP
    MOV BP, SP
    MOV R1, [BP+4]
    MOV R3, [BP+4]      ; MATCH(2): MOV R3, R1
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 3: Loads with Negative Offset
; ===================================================================
__function_test_neg_offset:
    PUSH BP
    MOV BP, SP
    MOV R1, [BP-8]
    MOV R3, [BP-8]      ; MATCH(3): MOV R3, R1
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 4: Different Base Registers (must keep)
; ===================================================================
__function_test_diff_reg:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2]
    MOV R3, [R4]        ; KEEP(1)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 5: Different Offsets (must keep)
; ===================================================================
__function_test_diff_offset:
    PUSH BP
    MOV BP, SP
    MOV R1, [BP+4]
    MOV R3, [BP+8]      ; KEEP(2)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 6: Comments & Whitespace Between Loads (SHOULD OPTIMIZE)
; ===================================================================
__function_test_comments:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2]
    ; Comments do not modify memory or registers
    MOV R3, [R2]        ; MATCH(4): MOV R3, R1
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 7: Three Consecutive Loads
; ===================================================================
__function_test_three:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2]
    MOV R3, [R2]        ; MATCH(5): MOV R3, R1
    MOV R4, [R2]        ; MATCH(6): MOV R4, R1
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 8: Base Register Overwritten by Load (must keep)
; ===================================================================
__function_test_self_clobber:
    PUSH BP
    MOV BP, SP
    MOV R2, [R2]        ; R2 now holds loaded value, not original address
    MOV R3, [R2]        ; KEEP(3)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 9: Base Register Modified Between Loads (must keep)
; ===================================================================
__function_test_base_modified:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2]
    IADD R2, 4          ; Base address pointer changed
    MOV R3, [R2]        ; KEEP(4)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 10: Value Register Modified Between Loads (must keep)
; ===================================================================
__function_test_val_modified:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2]
    MOV R1, 100         ; R1 no longer holds memory value
    MOV R3, [R2]        ; KEEP(5)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 11: Intervening Memory Store (must keep)
; ===================================================================
__function_test_store_intervening:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2]
    MOV [R5], R6        ; Memory store might invalidate cached memory
    MOV R3, [R2]        ; KEEP(6)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 12: Intervening Function Call (must keep)
; ===================================================================
__function_test_call_intervening:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2]
    CALL __some_function ; Function call clobbers registers and memory
    MOV R3, [R2]        ; KEEP(7)
    MOV SP, BP
    POP BP
    RET
__some_function:

; ===================================================================
; ❌ SCENARIO 13: Intervening Label / Control Boundary (must keep)
; ===================================================================
__function_test_label_intervening:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2]
_loop_header:
    MOV R3, [R2]        ; KEEP(8)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 14: Non-Register Destination (must keep)
; ===================================================================
__function_test_non_reg_dst:
    PUSH BP
    MOV BP, SP
	MOV R4, [R2]
    MOV [R1], R4        ; Indirect store/copy
    MOV R3, [R2]        ; KEEP(9)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 15: Mixed Registers and Offsets
; ===================================================================
__function_test_mixed:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2+4]
    MOV R3, [R2+4]      ; MATCH(7): MOV R3, R1
    MOV R5, [R6-8]
    MOV R7, [R6-8]      ; MATCH(8): MOV R7, R5
    MOV SP, BP
    POP BP
    RET

; ✅ MULTIPLE REDUNDANT LOADS (3+)
__function_test_triple_loads:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2+4]
    MOV R3, [R2+4]      ; MATCH(9) → MOV R3, R1
    MOV R4, [R2+4]      ; MATCH(10) → MOV R4, R1
    MOV R5, [R2+4]      ; MATCH(11) → MOV R5, R1
    MOV SP, BP
    POP BP
    RET

; ❌ GUARD: Floating-point with intervening store
__function_test_fp_load_guard_store:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2]        ; Load float
    MOV [R3], R4        ; Store could alias [R2]
    MOV R5, [R2]        ; KEEP(10) - memory may have changed
    MOV SP, BP
    POP BP
    RET
