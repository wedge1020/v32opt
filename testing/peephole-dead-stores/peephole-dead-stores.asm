; ===================================================================
; TEST: peephole-dead-stores - All Scenarios
; Run with: ./v32opt peephole-dead-stores.asm -fpeephole-dead-stores -v
; ===================================================================

; ===================================================================
; SCENARIO 1: Consecutive register stores (SHOULD REMOVE first)
; MOV R1, 10; MOV R1, 20 -> remove first MOV
; ===================================================================
__function_test_consecutive_reg:
    PUSH BP
    MOV BP, SP
    MOV R1, 10      ; MATCH(1) Remove (overwritten by next MOV)
    MOV R1, 20      ; KEEP(1)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; SCENARIO 2: Consecutive register stores with immediate (SHOULD REMOVE first)
; On Vircon32, MOV with immediate uses extra word, so removing saves space
; ===================================================================
__function_test_consecutive_reg_imm:
    PUSH BP
    MOV BP, SP
    MOV R2, 100     ; MATCH(2) Remove (overwritten, saves word)
    MOV R2, 200     ; KEEP(2)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; SCENARIO 3: Register stores with read in between (MUST NOT REMOVE)
; MOV R1, 10; IADD R2, R1; MOV R1, 20 -> keep first (R1 is read)
; ===================================================================
__function_test_reg_read_between:
    PUSH BP
    MOV BP, SP
    MOV R1, 10      ; KEEP(3) (R1 is read by next instruction)
    IADD R2, R1
    MOV R1, 20      ; KEEP(4)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; SCENARIO 4: Consecutive memory stores (SHOULD REMOVE first)
; MOV [R0], R1; MOV [R0], R2 -> remove first MOV
; ===================================================================
__function_test_consecutive_mem:
    PUSH BP
    MOV BP, SP
    MOV [R0], R1    ; MATCH(5) Remove (overwritten by next MOV)
    MOV [R0], R2    ; KEEP(5)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; SCENARIO 5: Memory stores with load in between (MUST NOT REMOVE)
; MOV [R0], R1; MOV R3, [R0]; MOV [R0], R2 -> keep first (memory is read)
; ===================================================================
__function_test_mem_load_between:
    PUSH BP
    MOV BP, SP
    MOV [R0], R1    ; KEEP(6) (memory is loaded before next store)
    MOV R3, [R0]
    MOV [R0], R2    ; KEEP(7)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; SCENARIO 6: Memory stores with offset (SHOULD REMOVE first)
; MOV [BP+4], R1; MOV [BP+4], R2 -> remove first MOV
; ===================================================================
__function_test_mem_offset:
    PUSH BP
    MOV BP, SP
    MOV [BP+4], R1  ; MATCH(8) Remove (overwritten at same offset)
    MOV [BP+4], R2  ; KEEP(8)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; SCENARIO 7: Memory stores with different offsets (MUST NOT REMOVE)
; MOV [BP+4], R1; MOV [BP+8], R2 -> keep both (different locations)
; ===================================================================
__function_test_mem_diff_offset:
    PUSH BP
    MOV BP, SP
    MOV [BP+4], R1  ; KEEP(9) (different offset)
    MOV [BP+8], R2  ; KEEP(10)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; SCENARIO 8: Memory stores with base register modification (MUST NOT REMOVE)
; MOV [R0], R1; IADD R0, 4; MOV [R0], R2 -> keep first (base changed)
; ===================================================================
__function_test_mem_base_modified:
    PUSH BP
    MOV BP, SP
    MOV [R0], R1    ; KEEP(11) (R0 is modified before next store)
    IADD R0, 4
    MOV [R0], R2    ; KEEP(12)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; SCENARIO 9: Multiple consecutive dead stores (SHOULD REMOVE all but last)
