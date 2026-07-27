; ===================================================================
; TEST: peephole_forwarding - All Scenarios
; Run with: ./v32opt test_forwarding.asm -fopt_peephole_forwarding -v
; ===================================================================

; ===================================================================
; ✅ RULE 1: Store-to-Load Forwarding
; ===================================================================

; --- Basic store-to-load ---
__function_test_store_load:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2
    MOV R3, [R1]        ; MATCH Should become: MOV R3, R2
    MOV SP, BP
    POP BP
    RET

; --- With offset ---
__function_test_store_load_offset:
    PUSH BP
    MOV BP, SP
    MOV [BP+4], R2
    MOV R3, [BP+4]     ; MATCH Should become: MOV R3, R2
    MOV SP, BP
    POP BP
    RET

; --- Memory overwritten (SHOULD NOT forward) ---
__function_test_store_load_overwrite:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2
    MOV [R1], R3       ; KEEP Overwrites memory
    MOV R4, [R1]       ; KEEP Should NOT forward (memory changed)
    MOV SP, BP
    POP BP
    RET

; --- Across control flow (SHOULD NOT forward) ---
__function_test_store_load_cfb:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2
    JMP _skip
    MOV R3, [R1]       ; KEEP Should NOT forward (across JMP)
_skip:
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ RULE 2: Copy Propagation (Register)
; ===================================================================

; --- Basic register propagation ---
__function_test_copy_prop_reg:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R3, R1          ; MATCH Should become: MOV R3, R2
    MOV SP, BP
    POP BP
    RET

; --- Through comments ---
__function_test_copy_prop_comments:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    ; comment
    MOV R3, R1          ; MATCH Should become: MOV R3, R2
    MOV SP, BP
    POP BP
    RET

; --- Source register modified (SHOULD NOT propagate) ---
__function_test_copy_prop_modified:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R1, R3         ; KEEP Overwrites R1
    MOV R4, R1         ; KEEP Should NOT propagate (R1 changed)
    MOV SP, BP
    POP BP
    RET

; --- Multiple propagations ---
__function_test_copy_prop_multiple:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R3, R1         ; MATCH Should become: MOV R3, R2
    MOV R4, R3         ; MATCH Should become: MOV R4, R2 (via R3→R2)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ RULE 2: Copy Propagation (Immediate)
; ===================================================================

; --- Basic immediate propagation ---
__function_test_copy_prop_imm:
    PUSH BP
    MOV BP, SP
    MOV R1, 42
    MOV R2, R1         ; MATCH Should become: MOV R2, 42
    MOV SP, BP
    POP BP
    RET

; --- Immediate to ALU ---
__function_test_copy_prop_alu:
    PUSH BP
    MOV BP, SP
    MOV R1, 10
    IADD R2, R1        ; MATCH Should become: IADD R2, 10
    ISUB R3, R1        ; MATCH Should become: ISUB R3, 10
    IMUL R4, R1        ; MATCH Should become: IMUL R4, 10
    MOV SP, BP
    POP BP
    RET

; --- Immediate to label (SHOULD propagate) ---
__function_test_copy_prop_label:
    PUSH BP
    MOV BP, SP
    MOV R1, _my_label
    JMP R1             ; MATCH Should become: JMP _my_label
    MOV SP, BP
    POP BP
    RET
_my_label:
    HLT

; ===================================================================
; ❌ GUARDS: Should NOT Propagate
; ===================================================================

; --- Into POW (SHOULD NOT propagate) ---
__function_test_guard_pow:
    PUSH BP
    MOV BP, SP
    MOV R1, 5
    POW R2, R1         ; KEEP Should NOT propagate immediate into POW
    MOV SP, BP
    POP BP
    RET

; --- Into ATAN2 (SHOULD NOT propagate) ---
__function_test_guard_atan2:
    PUSH BP
    MOV BP, SP
    MOV R1, 5
    ATAN2 R2, R1       ; KEEP Should NOT propagate immediate into ATAN2
    MOV SP, BP
    POP BP
    RET

; --- Into JT with numeric immediate (SHOULD NOT propagate) ---
__function_test_guard_jt:
    PUSH BP
    MOV BP, SP
    MOV R1, 0
    JT R2, R1           ; KEEP Should NOT propagate 0 into JT target
    MOV SP, BP
    POP BP
    RET

; --- Into JF with numeric immediate (SHOULD NOT propagate) ---
__function_test_guard_jf:
    PUSH BP
    MOV BP, SP
    MOV R1, 0
    JF R2, R1           ; KEEP Should NOT propagate 0 into JF target
    MOV SP, BP
    POP BP
    RET

; --- SP/BP protection ---
__function_test_guard_sp_bp:
    PUSH BP
    MOV BP, SP
    MOV SP, R1
    MOV R2, SP          ; KEEP Should NOT propagate (SP protected)
    MOV BP, R3
    MOV R4, BP          ; KEEP Should NOT propagate (BP protected)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ COMPLEX: Combined Scenarios
; ===================================================================

; --- Store-to-load + copy propagation ---
__function_test_combined:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2
    MOV R3, [R1]        ; MATCH Rule 1: → MOV R3, R2
    MOV R4, R3          ; MATCH Rule 2: → MOV R4, R2 (propagates R3→R2)
    MOV SP, BP
    POP BP
    RET
