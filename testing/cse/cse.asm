; ===================================================================
; TEST: cse - Common Subexpression Elimination
; Run with: ./v32opt cse.asm -fcse -v
; Note: Vircon32 uses 2-operand format: IADD R1, R2 means R1 = R1 + R2
; CSE Pattern: MOV Rx, A; OP Rx, B; ... MOV Ry, A; OP Ry, B -> MOV Ry, Rx
; ===================================================================

; ===================================================================
; ✅ SCENARIO 1: Basic CSE Pattern
; MOV R1, R5; IADD R1, R2; MOV R3, R5; IADD R3, R2 -> second IADD becomes MOV R3, R1
; ===================================================================
MOV R1, R5 ; KEEP
IADD R1, R2 ; KEEP
MOV R3, R5 ; KEEP
IADD R3, R2 ; MATCH

; ===================================================================
; ✅ SCENARIO 2: CSE with Immediate (Word Saving)
; MOV R1, R5; IMUL R1, 42; MOV R3, R5; IMUL R3, 42 -> second IMUL becomes MOV R3, R1
; Saves 1 word (IMUL with immediate is 2 words, MOV is 1 word)
; ===================================================================
MOV R1, R5 ; KEEP
IMUL R1, 42 ; KEEP
MOV R3, R5 ; KEEP
IMUL R3, 42 ; MATCH

; ===================================================================
; ✅ SCENARIO 3: Multiple Reuses
; ===================================================================
MOV R1, R5 ; KEEP
ISUB R1, R2 ; KEEP
MOV R3, R5 ; KEEP
ISUB R3, R2 ; MATCH
MOV R4, R5 ; KEEP
ISUB R4, R2 ; MATCH

; ===================================================================
; ❌ SCENARIO 4: CSE Blocked by Register Modification
; R1 is modified between the two expressions
; ===================================================================
MOV R1, R5 ; KEEP
IADD R1, R2 ; KEEP
MOV R1, 100 ; KEEP (modifies R1)
MOV R3, R5 ; KEEP
IADD R3, R2 ; KEEP

; ===================================================================
; ❌ SCENARIO 5: CSE Across Control Flow Boundary
; Control flow boundary prevents CSE
; ===================================================================
MOV R1, R5 ; KEEP
IADD R1, R2 ; KEEP
JMP _skip_cse ; KEEP
_skip_cse:
MOV R3, R5 ; KEEP
IADD R3, R2 ; KEEP

; ===================================================================
; ✅ SCENARIO 6: CSE with Different Operations
; Different operations should not match
; ===================================================================
MOV R1, R5 ; KEEP
IADD R1, R2 ; KEEP
MOV R3, R5 ; KEEP
ISUB R3, R2 ; KEEP

; ===================================================================
; ✅ SCENARIO 7: CSE with Indented Comments
; Comments should not block CSE
; ===================================================================
MOV R1, R5 ; KEEP
IADD R1, R2 ; KEEP
    ; Comment between expressions
MOV R3, R5 ; KEEP
    ; Another comment
IADD R3, R2 ; MATCH

; ===================================================================
; ✅ SCENARIO 8: CSE with OR
; ===================================================================
MOV R1, R5 ; KEEP
OR R1, R2 ; KEEP
MOV R3, R5 ; KEEP
OR R3, R2 ; MATCH

; ===================================================================
; ✅ SCENARIO 9: CSE with AND
; ===================================================================
MOV R1, R5 ; KEEP
AND R1, R2 ; KEEP
MOV R3, R5 ; KEEP
AND R3, R2 ; MATCH

; ===================================================================
; ✅ SCENARIO 10: CSE with XOR
; ===================================================================
MOV R1, R5 ; KEEP
XOR R1, R2 ; KEEP
MOV R3, R5 ; KEEP
XOR R3, R2 ; MATCH

; ===================================================================
; ✅ SCENARIO 11: CSE with Floating Point
; ===================================================================
MOV R1, R5 ; KEEP
FADD R1, R2 ; KEEP
MOV R3, R5 ; KEEP
FADD R3, R2 ; MATCH

; ===================================================================
; ✅ SCENARIO 12: CSE with Negative Immediate
; ===================================================================
MOV R1, R5 ; KEEP
IADD R1, -10 ; KEEP
MOV R3, R5 ; KEEP
IADD R3, -10 ; MATCH

; ===================================================================
; ❌ SCENARIO 13: Different Immediate Values
; ===================================================================
MOV R1, R5 ; KEEP
IADD R1, 42 ; KEEP
MOV R3, R5 ; KEEP
IADD R3, 100 ; KEEP

; ===================================================================
; ❌ SCENARIO 14: Different Source Registers
; ===================================================================
MOV R1, R5 ; KEEP
IADD R1, R2 ; KEEP
MOV R3, R6 ; KEEP
IADD R3, R2 ; KEEP

; ===================================================================
; ✅ SCENARIO 15: Chained CSE
; Multiple CSE opportunities in sequence
; ===================================================================
MOV R1, R10 ; KEEP
IADD R1, R2 ; KEEP
MOV R3, R10 ; KEEP
IADD R3, R2 ; MATCH
MOV R4, R10 ; KEEP
IADD R4, R2 ; MATCH
