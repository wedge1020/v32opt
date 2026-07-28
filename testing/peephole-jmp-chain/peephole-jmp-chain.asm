; ===================================================================
; TEST: peephole-jmp-chain - All Scenarios
; Run with: ./v32opt peephole-jmp-chain.asm -fpeephole-jmp-chain -v
; ===================================================================

; ===================================================================
; ✅ SCENARIO 1: Simple Chain (SHOULD OPTIMIZE)
; JMP L1; L1: JMP L2 → JMP L2
; ===================================================================

; 
JMP _L1 ; MATCH
_L1:
JMP _L2
_L2:
MOV R1, 42
RET

; ===================================================================
; ✅ SCENARIO 2: Chain with Comments (SHOULD OPTIMIZE)
; ===================================================================
JMP _L3 ; MATCH
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
JMP _L5 ; KEEP
_L6:
JMP _L7 ; KEEP
_L5:
MOV R3, 200
_L7:
RET

; ===================================================================
; ❌ SCENARIO 4: Code Between Label and JMP (MUST NOT OPTIMIZE)
; ===================================================================
JMP _L8  ; KEEP
_L8:
MOV R4, 50   ; Code between label and next JMP
JMP _L9  ; KEEP
_L9:
RET

; ===================================================================
; ✅ SCENARIO 5: Multiple Chains (SHOULD OPTIMIZE BOTH)
; ===================================================================
JMP _L10 ; MATCH
_L10:
JMP _L11 ; MATCH
_L11:
JMP _L12 ; MATCH
_L12:
RET

; ===================================================================
; ✅ SCENARIO 6: Return Label Chain (SHOULD OPTIMIZE)
; Matches compiler output pattern: JMP __func_return; __func_return: JMP L13
; ===================================================================
JMP __function_test_return ; MATCH
__function_test_return:
JMP _L13 ; MATCH
_L13:
RET

; ===================================================================
; ✅ SCENARIO 7: Blank Lines Between (SHOULD OPTIMIZE)
; ===================================================================
JMP _L14 ; MATCH

_L14:

JMP _L15 ; MATCH
_L15:
RET
