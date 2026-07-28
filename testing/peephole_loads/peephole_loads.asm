; ===================================================================
; TEST: peephole_loads - Comprehensive Test Suite
; Run with: ./v32opt test_loads.asm -fopt_peephole_loads -v -g
; ===================================================================

; ===================================================================
; ✅ SCENARIO 1: Basic Consecutive Loads
; ===================================================================
__function_test_basic:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2]
    MOV R3, [R2]        ; MATCH: MOV R3, R1
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
    MOV R3, [BP+4]      ; MATCH: MOV R3, R1
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
    MOV R3, [BP-8]      ; MATCH: MOV R3, R1
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 4: Different Base Registers (MUST KEEP)
; ===================================================================
__function_test_diff_reg:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2]
    MOV R3, [R4]        ; KEEP: MOV R3, [R4]
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 5: Different Offsets (MUST KEEP)
; ===================================================================
__function_test_diff_offset:
    PUSH BP
    MOV BP, SP
    MOV R1, [BP+4]
    MOV R3, [BP+8]      ; KEEP: MOV R3, [BP+8]
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
    MOV R3, [R2]        ; MATCH: MOV R3, R1
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
    MOV R3, [R2]        ; MATCH: MOV R3, R1
    MOV R4, [R2]        ; MATCH: MOV R4, R1
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 8: Base Register Overwritten by Load (MUST KEEP)
; ===================================================================
__function_test_self_clobber:
    PUSH BP
    MOV BP, SP
    MOV R2, [R2]        ; R2 now holds loaded value, not original address
    MOV R3, [R2]        ; KEEP: MOV R3, [R2]
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 9: Base Register Modified Between Loads (MUST KEEP)
; ===================================================================
__function_test_base_modified:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2]
    IADD R2, 4          ; Base address pointer changed
    MOV R3, [R2]        ; KEEP: MOV R3, [R2]
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 10: Value Register Modified Between Loads (MUST KEEP)
; ===================================================================
__function_test_val_modified:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2]
    MOV R1, 100         ; R1 no longer holds memory value
    MOV R3, [R2]        ; KEEP: MOV R3, [R2]
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 11: Intervening Memory Store (MUST KEEP)
; ===================================================================
__function_test_store_intervening:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2]
    MOV [R5], R6        ; Memory store might invalidate cached memory
    MOV R3, [R2]        ; KEEP: MOV R3, [R2]
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 12: Intervening Function Call (MUST KEEP)
; ===================================================================
__function_test_call_intervening:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2]
    CALL __some_function ; Function call clobbers registers and memory
    MOV R3, [R2]        ; KEEP: MOV R3, [R2]
    MOV SP, BP
    POP BP
    RET
__some_function:

; ===================================================================
; ❌ SCENARIO 13: Intervening Label / Control Boundary (MUST KEEP)
; ===================================================================
__function_test_label_intervening:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2]
_loop_header:
    MOV R3, [R2]        ; KEEP: MOV R3, [R2]
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 14: Non-Register Destination (MUST KEEP)
; ===================================================================
__function_test_non_reg_dst:
    PUSH BP
    MOV BP, SP
	MOV R4, [R2]
    MOV [R1], R4        ; Indirect store/copy
    MOV R3, [R2]        ; KEEP: MOV R3, [R2]
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
    MOV R3, [R2+4]      ; MATCH: MOV R3, R1
    MOV R5, [R6-8]
    MOV R7, [R6-8]      ; MATCH: MOV R7, R5
    MOV SP, BP
    POP BP
    RET
