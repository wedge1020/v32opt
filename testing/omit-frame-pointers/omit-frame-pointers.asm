; ===================================================================
; TEST: omit_frame_pointers - All Scenarios (Enhanced)
; Run with: ./v32opt omit-frame-pointers.asm -fomit-frame-pointers -v
; ===================================================================

; ===================================================================
; ✅ SCENARIO 1: Simple Function (SHOULD ELIMINATE)
; No BP usage, standard prologue/epilogue
; ===================================================================
__function_simple:
    PUSH BP ; MATCH
    MOV BP, SP ; MATCH
    MOV R1, 42
    MOV R2, 100
    MOV SP, BP ; MATCH
    POP BP ; MATCH
    RET

; ===================================================================
; ✅ SCENARIO 2: With Return Label (SHOULD ELIMINATE)
; Return label between body and epilogue
; ===================================================================
__function_with_return_label:
    PUSH BP ; MATCH
    MOV BP, SP ; MATCH
    MOV R1, 42
    MOV R2, 100
__function_with_return_label_return:
    MOV SP, BP ; MATCH
    POP BP ; MATCH
    RET

; ===================================================================
; ✅ SCENARIO 3: Local Variables (SHOULD ELIMINATE)
; Uses [BP-N] but not BP register directly
; ===================================================================
__function_local_vars:
    PUSH BP ; MATCH
    MOV BP, SP ; MATCH
    MOV [BP-4], R1
    MOV R2, [BP-4]
    MOV [BP-8], R3
    MOV R4, [BP-8]
    MOV SP, BP ; MATCH
    POP BP ; MATCH
    RET

; ===================================================================
; ✅ SCENARIO 4: Multiple Local Variables (SHOULD ELIMINATE)
; ===================================================================
__function_many_locals:
    PUSH BP ; MATCH
    MOV BP, SP ; MATCH
    MOV [BP-4], R1
    MOV [BP-8], R2
    MOV [BP-12], R3
    MOV R4, [BP-4]
    MOV R5, [BP-8]
    MOV R6, [BP-12]
    MOV SP, BP ; MATCH
    POP BP ; MATCH
    RET

; ===================================================================
; ❌ SCENARIO 5: Direct BP Usage (MUST NOT ELIMINATE)
; Uses BP register directly
; ===================================================================
__function_direct_bp:
    PUSH BP ; KEEP
    MOV BP, SP ; KEEP
    MOV R1, BP          ; Direct BP usage
    MOV R2, 42
    MOV SP, BP ; KEEP
    POP BP ; KEEP
    RET

; ===================================================================
; ❌ SCENARIO 6: BP in Arithmetic (MUST NOT ELIMINATE)
; ===================================================================
__function_bp_arith:
    PUSH BP ; KEEP
    MOV BP, SP ; KEEP
    IADD R1, BP        ; BP in arithmetic
    ISUB R2, BP
    MOV SP, BP ; KEEP
    POP BP ; KEEP
    RET

; ===================================================================
; ❌ SCENARIO 7: BP in Indirect (MUST NOT ELIMINATE)
; ===================================================================
__function_bp_indirect:
    PUSH BP ; KEEP
    MOV BP, SP ; KEEP
    MOV R1, [BP]       ; BP in indirect
    MOV [BP+4], R2
    MOV SP, BP ; KEEP
    POP BP ; KEEP
    RET

; ===================================================================
; ❌ SCENARIO 8: Nested Control Flow (MUST NOT ELIMINATE)
; Has internal labels
; ===================================================================
__function_nested:
    PUSH BP ; KEEP
    MOV BP, SP ; KEEP
    MOV R1, 1
    JT R1, _loop_start
    JMP _loop_end
_loop_start:
    IADD R1, 1
    JT R1, _loop_start
_loop_end:
    MOV SP, BP ; KEEP
    POP BP ; KEEP
    RET

; ===================================================================
; ❌ SCENARIO 9: Missing Prologue (MUST NOT ELIMINATE)
; ===================================================================
__function_no_prologue:
    MOV R1, 42
    MOV SP, BP ; KEEP
    POP BP ; KEEP
    RET

; ===================================================================
; ❌ SCENARIO 10: Missing Epilogue (MUST NOT ELIMINATE)
; ===================================================================
__function_no_epilogue:
    PUSH BP ; KEEP
    MOV BP, SP ; KEEP
    MOV R1, 42
    RET

