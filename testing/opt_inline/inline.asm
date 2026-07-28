; ===================================================================
; TEST: inline - All Scenarios
; Run with: ./v32opt test_inline.asm -fopt_inline -v
; ===================================================================

; ===================================================================
; ✅ SCENARIO 1: Basic Inlining (Single Instruction)
; ===================================================================
__function_test_basic_inline:
    PUSH BP
    MOV BP, SP
    CALL __add_one  ; MATCH
    MOV SP, BP
    POP BP
    RET

__add_one:
    IADD R1, 1  ; KEEP
    RET         ; KEEP

; ===================================================================
; ✅ SCENARIO 2: Inlining with BP-Based Arguments
; ===================================================================
__function_test_bp_args:
    PUSH BP
    MOV BP, SP
    CALL __add_two  ; MATCH
    MOV SP, BP
    POP BP
    RET

__add_two:
    MOV  R2, [BP+2]
    IADD R1, R2     ; KEEP (rewritten to [SP+0] when inlined)
    RET

; ===================================================================
; ❌ SCENARIO 3: Skip Non-Leaf Function (Contains CALL)
; ===================================================================
__function_test_non_leaf:
    PUSH BP
    MOV BP, SP
    CALL __complex  ; KEEP (not inlined: contains CALL)
    MOV SP, BP
    POP BP
    RET

__complex:
    CALL __helper  ; KEEP
    RET            ; KEEP

__helper:
    IADD R1, 1     ; KEEP
    RET            ; KEEP

; ===================================================================
; ❌ SCENARIO 4: Skip Stack Manipulation (PUSH/POP)
; ===================================================================
__function_test_stack_manip:
    PUSH BP
    MOV BP, SP
    CALL __stack_user  ; KEEP (not inlined: uses PUSH/POP)
    MOV SP, BP
    POP BP
    RET

__stack_user:
    PUSH R1  ; KEEP
    POP R2   ; KEEP
    RET      ; KEEP

; ===================================================================
; ❌ SCENARIO 5: Skip Local Variables ([BP-N])
; ===================================================================
__function_test_local_vars:
    PUSH BP
    MOV BP, SP
    CALL __local_user  ; KEEP (not inlined: uses [BP-N])
    MOV SP, BP
    POP BP
    RET

__local_user:
    MOV R1, [BP-4]  ; KEEP (local var)
    RET             ; KEEP

; ===================================================================
; ❌ SCENARIO 6: Inlining Before First Label (Skipped)
; ===================================================================
CALL __early_func  ; KEEP (not inlined: before first label)

%define global_var 42

__early_func:
    IADD R1, 1  ; KEEP
    RET         ; KEEP

; ===================================================================
; ✅ SCENARIO 7: Aggressive Inlining (MAX_BODY_INS=16)
; Run with: ./v32opt test_inline.asm -fopt_inline -finline-max=16 -v
; ===================================================================
__function_test_aggressive:
    PUSH BP
    MOV BP, SP
    CALL __large_func  ; MATCH (inlined if -finline-max=16)
    MOV SP, BP
    POP BP
    RET

__large_func:
    IADD R1, 1   ; KEEP
    IADD R2, 2   ; KEEP
    IADD R3, 3   ; KEEP
    IADD R4, 4   ; KEEP
    IADD R5, 5   ; KEEP
    IADD R6, 6   ; KEEP
    IADD R7, 7   ; KEEP
    IADD R8, 8   ; KEEP
    RET          ; KEEP
