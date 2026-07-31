; ===================================================================
; TEST: peephole-compiler-myopia
; Run with: ./v32opt peephole-compiler-myopia.asm -fpeephole-compiler-myopia -v
; ===================================================================

; ===================================================================
; ✅ SCENARIO 1: Basic store-then-load (SHOULD REMOVE)
; ===================================================================
__function_test_basic:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2   ; Store to memory
    MOV R2, [R1]   ; MATCH(1) Redundant load - remove
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 2: With intervening instruction (SHOULD REMOVE)
; ===================================================================
__function_test_intervening:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2   ; Store
    IADD R3, R4    ; Safe intervening instruction
    MOV R2, [R1]   ; MATCH(2) Redundant load - remove
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 3: Multiple safe intervening instructions (SHOULD REMOVE)
; ===================================================================
__function_test_multiple_intervening:
    PUSH BP
    MOV BP, SP
    MOV [R1+4], R5 ; Store with offset
    IADD R6, R7    ; Safe
    ISUB R8, R9    ; Safe
    MOV R5, [R1+4] ; MATCH(3) Redundant load - remove
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 4: Intervening instruction modifies R_src (MUST NOT REMOVE)
; ===================================================================
__function_test_modifies_src:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2   ; Store
    IADD R2, R3    ; Modifies R2 - invalidates optimization
    MOV R2, [R1]   ; KEEP(1) Cannot remove
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 5: Intervening write to memory location (MUST NOT REMOVE)
; ===================================================================
__function_test_modifies_mem:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2   ; Store
    MOV [R1], R3   ; Overwrites [R1] - invalidates optimization
    MOV R2, [R1]   ; KEEP(2) Cannot remove
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 6: Control flow boundary (MUST NOT REMOVE)
; ===================================================================
__function_test_control_boundary:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2   ; Store
    JMP __skip     ; Control flow boundary
    MOV R2, [R1]   ; KEEP(3) After boundary
__skip:
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 7: With comments/blanks (SHOULD REMOVE)
; ===================================================================
__function_test_with_blanks:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2   ; Store

    ; Comment line
    IADD R3, R4    ; Safe intervening

    MOV R2, [R1]   ; MATCH(4) Redundant load - remove
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 8: Different memory offset (MUST NOT REMOVE)
; ===================================================================
__function_test_diff_offset:
    PUSH BP
    MOV BP, SP
    MOV [R1+4], R2 ; Store to [R1+4]
    IADD R3, R4    ; Safe but...
    MOV R2, [R1+8] ; KEEP(4) Different offset - cannot remove
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 9: Mixed scenarios (SHOULD OPTIMIZE SELECTIVELY)
; ===================================================================
__function_test_mixed:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2      ; Remove (safe intervening follows)
    IADD R3, R4       ; Safe
    MOV R2, [R1]      ; MATCH(5) Remove

    MOV [R5], R6      ; Store
    IADD R6, R7       ; Modifies R6 - invalidates
    MOV R6, [R5]      ; KEEP(5) Cannot remove

    MOV [R8+4], R9    ; Remove (safe intervening)
    ISUB R10, R11     ; Safe
    MOV R9, [R8+4]    ; MATCH(6) Remove

    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 10: Testing intervening instructions (SHOULD REMOVE)
; ===================================================================
__function_test_intervening_gap:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2   ; Store

    ; Comment line
    IADD R3, R4    ; Safe intervening
    ISUB R3, 2     ; Safe intervening
	MOV  R5, R3    ; Safe intervening
	IMUL R5, 3     ; Safe intervening

    ; Comment line 2
    IADD R3, R4    ; Safe intervening
    ISUB R3, 2     ; Safe intervening
	MOV  R5, R3    ; Safe intervening
	IMUL R5, 3     ; Safe intervening

    ; Comment line 3
    IADD R3, R4    ; Safe intervening
    ISUB R3, 2     ; Safe intervening
	MOV  R5, R3    ; Safe intervening
	IMUL R5, 3     ; Safe intervening

    MOV R2, [R1]   ; MATCH(7) Redundant load - remove
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 12: Exceeding scan distance (MUST NOT REMOVE)
; ===================================================================
__function_test_intervening_too_large_gap:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2   ; Store

    ; Comment line
    IADD R3, R4    ; Safe intervening
    ISUB R3, 2     ; Safe intervening
	MOV  R5, R3    ; Safe intervening
	IMUL R5, 3     ; Safe intervening

    ; Comment line
    IADD R3, R4    ; Safe intervening
    ISUB R3, 2     ; Safe intervening
	MOV  R5, R3    ; Safe intervening
	IMUL R5, 3     ; Safe intervening

    ; Comment line
    IADD R3, R4    ; Safe intervening
    ISUB R3, 2     ; Safe intervening
	MOV  R5, R3    ; Safe intervening
	IMUL R5, 3     ; Safe intervening

    ; Comment line
    IADD R3, R4    ; Safe intervening
    ISUB R3, 2     ; Safe intervening
	MOV  R5, R3    ; Safe intervening
	IMUL R5, 3     ; Safe intervening

    ; Comment line
    IADD R3, R4    ; Safe intervening
    ISUB R3, 2     ; Safe intervening
	MOV  R5, R3    ; Safe intervening
	IMUL R5, 3     ; Safe intervening

    ; Comment line 2
    IADD R3, R4    ; Safe intervening
    ISUB R3, 2     ; Safe intervening
	MOV  R5, R3    ; Safe intervening
	IMUL R5, 3     ; Safe intervening

    ; Comment line 3
    IADD R3, R4    ; Safe intervening
    ISUB R3, 2     ; Safe intervening
	MOV  R5, R3    ; Safe intervening
	IMUL R5, 3     ; Safe intervening

    MOV R2, [R1]   ; MATCH(8) Redundant load - remove
    MOV SP, BP
    POP BP
    RET
