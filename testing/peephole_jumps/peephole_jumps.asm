; ===================================================================
; TEST: peephole_jumps - Rigorous Edge Cases & Coverage
; Run with: ./v32opt test_jumps.asm -fopt_peephole_jumps -v
; ===================================================================

; ===================================================================
; ✅ SCENARIO 1: Basic Redundant Unconditional Jump to Next Label
; ===================================================================
JMP _L1 ; MATCH
_L1:
MOV R1, 42 ; KEEP
RET ; KEEP


; ===================================================================
; ✅ SCENARIO 2: Redundant Conditional Jumps (JT/JF) to Next Label
; ===================================================================
JT R0, _L2 ; MATCH
_L2:
JF R1, _L3 ; MATCH
_L3:
MOV R1, 42 ; KEEP
RET ; KEEP


; ===================================================================
; ✅ SCENARIO 3: Redundant Jump Over Indented Comments & Blank Lines
; ===================================================================
JMP _L4 ; MATCH
    ; Indented comment 1
    ; Indented comment 2

_L4:
MOV R1, 42 ; KEEP
RET ; KEEP


; ===================================================================
; ❌ SCENARIO 4: Valid Backward Jump / Infinite Loop (must keep)
; ===================================================================
_L5_loop:
MOV R1, 1 ; KEEP
JMP _L5_loop ; KEEP


; ===================================================================
; ❌ SCENARIO 5: Jump to Non-Immediate Label (must keep)
; ===================================================================
JMP _L7 ; KEEP
_L6:
MOV R1, 42 ; KEEP
_L7:
MOV R2, 100 ; KEEP
RET ; KEEP


; ===================================================================
; ❌ SCENARIO 6: Non-Label Jump Operands (Indirect / Fixed Address)
; ===================================================================
JMP R0 ; KEEP
MOV R1, 42 ; KEEP
JMP 0x1000 ; KEEP
MOV R2, 100 ; KEEP
RET ; KEEP


; ===================================================================
; ✅ SCENARIO 7: Unreachable Code Elimination after JMP
; ===================================================================
JMP _L8 ; KEEP
MOV R1, 99 ; MATCH
MOV R2, 100 ; MATCH
_L8:
MOV R3, 42 ; KEEP
RET ; KEEP


; ===================================================================
; ✅ SCENARIO 8: Unreachable Code Elimination after RET and HLT
; ===================================================================
RET ; KEEP
MOV R1, 123 ; MATCH
IADD R1, 1 ; MATCH
_L9:
HLT ; KEEP
MOV R2, 456 ; MATCH
_L10:
MOV R3, 1 ; KEEP
RET ; KEEP


; ===================================================================
; ✅ SCENARIO 9: Unreachable Code Interspersed with Indented Comments
; ===================================================================
JMP _L11 ; KEEP
    ; Comment inside dead code block
MOV R1, 10 ; MATCH
    ; Another comment inside dead code block
MOV R2, 20 ; MATCH
_L11:
MOV R1, 42 ; KEEP
RET ; KEEP


; ===================================================================
; ✅ SCENARIO 10: Branch Over Jump Inversion (JF + JMP -> JT)
; ===================================================================
JF R0, _else_1 ; MATCH
JMP _then_1 ; MATCH
_else_1:
MOV R1, 10 ; KEEP
_then_1:
MOV R2, 20 ; KEEP
RET ; KEEP


; ===================================================================
; ✅ SCENARIO 11: Branch Over Jump Inversion (JT + JMP -> JF)
; ===================================================================
JT R0, _else_2 ; MATCH
JMP _then_2 ; MATCH
_else_2:
MOV R1, 10 ; KEEP
_then_2:
MOV R2, 20 ; KEEP
RET ; KEEP


; ===================================================================
; ✅ SCENARIO 12: Branch Over Jump with Indented Comments
; ===================================================================
JF R0, _else_3 ; MATCH
    ; Indented comment between branch and jump
JMP _then_3 ; MATCH
    ; Indented comment before target label
_else_3:
MOV R1, 10 ; KEEP
_then_3:
MOV R2, 20 ; KEEP
RET ; KEEP


; ===================================================================
; ❌ SCENARIO 13: Branch Over Jump Fail - Misaligned Target Label
; ===================================================================
JF R0, _wrong_label ; KEEP
JMP _end_4 ; KEEP
_other_label:
MOV R1, 10 ; KEEP
_wrong_label:
_end_4:
RET ; KEEP


; ===================================================================
; ❌ SCENARIO 14: Branch Over Jump Fail - Second Jump is NOT JMP
; ===================================================================
JF R0, _else_5 ; KEEP
JT R1, _end_5 ; KEEP
_else_5:
MOV R1, 10 ; KEEP
_end_5:
RET ; KEEP


; ===================================================================
; ✅ SCENARIO 15: Chained Optimization (Cascading Jump Removals)
; ===================================================================
JMP _chain_1 ; MATCH
_chain_1:
JMP _chain_2 ; MATCH
_chain_2:
JF R0, _else_chained ; MATCH
JMP _end_chained ; MATCH
_else_chained:
_end_chained:
MOV R1, 42 ; KEEP
RET ; KEEP
