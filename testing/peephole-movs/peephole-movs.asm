; ===================================================================
; TEST: peephole_movs - Comprehensive Test Suite
; Run with: ./v32opt test_movs.asm -fopt_peephole_movs -v -g
; ===================================================================

; ===================================================================
; ✅ PATTERN 1: Duplicate Move Elimination (Register)
; ===================================================================

; --- Basic duplicate ---
__function_test_dup_reg:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R1, R2          ; MATCH
    MOV SP, BP
    POP BP
    RET

; --- Multiple duplicates ---
__function_test_dup_reg_multiple:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R1, R2          ; MATCH
    MOV R3, R4
    MOV R3, R4          ; MATCH
    MOV SP, BP
    POP BP
    RET

; --- Three consecutive duplicates ---
__function_test_dup_reg_triple:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R1, R2          ; MATCH
    MOV R1, R2          ; MATCH
    MOV SP, BP
    POP BP
    RET

; --- With comments & whitespace ---
__function_test_dup_reg_comments:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    ; Inline comment should not block optimization
    MOV R1, R2          ; MATCH
    MOV SP, BP
    POP BP
    RET

; --- Duplicate across independent instruction (Forward Scan) ---
__function_test_dup_forward_scan:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    IADD R5, 10         ; Does not modify R1 or R2
    MOV R1, R2          ; MATCH
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ PATTERN 1 GUARD: Different Sources & Register Hazards
; ===================================================================

; --- Different source registers ---
__function_test_dup_diff_src:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R1, R3          ; KEEP
    MOV SP, BP
    POP BP
    RET

; --- Different destination ---
__function_test_dup_diff_dst:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R3, R2          ; KEEP
    MOV SP, BP
    POP BP
    RET

; --- Destination modified between moves (must keep) ---
__function_test_dup_dst_modified:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    IADD R1, 5          ; R1 modified, second move is not redundant
    MOV R1, R2          ; KEEP
    MOV SP, BP
    POP BP
    RET

; --- Source modified between moves (must keep) ---
__function_test_dup_src_modified:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    IADD R2, 5          ; R2 modified, second move loads new value
    MOV R1, R2          ; KEEP
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ PATTERN 1: Duplicate Move Elimination (Immediate)
; ===================================================================

; --- Immediate value ---
__function_test_dup_imm:
    PUSH BP
    MOV BP, SP
    MOV R1, 42
    MOV R1, 42          ; MATCH
    MOV SP, BP
    POP BP
    RET

; --- Negative immediate ---
__function_test_dup_neg_imm:
    PUSH BP
    MOV BP, SP
    MOV R1, -5
    MOV R1, -5          ; MATCH
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ PATTERN 1: Duplicate Move Elimination (Indirect)
; ===================================================================

; --- Indirect address ---
__function_test_dup_indirect:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2]
    MOV R1, [R2]        ; MATCH
    MOV SP, BP
    POP BP
    RET

; --- Indirect with offset ---
__function_test_dup_indirect_offset:
    PUSH BP
    MOV BP, SP
    MOV R1, [BP+4]
    MOV R1, [BP+4]      ; MATCH
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ PATTERN 1 GUARD: Self-Referential Load & Memory Hazards
; ===================================================================

; --- Self-referential load: MOV r, [r]; MOV r, [r] ---
__function_test_self_ref:
    PUSH BP
    MOV BP, SP
    MOV R1, [R1]        ; R1 clobbered with memory value
    MOV R1, [R1]        ; KEEP
    MOV SP, BP
    POP BP
    RET

; --- Self-referential load with offset ---
__function_test_self_ref_offset:
    PUSH BP
    MOV BP, SP
    MOV R1, [R1+4]      ; Address pointer clobbered
    MOV R1, [R1+4]      ; KEEP
    MOV SP, BP
    POP BP
    RET

; --- Different destination for self-ref load ---
__function_test_self_ref_diff_dst:
    PUSH BP
    MOV BP, SP
    MOV R1, [R1]
    MOV R2, [R1]        ; KEEP: (R1 address was clobbered!)
    MOV SP, BP
    POP BP
    RET

; --- Intervening memory write hazard (must keep) ---
__function_test_indirect_mem_hazard:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2]
    MOV [R5], R6        ; Memory store might alias and overwrite [R2]
    MOV R1, [R2]        ; KEEP
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ PATTERN 2: Mirror Move Elimination
; ===================================================================

; --- Basic mirror ---
__function_test_mirror:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R2, R1          ; MATCH
    MOV SP, BP
    POP BP
    RET

; --- Multiple mirrors ---
__function_test_mirror_multiple:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R2, R1          ; MATCH
    MOV R3, R4
    MOV R4, R3          ; MATCH
    MOV SP, BP
    POP BP
    RET

; --- Mirror with comments ---
__function_test_mirror_comments:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    ; comment
    MOV R2, R1          ; MATCH
    MOV SP, BP
    POP BP
    RET

; --- Mirror across independent instruction (Forward Scan) ---
__function_test_mirror_forward_scan:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R5, R6          ; Independent move
    MOV R2, R1          ; MATCH
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ PATTERN 2 GUARD: Not Mirror & Register Hazards
; ===================================================================

; --- Different registers ---
__function_test_mirror_diff:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R3, R4          ; KEEP
    MOV SP, BP
    POP BP
    RET

; --- Partial mirror ---
__function_test_mirror_partial:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R2, R3          ; KEEP
    MOV SP, BP
    POP BP
    RET

; --- Register modified before mirror (must keep) ---
__function_test_mirror_modified:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    IADD R1, 1          ; R1 changed, values no longer mirrored
    MOV R2, R1          ; KEEP
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ CONTROL FLOW BOUNDARY GUARDS (must keep)
; ===================================================================

; --- Intervening Label ---
__function_test_boundary_label:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
_loop_target:
    MOV R1, R2          ; KEEP
    MOV SP, BP
    POP BP
    RET

; --- Intervening Function Call ---
__function_test_boundary_call:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    CALL __some_function ; Calls clobber registers
    MOV R1, R2          ; KEEP
    MOV SP, BP
    POP BP
    RET
__some_function:

; --- Intervening Conditional Branch ---
__function_test_boundary_branch:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    JT R5, _skip
    MOV R1, R2          ; KEEP
_skip:
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ COMPLEX: Mixed Patterns
; ===================================================================

; --- Duplicate + Mirror + Immediates ---
__function_test_mixed:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R1, R2          ; MATCH
    MOV R3, R4
    MOV R4, R3          ; MATCH
    MOV R5, 10
    MOV R5, 10          ; MATCH
    MOV SP, BP
    POP BP
    RET
