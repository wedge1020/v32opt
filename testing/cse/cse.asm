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
MOV R1, R5 ; KEEP(1)
IADD R1, R2 ; KEEP(2)
MOV R3, R5 ; KEEP(3)
IADD R3, R2 ; MATCH(1)

; ===================================================================
; ✅ SCENARIO 2: CSE with Immediate (Word Saving)
; MOV R1, R5; IMUL R1, 42; MOV R3, R5; IMUL R3, 42 -> second IMUL becomes MOV R3, R1
; Saves 1 word (IMUL with immediate is 2 words, MOV is 1 word)
; ===================================================================
MOV R1, R5 ; KEEP(4)
IMUL R1, 42 ; KEEP(5)
MOV R3, R5 ; KEEP(6)
IMUL R3, 42 ; MATCH(2)

; ===================================================================
; ✅ SCENARIO 3: Multiple Reuses
; ===================================================================
MOV R1, R5 ; KEEP(7)
ISUB R1, R2 ; KEEP(8)
MOV R3, R5 ; KEEP(9)
ISUB R3, R2 ; MATCH(3)
MOV R4, R5 ; KEEP(10)
ISUB R4, R2 ; MATCH(4)

; ===================================================================
; ❌ SCENARIO 4: CSE Blocked by Register Modification
; R1 is modified between the two expressions
; ===================================================================
MOV R1, R5 ; KEEP(11)
IADD R1, R2 ; KEEP(12)
MOV R1, 100 ; KEEP(13) (modifies R1)
MOV R3, R5 ; KEEP(14)
IADD R3, R2 ; KEEP(15)

; ===================================================================
; ❌ SCENARIO 5: CSE Across Control Flow Boundary
; Control flow boundary prevents CSE
; ===================================================================
MOV R1, R5 ; KEEP(16)
IADD R1, R2 ; KEEP(17)
JMP _skip_cse ; KEEP(18)
_skip_cse:
MOV R3, R5 ; KEEP(19)
IADD R3, R2 ; KEEP(20)

; ===================================================================
; ✅ SCENARIO 6: CSE with Different Operations
; Different operations should not match
; ===================================================================
MOV R1, R5 ; KEEP(21)
IADD R1, R2 ; KEEP(22)
MOV R3, R5 ; KEEP(23)
ISUB R3, R2 ; KEEP(24)

; ===================================================================
; ✅ SCENARIO 7: CSE with Indented Comments
; Comments should not block CSE
; ===================================================================
MOV R1, R5 ; KEEP(25)
IADD R1, R2 ; KEEP(26)
    ; Comment between expressions
MOV R3, R5 ; KEEP(27)
    ; Another comment
IADD R3, R2 ; MATCH(5)

; ===================================================================
; ✅ SCENARIO 8: CSE with OR
; ===================================================================
MOV R1, R5 ; KEEP(28)
OR R1, R2 ; KEEP(29)
MOV R3, R5 ; KEEP(30)
OR R3, R2 ; MATCH(6)

; ===================================================================
; ✅ SCENARIO 9: CSE with AND
; ===================================================================
MOV R1, R5 ; KEEP(31)
AND R1, R2 ; KEEP(32)
MOV R3, R5 ; KEEP(33)
AND R3, R2 ; MATCH(7)

; ===================================================================
; ✅ SCENARIO 10: CSE with XOR
; ===================================================================
MOV R1, R5 ; KEEP(34)
XOR R1, R2 ; KEEP(35)
MOV R3, R5 ; KEEP(36)
XOR R3, R2 ; MATCH(8)

; ===================================================================
; ✅ SCENARIO 11: CSE with Floating Point
; ===================================================================
MOV R1, R5 ; KEEP(37)
FADD R1, R2 ; KEEP(38)
MOV R3, R5 ; KEEP(39)
FADD R3, R2 ; MATCH(9)

; ===================================================================
; ✅ SCENARIO 12: CSE with Negative Immediate
; ===================================================================
MOV R1, R5 ; KEEP(40)
IADD R1, -10 ; KEEP(41)
MOV R3, R5 ; KEEP(42)
IADD R3, -10 ; MATCH(10)

; ===================================================================
; ❌ SCENARIO 13: Different Immediate Values
; ===================================================================
MOV R1, R5 ; KEEP(43)
IADD R1, 42 ; KEEP(44)
MOV R3, R5 ; KEEP(45)
IADD R3, 100 ; KEEP(46)

; ===================================================================
; ❌ SCENARIO 14: Different Source Registers
; ===================================================================
MOV R1, R5 ; KEEP(47)
IADD R1, R2 ; KEEP(48)
MOV R3, R6 ; KEEP(49)
IADD R3, R2 ; KEEP(50)

; ===================================================================
; ✅ SCENARIO 15: Chained CSE
; Multiple CSE opportunities in sequence
; ===================================================================
MOV R1, R10 ; KEEP(51)
IADD R1, R2 ; KEEP(52)
MOV R3, R10 ; KEEP(53)
IADD R3, R2 ; MATCH(11)
MOV R4, R10 ; KEEP(54)
IADD R4, R2 ; MATCH(12)
