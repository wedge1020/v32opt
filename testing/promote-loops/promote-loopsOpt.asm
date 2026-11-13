; ===================================================================
; TEST: promote-loops - Compiler-Style Patterns
; Run with: ./v32opt promote-loops.asm -fpromote-loops -v
;
; Each scenario is its own "__function_" so the whole-function safety checks
; (a frame address that escapes ANYWHERE in the function disables the pass),
; and have the prologue/epilogue the C compiler writes (a promoted register gets
; its PUSH and POP there)
; apply to it alone. Functions that need the frame, call/return behavior
; and the nested/alias/goto cases are covered by testing/promote/.
; ===================================================================

; ===================================================================
; ✅ SCENARIO 1: Simple While Loop (SHOULD PROMOTE)
; Matches typical compiler output for: while (cond) { ... }
; ===================================================================
__function_scenario1:
    PUSH BP
    MOV BP, SP
    ISUB SP, 8
    PUSH R4
    MOV R4, [BP-4]
__while_1_start:
    MOV R1, R4
    IADD R1, 1          ; Use it
    MOV R4, R1
    MOV R5, R4
    JMP __while_1_start ; Unconditional back-edge
__while_1_exit:
    POP R4
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 2: For Loop (SHOULD PROMOTE)
; Matches typical compiler output for: for (i=0; i<N; i++) { ... }
; ===================================================================
__function_scenario2:
    PUSH BP
    MOV BP, SP
    ISUB SP, 8
    PUSH R4
__for_1_start:
    MOV R4, R1
    MOV R2, R4
    IADD R2, 1          ; i++
    MOV R4, R2
    MOV R5, R4
    JMP __for_1_start   ; Back-edge
__for_1_exit:
    POP R4
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 3: Loop with Multiple Locals (SHOULD PROMOTE)
; ===================================================================
__function_scenario3:
    PUSH BP
    MOV BP, SP
    ISUB SP, 8
    PUSH R6
__for_2_start:
    MOV R6, R1
    MOV [BP-8], R2      ; Local var 2
    MOV R3, R6
    MOV R4, [BP-8]      ; Read var 2
    IADD R5, R3
    ISUB R5, R4
    MOV R6, R5
    JMP __for_2_start
__for_2_exit:
    POP R6
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 4: Loop with Exit Label (SHOULD PROMOTE)
; Matches compiler output with explicit exit label
; ===================================================================
__function_scenario4:
    PUSH BP
    MOV BP, SP
    ISUB SP, 8
__while_2_start:
    MOV [BP-4], R1
    MOV R2, [BP-4]
    IADD R2, 1
    MOV [BP-4], R2
    MOV R5, [BP-4]
    JF R1, __while_2_exit ; Conditional exit
    JMP __while_2_start   ; Back-edge
__while_2_exit:
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 5: Loop with CALL (MUST NOT PROMOTE)
; ===================================================================
__function_scenario5:
    PUSH BP
    MOV BP, SP
    ISUB SP, 8
__for_3_start:
    CALL _some_func     ; Safety check fails
    MOV [BP-4], R1
    MOV R2, [BP-4]
    JMP __for_3_start
__for_3_exit:
    MOV SP, BP
    POP BP
    RET
_some_func:

; ===================================================================
; ❌ SCENARIO 6: Loop with BP Usage (MUST NOT PROMOTE)
; ===================================================================
__function_scenario6:
    PUSH BP
    MOV BP, SP
    ISUB SP, 8
__for_4_start:
    MOV R1, BP          ; Direct BP usage
    MOV [BP-4], R1
    MOV R2, [BP-4]
    JMP __for_4_start
__for_4_exit:
    MOV SP, BP
    POP BP
    RET