; ===================================================================
; ❌ SCENARIO 11: Partial Prologue (MUST NOT ELIMINATE)
; Only PUSH BP, missing MOV BP, SP
; ===================================================================
__function_partial_prologue:
    PUSH BP ; KEEP
    MOV R1, 42
    MOV SP, BP ; KEEP
    POP BP ; KEEP
    RET

; ===================================================================
; ❌ SCENARIO 12: Partial Epilogue (MUST NOT ELIMINATE)
; Only POP BP, missing MOV SP, BP
; ===================================================================
__function_partial_epilogue:
    PUSH BP ; KEEP
    MOV BP, SP ; KEEP
    MOV R1, 42
    POP BP ; KEEP
    RET

; ===================================================================
; ✅ SCENARIO 13: Empty Function (SHOULD ELIMINATE)
; ===================================================================
__function_empty:
    PUSH BP ; MATCH
    MOV BP, SP ; MATCH
    MOV SP, BP ; MATCH
    POP BP ; MATCH
    RET

; ===================================================================
; ✅ SCENARIO 14: Function with CALL (SHOULD ELIMINATE)
; CALL doesn't use BP directly
; ===================================================================
__function_with_call:
    PUSH BP ; MATCH
    MOV BP, SP ; MATCH
    CALL _some_func
    MOV R1, 42
    MOV SP, BP ; MATCH
    POP BP ; MATCH
    RET
_some_func:

; ===================================================================
; ❌ SCENARIO 15: Function Modifying BP (MUST NOT ELIMINATE)
; ===================================================================
__function_modify_bp:
    PUSH BP ; KEEP
    MOV BP, SP ; KEEP
    MOV BP, R1          ; Modifies BP
    MOV R2, 42
    MOV SP, BP ; KEEP
    POP BP ; KEEP
    RET

; ===================================================================
; ✅ SCENARIO 16: Complex Function (SHOULD ELIMINATE)
; Multiple operations, no BP usage
; ===================================================================
__function_complex:
    PUSH BP ; MATCH
    MOV BP, SP ; MATCH
    MOV R1, 10
    IADD R2, R1
    ISUB R3, 5
    IMUL R4, 2
    IDIV R5, 2
    MOV SP, BP ; MATCH
    POP BP ; MATCH
    RET

; ===================================================================
; ❌ SCENARIO 17: SP Modified (MUST NOT ELIMINATE)
; Direct SP modification invalidates frame elimination
; ===================================================================
__function_sp_modified:
    PUSH BP ; KEEP
    MOV BP, SP ; KEEP
    ISUB SP, 16        ; Allocate stack space - uses SP directly
    MOV R1, 42
    MOV R2, 100
    MOV SP, BP ; KEEP
    POP BP ; KEEP
    RET

; ===================================================================
; ❌ SCENARIO 18: SP in Indirect Mode (MUST NOT ELIMINATE)
; Uses [SP] for stack operations - depends on frame
; ===================================================================
__function_sp_indirect:
    PUSH BP ; KEEP
    MOV BP, SP ; KEEP
    MOV R1, 42
    MOV [SP], R1       ; Store on stack using SP directly
    MOV R2, [SP]       ; Load from stack
    MOV SP, BP ; KEEP
    POP BP ; KEEP
    RET

; ===================================================================
; ❌ SCENARIO 19: Mixed SP and BP Usage (MUST NOT ELIMINATE)
; Uses both SP and BP in ways that depend on frame pointer
; ===================================================================
__function_mixed_sp_bp:
    PUSH BP ; KEEP
    MOV BP, SP ; KEEP
    ISUB SP, 8         ; Allocate stack space
    MOV [BP-4], R1     ; Local variable
    MOV [SP], R2       ; Stack access
    MOV R3, [BP-4]     ; Local variable
    MOV R4, [SP]       ; Stack access
    MOV SP, BP ; KEEP
    POP BP ; KEEP
    RET

; ===================================================================
; ❌ SCENARIO 20: SP Allocation with Offsets (MUST NOT ELIMINATE)
; Multiple SP-based stack operations
; ===================================================================
__function_sp_alloc:
    PUSH BP ; KEEP
    MOV BP, SP ; KEEP
    ISUB SP, 24        ; Allocate space for 6 words
    MOV [SP], R1
    MOV [SP+4], R2
    MOV [SP+8], R3
    MOV [SP+12], R4
    MOV [SP+16], R5
    MOV [SP+20], R6
    CALL _helper
    MOV SP, BP ; KEEP
    POP BP ; KEEP
    RET
_helper:
    RET
