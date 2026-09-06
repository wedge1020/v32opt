; ===================================================================
; TEST: peephole-dead-stores
; Run with: ./v32opt peephole-dead-stores.asm -fpeephole-dead-stores -v
; ===================================================================

; ===================================================================
; ✅ SCENARIO 1: Basic Register Dead Store (SHOULD REMOVE)
; ===================================================================
_function_test_basic_reg:
    PUSH BP
    MOV BP, SP
    MOV R1, 5      ; MATCH(1) Overwritten immediately
    MOV R1, 10
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 2: Intervening Safe Instructions (Reg) (SHOULD REMOVE)
; ===================================================================
_function_test_intervening_reg:
    PUSH BP
    MOV BP, SP
    MOV R2, R3     ; MATCH(2) Dead store
    IADD R4, R5
    MOV R2, R6     ; Overwrites R2 safely
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 3: Register Read Before Overwrite (MUST NOT REMOVE)
; ===================================================================
_function_test_read_reg:
    PUSH BP
    MOV BP, SP
    MOV R1, 5      ; KEEP(1) Read by next instruction
    MOV R2, R1
    MOV R1, 10
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 4: Basic Memory Dead Store (SHOULD REMOVE)
; ===================================================================
_function_test_basic_mem:
    PUSH BP
    MOV BP, SP
    MOV [BP-4], R1 ; MATCH(3) Overwritten immediately
    MOV [BP-4], R2
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 5: Safe Intervening Memory Access (SHOULD REMOVE)
; ===================================================================
_function_test_safe_mem_alias:
    PUSH BP
    MOV BP, SP
    MOV [BP-4], R1 ; MATCH(4) Overwritten later
    MOV [BP-8], R2 ; Safe: different offset, same base (no alias)
    MOV R3, [BP-12]; Safe: different offset, same base (no alias)
    MOV [BP-4], R4 ; Overwrite confirmation
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 6: Exact Memory Read Before Overwrite (MUST NOT REMOVE)
; ===================================================================
_function_test_read_mem:
    PUSH BP
    MOV BP, SP
    MOV [BP-4], R1 ; KEEP(2) Read by next instruction
    MOV R2, [BP-4]
    MOV [BP-4], R3
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 7: Unknown Memory Alias Write (MUST NOT REMOVE)
; ===================================================================
_function_test_alias_write:
    PUSH BP
    MOV BP, SP
    MOV [BP-4], R1 ; KEEP(3) Interrupted by alien write
    MOV [R5], R2   ; Alien write (might resolve to BP-4 dynamically)
    MOV [BP-4], R3
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 8: Unknown Memory Alias Read (MUST NOT REMOVE)
; ===================================================================
_function_test_alias_read:
    PUSH BP
    MOV BP, SP
    MOV [BP-4], R1 ; KEEP(4) Might be read by unknown alias
    MOV R2, [R5]   ; Alien read (might resolve to BP-4 dynamically)
    MOV [BP-4], R3
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 9: Base Register Modified (MUST NOT REMOVE)
; ===================================================================
_function_test_base_modified:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2   ; KEEP(5) Base tracking corrupted
    IADD R1, 4     ; Modifies base
    MOV [R1], R3   ; Points to a new address relative to original!
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 10: Blank Lines and Comments Resilience (SHOULD REMOVE)
; ===================================================================
_function_test_blanks:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2   ; MATCH(5) Dead store

    ; Intervening blanks and comments
    IADD R3, R4

    MOV [R1], R5   ; Overwrite
    MOV SP, BP
    POP BP
    RET
    
; ===================================================================
; ❌ SCENARIO 11: Control Flow Escape (MUST NOT REMOVE)
; ===================================================================
_function_test_boundary:
    PUSH BP
    MOV BP, SP
    MOV R1, 5      ; KEEP(6) Escapes basic block before dead store check
    JMP _skip
    MOV R1, 10
_skip:
    MOV SP, BP
    POP BP
    RET
