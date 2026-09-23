; ===================================================================
; TEST: peephole-jumps - Rigorous Edge Cases & Coverage
; Run with: ./v32opt peephole-jumps.asm -fpeephole-jumps -v
; ===================================================================

; ===================================================================
; ✅ SCENARIO 1: Basic Redundant Unconditional Jump to Next Label
; ===================================================================
_jumps_s1:
JMP _L1 ; MATCH(1)
_L1:
MOV R1, 42 ; KEEP(1)
RET ; KEEP(2)


; ===================================================================
; ✅ SCENARIO 2: Redundant Conditional Jumps (JT/JF) to Next Label
; ===================================================================
_jumps_s2:
JT R0, _L2 ; MATCH(2)
_L2:
JF R1, _L3 ; MATCH(3)
_L3:
MOV R1, 42 ; KEEP(3)
RET ; KEEP(4)


; ===================================================================
; ✅ SCENARIO 3: Redundant Jump Over Indented Comments & Blank Lines
; ===================================================================
_jumps_s3:
JMP _L4 ; MATCH(4)
    ; Indented comment 1
    ; Indented comment 2

_L4:
MOV R1, 42 ; KEEP(5)
RET ; KEEP(6)


; ===================================================================
; ❌ SCENARIO 4: Valid Backward Jump / Infinite Loop (must keep)
; ===================================================================
_jumps_s4:
_L5_loop:
MOV R1, 1 ; KEEP(7)
JMP _L5_loop ; KEEP(8)


; ===================================================================
; ❌ SCENARIO 5: Jump to Non-Immediate Label (must keep)
; ===================================================================
_jumps_s5:
JMP _L7 ; KEEP(9)
_L6:
MOV R1, 42 ; KEEP(10)
_L7:
MOV R2, 100 ; KEEP(11)
RET ; KEEP(12)


; ===================================================================
; ❌ SCENARIO 6: Non-Label Jump Operands (Indirect / Fixed Address)
; ===================================================================
_jumps_s6:
JMP R0 ; KEEP(13)
MOV R1, 42 ; KEEP(14)
JMP 0x1000 ; KEEP(15)
MOV R2, 100 ; KEEP(16)
RET ; KEEP(17)


; ===================================================================
; ✅ SCENARIO 7: Unreachable Code Elimination after JMP
; ===================================================================
_jumps_s7:
JMP _L8 ; MATCH(18) (becomes jump-to-next-label once dead code is gone)
MOV R1, 99 ; MATCH(5)
MOV R2, 100 ; MATCH(6)
_L8:
MOV R3, 42 ; KEEP(19)
RET ; KEEP(20)


; ===================================================================
; ✅ SCENARIO 8: Unreachable Code Elimination after RET and HLT
; ===================================================================
_jumps_s8:
RET ; KEEP(21)
MOV R1, 123 ; KEEP(21) (target of JMP 0x1000 is not a label: unknowable)
IADD R1, 1 ; KEEP(22) (target of JMP 0x1000 is not a label: unknowable)
_L9:
HLT ; KEEP(22)
MOV R2, 456 ; KEEP(23) (target of JMP 0x1000 is not a label: unknowable)
_L10:
MOV R3, 1 ; KEEP(23)
RET ; KEEP(24)


; ===================================================================
; ✅ SCENARIO 9: Unreachable Code Interspersed with Indented Comments
; ===================================================================
_jumps_s9:
JMP _L11 ; MATCH(25) (becomes jump-to-next-label once dead code is gone)
    ; Comment inside dead code block
MOV R1, 10 ; MATCH(10)
    ; Another comment inside dead code block
MOV R2, 20 ; MATCH(11)
_L11:
MOV R1, 42 ; KEEP(26)
RET ; KEEP(27)


; ===================================================================
; ✅ SCENARIO 10: Branch Over Jump Inversion (JF + JMP -> JT)
; ===================================================================
_jumps_s10:
JF R0, _else_1 ; MATCH(12)
JMP _then_1 ; MATCH(13)
_else_1:
MOV R1, 10 ; KEEP(28)
_then_1:
MOV R2, 20 ; KEEP(29)
RET ; KEEP(30)


; ===================================================================
; ✅ SCENARIO 11: Branch Over Jump Inversion (JT + JMP -> JF)
; ===================================================================
_jumps_s11:
JT R0, _else_2 ; MATCH(14)
JMP _then_2 ; MATCH(15)
_else_2:
MOV R1, 10 ; KEEP(31)
_then_2:
MOV R2, 20 ; KEEP(32)
RET ; KEEP(33)


; ===================================================================
; ✅ SCENARIO 12: Branch Over Jump with Indented Comments
; ===================================================================
_jumps_s12:
JF R0, _else_3 ; MATCH(16)
    ; Indented comment between branch and jump
JMP _then_3 ; MATCH(17)
    ; Indented comment before target label
_else_3:
MOV R1, 10 ; KEEP(34)
_then_3:
MOV R2, 20 ; KEEP(35)
RET ; KEEP(36)


; ===================================================================
; ❌ SCENARIO 13: Branch Over Jump Fail - Misaligned Target Label
; ===================================================================
_jumps_s13:
JF R0, _wrong_label ; KEEP(37)
JMP _end_4 ; KEEP(38)
_other_label:
MOV R1, 10 ; KEEP(39)
_wrong_label:
_end_4:
RET ; KEEP(40)


; ===================================================================
; ❌ SCENARIO 14: Branch Over Jump Fail - Second Jump is NOT JMP
; ===================================================================
_jumps_s14:
JF R0, _else_5 ; KEEP(41)
JT R1, _end_5 ; KEEP(42)
_else_5:
MOV R1, 10 ; KEEP(43)
_end_5:
RET ; KEEP(44)


; ===================================================================
; ✅ SCENARIO 15: Chained Optimization (Cascading Jump Removals)
; ===================================================================
_jumps_s15:
JMP _chain_1 ; MATCH(18)
_chain_1:
JMP _chain_2 ; MATCH(19)
_chain_2:
JF R0, _else_chained ; MATCH(20)
JMP _end_chained ; MATCH(21)
_else_chained:
_end_chained:
MOV R1, 42 ; KEEP(45)
RET ; KEEP(46)
