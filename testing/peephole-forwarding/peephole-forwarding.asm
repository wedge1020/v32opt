; ===================================================================
; TEST: peephole-forwarding - Extended Stress & Regression Suite
; Run with: ./v32opt peephole-forwarding.asm -fpeephole-forwarding -v
; ===================================================================

; ===================================================================
; ✅ SECTION 1: Store-to-Load Forwarding (Rule 1)
; ===================================================================

; --- Basic store-to-load ---
__function_test_store_load:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2
    MOV R3, [R1]        ; MATCH(1) Should become: MOV R3, R2
    MOV SP, BP
    POP BP
    RET

; --- Positive offset store-to-load ---
__function_test_store_load_pos_offset:
    PUSH BP
    MOV BP, SP
    MOV [BP+4], R2
    MOV R3, [BP+4]     ; MATCH(2) Should become: MOV R3, R2
    MOV SP, BP
    POP BP
    RET

; --- Negative offset store-to-load ---
__function_test_store_load_neg_offset:
    PUSH BP
    MOV BP, SP
    MOV [BP-8], R2
    MOV R3, [BP-8]     ; MATCH(3) Should become: MOV R3, R2
    MOV SP, BP
    POP BP
    RET

; --- Offset mismatch (SHOULD NOT forward) ---
__function_test_store_load_offset_mismatch:
    PUSH BP
    MOV BP, SP
    MOV [BP+4], R2
    MOV R3, [BP+8]     ; KEEP(1) Offsets differ (+4 vs +8)
    MOV SP, BP
    POP BP
    RET

; --- Base register mismatch (SHOULD NOT forward) ---
__function_test_store_load_base_mismatch:
    PUSH BP
    MOV BP, SP
    MOV [R1], R3
    MOV R4, [R2]       ; KEEP(2) Base registers differ (R1 vs R2)
    MOV SP, BP
    POP BP
    RET

; --- Memory overwritten (SHOULD NOT forward) ---
__function_test_store_load_overwrite:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2
    MOV R3, 0
    MOV [R1], R3       ; KEEP(3) Overwrites memory
    IADD R3, 1         ; Clobbers R3 so Rule 1 cannot forward R3 to R4
    MOV R4, [R1]       ; KEEP(4) Should NOT forward (memory changed)
    MOV SP, BP
    POP BP
    RET

; --- Base register modified before load (SHOULD NOT forward) ---
__function_test_store_load_base_modified:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2
    IADD R1, 4         ; Base reg R1 changed
    MOV R3, [R1]       ; KEEP(5) Should NOT forward (R1 changed)
    MOV SP, BP
    POP BP
    RET

; --- Source register modified before load (SHOULD NOT forward) ---
__function_test_store_load_src_modified:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2
    IADD R2, 1         ; Source reg R2 changed
    MOV R3, [R1]       ; KEEP(6) Should NOT forward (R2 changed)
    MOV SP, BP
    POP BP
    RET

; --- Across control flow (SHOULD NOT forward) ---
__function_test_store_load_cfb:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2
    JMP _skip_stl
    MOV R3, [R1]       ; KEEP(7) Should NOT forward (across JMP)
_skip_stl:
    MOV SP, BP
    POP BP
    RET


; ===================================================================
; ✅ SECTION 2: Register Copy Propagation (Rule 2)
; ===================================================================

; --- Basic register propagation ---
__function_test_copy_prop_reg:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R3, R1          ; MATCH(4) Should become: MOV R3, R2
    MOV SP, BP
    POP BP
    RET

; --- Through comments and whitespace ---
__function_test_copy_prop_comments:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    ; intermediate comment line
    MOV R3, R1          ; MATCH(5) Should become: MOV R3, R2
    MOV SP, BP
    POP BP
    RET

; --- Defined register modified (SHOULD NOT propagate) ---
__function_test_copy_prop_def_modified:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    IADD R1, 5         ; Overwrites R1
    MOV R4, R1         ; KEEP(8) Should NOT propagate (R1 changed)
    MOV SP, BP
    POP BP
    RET

; --- Source register modified (SHOULD NOT propagate) ---
__function_test_copy_prop_src_modified:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    IADD R2, 5         ; Overwrites R2
    MOV R4, R1         ; KEEP(9) Should NOT propagate (R2 changed)
    MOV SP, BP
    POP BP
    RET

; --- Multiple sequential reads ---
__function_test_copy_prop_multi_read:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R3, R1         ; MATCH(6) Should become: MOV R3, R2
    IADD R4, R1        ; MATCH(7) Should become: IADD R4, R2
    ISUB R5, R1        ; MATCH(8) Should become: ISUB R5, R2
    MOV SP, BP
    POP BP
    RET

; --- Chained multi-hop propagation ---
__function_test_copy_prop_chained:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R3, R1         ; MATCH(9) Should become: MOV R3, R2
    MOV R4, R3         ; MATCH(10) Should become: MOV R4, R2 (propagates R3->R2)
    MOV SP, BP
    POP BP
    RET

; --- Blocked by intervening Label (SHOULD NOT propagate) ---
__function_test_copy_prop_label_boundary:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
_block_label:
    MOV R3, R1         ; KEEP(10) Should NOT propagate across label boundary
    MOV SP, BP
    POP BP
    RET


; ===================================================================
; ✅ SECTION 3: Immediate & Label Copy Propagation (Rule 2)
; ===================================================================

; --- Basic positive immediate propagation ---
__function_test_copy_prop_imm_pos:
    PUSH BP
    MOV BP, SP
    MOV R1, 42
    MOV R2, R1         ; MATCH(11) Should become: MOV R2, 42
    MOV SP, BP
    POP BP
    RET

