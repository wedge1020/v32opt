; ===================================================================
; TEST: peephole_jumps - All Scenarios
; Run with: ./v32opt test_jumps.asm -fopt_peephole_jumps -v
; ===================================================================

; ===================================================================
; ✅ SCENARIO 1: Basic Jump to Next Label (SHOULD REMOVE JMP)
; ===================================================================
JMP _L1 ; MATCH
_L1:
MOV R1, 42
RET

; ===================================================================
; ✅ SCENARIO 2: Jump with Comments (SHOULD REMOVE JMP)
; ===================================================================
JMP _L2 ; MATCH
; This is a comment
; Another comment
_L2:
MOV R1, 42
RET

; ===================================================================
; ✅ SCENARIO 3: Jump with Blank Lines (SHOULD REMOVE JMP)
; ===================================================================
JMP _L3 ; MATCH

_L3:
MOV R1, 42
RET

; ===================================================================
; ❌ SCENARIO 4: Jump to Different Label (MUST NOT REMOVE)
; ===================================================================
JMP _L5 ; KEEP
_L4:
MOV R1, 42
_L5:
MOV R2, 100
RET

; ===================================================================
; ❌ SCENARIO 5: Label Not Immediate Next (MUST NOT REMOVE)
; ===================================================================
JMP _L6 ; KEEP
MOV R1, 42
_L6:
MOV R2, 100
RET

; ===================================================================
; ✅ SCENARIO 6: Multiple Redundant Jumps (SHOULD REMOVE BOTH)
; ===================================================================
JMP _L7 ; MATCH
_L7:
JMP _L8 ; MATCH
_L8:
MOV R1, 42
RET

; ===================================================================
; ✅ SCENARIO 7: Multi-case Match (SHOULD REMOVE JMP)
; ===================================================================
JMP _My_Label ; MATCH
_My_Label:
MOV R1, 42
RET

; ===================================================================
; ❌ SCENARIO 8: Jump to Register (MUST NOT REMOVE)
; ===================================================================
JMP R1 ; KEEP
MOV R2, 42
RET

; ===================================================================
; ❌ SCENARIO 9: Jump to Immediate Address (MUST NOT REMOVE)
; ===================================================================
JMP 0x1000 ; KEEP
MOV R1, 42
RET

; ===================================================================
; ✅ SCENARIO 10: Function Entry Pattern
; ===================================================================
__function_test_entry:
    JMP __function_test_entry_start ; MATCH
__function_test_entry_start:
    MOV R1, 42
    RET

; ===================================================================
; ✅ SCENARIO 11: Return Label Pattern
; ===================================================================
JMP __function_test_return ; MATCH
__function_test_return:
MOV R1, 42
RET

; ===================================================================
; ✅ SCENARIO 12: Mixed Valid and Invalid
; ===================================================================
JMP _L10 ; MATCH
_L10:
MOV R1, 1
JMP _L11 ; KEEP
_L12:
MOV R2, 2
JMP _L12 ; KEEP
_L11:
MOV R3, 3
RET
