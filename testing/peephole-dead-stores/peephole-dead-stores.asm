; ===================================================================
; TEST: peephole-dead-stores - All Scenarios
; Run with: ./v32opt peephole-dead-stores.asm -fpeephole-dead-stores -v
; ===================================================================

; ===================================================================
; ✅ SCENARIO 1: Standard Dead Store (SHOULD REMOVE)
; Overwritten before read
; ===================================================================
__function_test_standard:
    PUSH BP
    MOV BP, SP
    MOV R1, 100         ; Should be removed (dead store)
    MOV R1, 200         ; Overwrites R1
    MOV R2, R1          ; Uses R1
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 2: Multiple Overwrites (SHOULD REMOVE)
; ===================================================================
__function_test_multiple:
    PUSH BP
    MOV BP, SP
    MOV R1, 100         ; Should be removed
    MOV R1, 200         ; Should be removed
    MOV R1, 300         ; Final value
    MOV R2, R1
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 3: Different Registers (MUST NOT REMOVE)
; ===================================================================
__function_test_diff_reg:
    PUSH BP
    MOV BP, SP
    MOV R1, 100         ; Should NOT be removed
    MOV R2, 200         ; Different register
    MOV R3, R1          ; Uses R1
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 4: Register Read Before Overwrite (MUST NOT REMOVE)
; ===================================================================
__function_test_read_before:
    PUSH BP
    MOV BP, SP
    MOV R1, 100         ; Should NOT be removed (read before overwrite)
    MOV R2, R1          ; Reads R1
    MOV R1, 200         ; Overwrites R1
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 5: Register Used in ALU (MUST NOT REMOVE)
; ===================================================================
__function_test_alu_use:
    PUSH BP
    MOV BP, SP
    MOV R1, 100         ; Should NOT be removed (used in ALU)
    IADD R1, 50         ; Reads and writes R1
    MOV R2, R1
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 6: Register Used in Branch (MUST NOT REMOVE)
; ===================================================================
__function_test_branch_use:
    PUSH BP
    MOV BP, SP
    MOV R1, 100         ; Should NOT be removed (used in JT)
    JT R1, target
    MOV R1, 200         ; Never reached if R1 != 0
target:
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 7: Terminal Dead Store (SHOULD REMOVE)
; Not live-out across RET
; ===================================================================
__function_test_terminal:
    PUSH BP
    MOV BP, SP
    MOV R1, 100         ; Should be removed (dead before RET)
    MOV R2, 200
    MOV R0, 0           ; Set return value
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 8: R0 is Live-Out (MUST NOT REMOVE)
; ===================================================================
__function_test_r0_live:
    PUSH BP
    MOV BP, SP
    MOV R0, 100         ; Should NOT be removed (R0 is live-out)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 9: SP is Live-Out (MUST NOT REMOVE)
; ===================================================================
__function_test_sp_live:
    PUSH BP
    MOV BP, SP
    MOV SP, R1          ; Should NOT be removed (SP is live-out)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 10: BP is Live-Out (MUST NOT REMOVE)
; ===================================================================
__function_test_bp_live:
    PUSH BP
    MOV BP, SP
    MOV BP, R1          ; Should NOT be removed (BP is live-out)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 11: Dead Store with Comments (SHOULD REMOVE)
; ===================================================================
__function_test_comments:
    PUSH BP
    MOV BP, SP
    MOV R1, 100         ; Should be removed
    ; comment
    MOV R1, 200         ; Overwrites R1
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 12: Dead Store Across Labels (SHOULD REMOVE)
; ===================================================================
__function_test_labels:
    PUSH BP
    MOV BP, SP
    MOV R1, 100         ; Should be removed (labels don't block scan)
internal_label:
    MOV R1, 200         ; Overwrites R1
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 13: Control Flow Boundary (MUST NOT REMOVE)
; ===================================================================
__function_test_cfb:
    PUSH BP
    MOV BP, SP
    MOV R1, 100         ; Should NOT be removed (JMP is CF boundary)
    JMP skip
    MOV R1, 200
skip:
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 14: Function Call Boundary (MUST NOT REMOVE)
; ===================================================================
__function_test_call:
    PUSH BP
    MOV BP, SP
    MOV R1, 100         ; Should NOT be removed (CALL is CF boundary)
    CALL some_func
    MOV R1, 200
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 15: Multiple Dead Stores
; ===================================================================
__function_test_multiple_dead:
    PUSH BP
    MOV BP, SP
    MOV R1, 100         ; Should be removed
    MOV R2, 200         ; Should be removed
    MOV R1, 300         ; Should be removed
    MOV R2, 400         ; Final values
    MOV R3, R1
    MOV R4, R2
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 16: Dead Store Before RET
; ===================================================================
__function_test_terminal_multiple:
    PUSH BP
    MOV BP, SP
    MOV R1, 100         ; Should be removed
    MOV R2, 200         ; Should be removed
    MOV R3, 300         ; Should be removed
    MOV R0, 0           ; Return value (R0 is live-out)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 17: Indirect Dead Store
; ===================================================================
__function_test_indirect_dead:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2        ; Should be removed (overwritten)
    MOV [R1], R3        ; Overwrites [R1]
    MOV R4, [R1]
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 18: Indirect Live Store
; ===================================================================
__function_test_indirect_live:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2        ; Should NOT be removed (read before overwrite)
    MOV R3, [R1]        ; Reads [R1]
    MOV [R1], R4        ; Overwrites [R1]
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 19: Complex Dead Stores
; ===================================================================
__function_test_complex:
    PUSH BP
    MOV BP, SP
    MOV R1, 100         ; Dead (overwritten)
    MOV R2, 200         ; Dead (overwritten)
    MOV R1, 300         ; Dead (overwritten)
    MOV R2, 400         ; Final values
    MOV R3, R1          ; Uses R1
    MOV R4, R2          ; Uses R2
    MOV R5, 500         ; Dead before RET
    MOV R0, 0           ; Return value
    MOV SP, BP
    POP BP
    RET