; --- Negative immediate propagation ---
__function_test_copy_prop_imm_neg:
    PUSH BP
    MOV BP, SP
    MOV R1, -128
    MOV R2, R1         ; MATCH(12) Should become: MOV R2, -128
    MOV SP, BP
    POP BP
    RET

; --- Hexadecimal immediate propagation ---
__function_test_copy_prop_imm_hex:
    PUSH BP
    MOV BP, SP
    MOV R1, 0xFF00
    MOV R2, R1         ; MATCH(13) Should become: MOV R2, 0xFF00
    MOV SP, BP
    POP BP
    RET

; --- Immediate to ALU operations ---
__function_test_copy_prop_alu:
    PUSH BP
    MOV BP, SP
    MOV R1, 10
    IADD R2, R1        ; MATCH(14) Should become: IADD R2, 10
    ISUB R3, R1        ; MATCH(15) Should become: ISUB R3, 10
    IMUL R4, R1        ; MATCH(16) Should become: IMUL R4, 10
    AND R5, R1         ; MATCH(17) Should become: IAND R5, 10
    MOV SP, BP
    POP BP
    RET

; --- Label address propagation into JMP ---
__function_test_copy_prop_label_jmp:
    PUSH BP
    MOV BP, SP
    MOV R1, _target_label
    JMP R1             ; KEEP(11) Should not touch
    MOV SP, BP
    POP BP
    RET
_target_label:
    HLT


; ===================================================================
; ❌ SECTION 4: Guard Conditions (must keep)
; ===================================================================

; --- Into Memory Store (SHOULD NOT propagate immediate) ---
; Vircon32 CANNOT do MOV [R2], 100 directly; it requires a register.
__function_test_guard_imm_store:
    PUSH BP
    MOV BP, SP
    MOV R1, 100
    MOV [R2], R1       ; KEEP(12) Should NOT propagate 100 into indirect destination
    MOV SP, BP
    POP BP
    RET

; --- Into POW instruction (SHOULD NOT propagate) ---
__function_test_guard_pow:
    PUSH BP
    MOV BP, SP
    MOV R1, 5
    POW R2, R1         ; KEEP(13) Should NOT propagate immediate into POW
    MOV SP, BP
    POP BP
    RET

; --- Into ATAN2 instruction (SHOULD NOT propagate) ---
__function_test_guard_atan2:
    PUSH BP
    MOV BP, SP
    MOV R1, 5
    ATAN2 R2, R1       ; KEEP(14) Should NOT propagate immediate into ATAN2
    MOV SP, BP
    POP BP
    RET

; --- Into JT with numeric immediate target (SHOULD NOT propagate) ---
__function_test_guard_jt_num:
    PUSH BP
    MOV BP, SP
    MOV R1, 0
    JT R2, R1           ; KEEP(15) Should NOT propagate numeric 0 into JT
    MOV SP, BP
    POP BP
    RET

; --- Into JF with numeric immediate target (SHOULD NOT propagate) ---
__function_test_guard_jf_num:
    PUSH BP
    MOV BP, SP
    MOV R1, 0
    JF R2, R1           ; KEEP(16) Should NOT propagate numeric 0 into JF
    MOV SP, BP
    POP BP
    RET

; --- Stack Pointer (SP) Protection ---
__function_test_guard_sp:
    PUSH BP
    MOV BP, SP
    MOV SP, R1
    MOV R2, SP          ; KEEP(17) Should NOT propagate (SP protected)
    MOV SP, BP
    POP BP
    RET

; --- Base Pointer (BP) Protection ---
__function_test_guard_bp:
    PUSH BP
    MOV BP, SP
    MOV BP, R3
    MOV R4, BP          ; KEEP(18) Should NOT propagate (BP protected)
    MOV SP, BP
    POP BP
    RET


; ===================================================================
; ✅ SECTION 5: Complex & Combined Scenarios
; ===================================================================

; --- Store-to-load followed immediately by copy propagation ---
__function_test_combined_stl_and_cp:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2
    MOV R3, [R1]        ; MATCH(18) Rule 1: -> MOV R3, R2
    MOV R4, R3          ; MATCH(19) Rule 2: -> MOV R4, R2
    MOV SP, BP
    POP BP
    RET

; --- Multi-instruction block propagation sequence ---
__function_test_combined_sequence:
    PUSH BP
    MOV BP, SP
    MOV R1, 25
    IADD R2, R1        ; MATCH(20) -> IADD R2, 25
    MOV R3, R1         ; MATCH(21) -> MOV R3, 25
    IMUL R4, R3        ; MATCH(22) -> IMUL R4, 25 (via R3->25 propagation)
    MOV SP, BP
    POP BP
    RET

; ❌ GUARD: Immediate into POW (unsupported)
__function_test_guard_pow_imm:
    PUSH BP
    MOV BP, SP
    MOV R1, 2.0
    POW R2, R1            ; KEEP(19) - POW doesn't accept immediates
    MOV SP, BP
    POP BP
    RET

; ❌ GUARD: Immediate into JT (numeric branch target)
__function_test_guard_jt_imm:
    PUSH BP
    MOV BP, SP
    MOV R1, 0x1000
    JT R2, R1             ; KEEP(20) - JT target must be label/reg, not immediate
    MOV SP, BP
    POP BP
    RET

; ✅ COPY PROPAGATION WITH FLOATING-POINT
__function_test_fp_copy_prop:
    PUSH BP
    MOV BP, SP
    MOV R1, 3.14          ; Float immediate
    FADD R2, R1           ; MATCH(23) → FADD R2, 3.14
    MOV SP, BP
    POP BP
    RET