; MOV R1, 1; MOV R1, 2; MOV R1, 3 -> remove first two
; ===================================================================
__function_test_multiple_consecutive:
    PUSH BP
    MOV BP, SP
    MOV R1, 1       ; MATCH(13) Remove (overwritten)
    MOV R1, 2       ; MATCH(14) Remove (overwritten)
    MOV R1, 3       ; KEEP(13)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; SCENARIO 10: Dead store with non-reading instructions between
; MOV R1, 10; IADD R2, R3; MOV R1, 20 -> remove first (R1 not read)
; ===================================================================
__function_test_dead_with_nonread:
    PUSH BP
    MOV BP, SP
    MOV R1, 10      ; MATCH(15) Remove (not read before overwrite)
    IADD R2, R3
    MOV R1, 20      ; KEEP(14)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; SCENARIO 11: Store to SP/BP (MUST NOT REMOVE)
; Special registers should not be optimized
; ===================================================================
__function_test_special_regs:
    PUSH BP
    MOV BP, SP
    MOV SP, 100     ; KEEP(15) (SP is special)
    MOV SP, 200     ; KEEP(16)
    MOV BP, 100     ; KEEP(17) (BP is special)
    MOV BP, 200     ; KEEP(18)
    POP BP
    RET

; ===================================================================
; SCENARIO 12: Mixed register and memory dead stores
; ===================================================================
__function_test_mixed:
    PUSH BP
    MOV BP, SP
    MOV R1, 10      ; MATCH(19) Remove (overwritten)
    MOV R1, 20      ; KEEP(19)
    MOV [R2], R3    ; MATCH(20) Remove (overwritten)
    MOV [R2], R4    ; KEEP(20)
    MOV R5, 100     ; MATCH(21) Remove (not read before overwrite)
    IADD R6, R7
    MOV R5, 200     ; KEEP(21)
    MOV [R8+4], R9  ; MATCH(22) Remove (overwritten at same offset)
    MOV [R8+4], R10 ; KEEP(22)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; SCENARIO 13: Store with control flow boundary (MUST NOT REMOVE)
; MOV R1, 10; JMP label; MOV R1, 20 -> keep first (boundary prevents optimization)
; ===================================================================
__function_test_control_flow:
    PUSH BP
    MOV BP, SP
    MOV R1, 10      ; KEEP(23) (JMP is control flow boundary)
    JMP __label_test
    MOV R1, 20      ; KEEP(24)
__label_test:
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; SCENARIO 14: Dead store with immediate 0 (SHOULD REMOVE)
; MOV R1, 0; MOV R1, 5 -> remove first
; ===================================================================
__function_test_imm_zero:
    PUSH BP
    MOV BP, SP
    MOV R1, 0       ; MATCH(25) Remove (overwritten)
    MOV R1, 5       ; KEEP(25)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; SCENARIO 15: Store followed by different register store (MUST NOT REMOVE)
; MOV R1, 10; MOV R2, 20 -> keep both (different registers)
; ===================================================================
__function_test_diff_regs:
    PUSH BP
    MOV BP, SP
    MOV R1, 10      ; KEEP(26) (different register)
    MOV R2, 20      ; KEEP(27)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; SCENARIO 16: Memory dead store with multiple instructions between
; MOV [R0], R1; IADD R2, R3; ISUB R4, R5; MOV [R0], R6 -> remove first
; ===================================================================
__function_test_mem_multi_between:
    PUSH BP
    MOV BP, SP
    MOV [R0], R1    ; MATCH(28) Remove (not read before overwrite)
    IADD R2, R3
    ISUB R4, R5
    MOV [R0], R6    ; KEEP(28)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; SCENARIO 17: Memory store followed by different offset store (MUST NOT REMOVE)
; MOV [R0+4], R1; MOV [R0+8], R2 -> keep both
; ===================================================================
__function_test_mem_diff_offset_keep:
    PUSH BP
    MOV BP, SP
    MOV [R0+4], R1  ; KEEP(29) (different offset)
    MOV [R0+8], R2  ; KEEP(30)
    MOV SP, BP
    POP BP
    RET
