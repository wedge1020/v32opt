; ===================================================================
; TEST: peephole_jmp_chain - All Scenarios
; Run with: ./v32opt test_jmp_chain.asm -fopt_peephole_jmp_chain -v
; ===================================================================

; ===================================================================
; ✅ SCENARIO 1: Simple Chain (SHOULD OPTIMIZE)
; JMP L1; L1: JMP L2 → JMP L2
; ===================================================================

; 
JMP _L1
_L1:
JMP _L2
_L2:
MOV R1, 42
RET

; ===================================================================
; ✅ SCENARIO 2: Chain with Comments (SHOULD OPTIMIZE)
; ===================================================================
JMP _L3
; This is a comment
; Another comment
_L3:
JMP _L4
_L4:
MOV R2, 100
RET

; ===================================================================
; ❌ SCENARIO 3: Mismatched Target (MUST NOT OPTIMIZE)
; JMP L5; L6: JMP L7 → Stay as-is (JMP targets L5, not L6)
; ===================================================================
JMP _L5
_L6:
JMP _L7
_L5:
MOV R3, 200
_L7:
RET

; ===================================================================
; ❌ SCENARIO 4: Code Between Label and JMP (MUST NOT OPTIMIZE)
; ===================================================================
JMP _L8
_L8:
MOV R4, 50   ; Code between label and next JMP
JMP _L9
_L9:
RET

; ===================================================================
; ✅ SCENARIO 5: Multiple Chains (SHOULD OPTIMIZE BOTH)
; ===================================================================
JMP _L10
_L10:
JMP _L11
_L11:
JMP _L12
_L12:
RET

; ===================================================================
; ✅ SCENARIO 6: Return Label Chain (SHOULD OPTIMIZE)
; Matches compiler output pattern: JMP __func_return; __func_return: JMP L13
; ===================================================================
JMP __function_test_return
__function_test_return:
JMP _L13
_L13:
RET

; ===================================================================
; ✅ SCENARIO 7: Blank Lines Between (SHOULD OPTIMIZE)
; ===================================================================
JMP _L14

_L14:

JMP _L15
_L15:
RET
