; ===================================================================
; TEST: peephole-shifts - Shift Optimizations
; Run with: ./v32opt peephole-shifts.asm -fpeephole-shifts -v
; ===================================================================

; ===================================================================
; ✅ SECTION 1: SHL by 0 → Remove
; ===================================================================

__function_test_shl_by_zero:
    PUSH BP
    MOV BP, SP
    SHL R1, 0          ; MATCH(1) Should be removed
    MOV SP, BP
    POP BP
    RET

__function_test_shl_by_zero_multi:
    PUSH BP
    MOV BP, SP
    SHL R1, 0          ; MATCH(2) Should be removed
    SHL R2, 0          ; MATCH(3) Should be removed
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SECTION 2: SHL by 1 → IADD r, r
; ===================================================================

__function_test_shl_by_one:
    PUSH BP
    MOV BP, SP
    SHL R1, 1          ; MATCH(4) Should become: IADD R1, R1
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SECTION 3: Opposite Shifts Cancel Out
; ===================================================================

__function_test_shl_cancel:
    PUSH BP
    MOV BP, SP
    SHL R1, 5          ; MATCH(5) Should be removed
    SHL R1, -5         ; MATCH(6) Should be removed
    MOV SP, BP
    POP BP
    RET

__function_test_shl_cancel_sequence:
    PUSH BP
    MOV BP, SP
    SHL R1, 2          ; MATCH(7) Should be removed
    SHL R1, -2         ; MATCH(8) Should be removed
    SHL R1, 1          ; MATCH(9) Should become: IADD R1, R1
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SECTION 4: SHL with Same Source and Destination
; ===================================================================

__function_test_shl_same_reg:
    PUSH BP
    MOV BP, SP
    SHL R1, R1         ; MATCH(10) Should be removed
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SECTION 5: Guard Conditions (must KEEP)
; ===================================================================

__function_test_shl_no_cancel:
    PUSH BP
    MOV BP, SP
    SHL R1, 4          ; KEEP(1) Different amounts
    SHL R1, -5         ; KEEP(2) Different amounts
    MOV SP, BP
    POP BP
    RET

__function_test_shl_diff_regs:
    PUSH BP
    MOV BP, SP
    SHL R1, 3          ; KEEP(3) Different registers
    SHL R2, -3         ; KEEP(4) Different registers
    MOV SP, BP
    POP BP
    RET

__function_test_shl_same_dir:
    PUSH BP
    MOV BP, SP
    SHL R1, 2          ; KEEP(5) Same direction
    SHL R1, 2          ; KEEP(6) Same direction
    MOV SP, BP
    POP BP
    RET

__function_test_shl_float:
    PUSH BP
    MOV BP, SP
	MOV R2, 1.0
    SHL R1, R2         ; KEEP(7) Float immediate
    MOV SP, BP
    POP BP
    RET

__function_test_shl_reg_operand:
    PUSH BP
    MOV BP, SP
    SHL R1, R2         ; KEEP(8) Register operand
    MOV SP, BP
    POP BP
    RET

__function_test_shl_large:
    PUSH BP
    MOV BP, SP
    SHL R1, 8          ; KEEP(9) Large shift amount
    SHL R1, 16         ; KEEP(10) Large shift amount
    MOV SP, BP
    POP BP
    RET

__function_test_shl_negative:
    PUSH BP
    MOV BP, SP
    SHL R1, -1         ; KEEP(11) Negative shift (shift right)
    SHL R1, -8         ; KEEP(12) Negative shift
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SECTION 6: Combined Shift Scenarios
; ===================================================================

__function_test_shifts_combined:
    PUSH BP
    MOV BP, SP
    SHL R1, 0          ; MATCH(11) Should be removed
    SHL R2, 1          ; MATCH(12) Should become: IADD R2, R2
    SHL R3, R3        ; MATCH(13) Should be removed
    SHL R4, 3          ; KEEP(13) No optimization
    SHL R4, -3         ; MATCH(14) Should be removed
    MOV SP, BP
    POP BP
    RET

__function_test_shifts_mixed:
    PUSH BP
    MOV BP, SP
    SHL R1, 0          ; MATCH(15) Should be removed
    IADD R2, 5         ; KEEP(14) Not a shift
    SHL R3, 1          ; MATCH(16) Should become: IADD R3, R3
    SHL R4, R4        ; MATCH(17) Should be removed
    MOV SP, BP
    POP BP
    RET
