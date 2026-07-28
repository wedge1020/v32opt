Here's a **comprehensive test program** for `peephole_jmp_chain` that covers all edge cases:

```asm
; ===================================================================
; TEST: peephole_jmp_chain - All Scenarios
; Run with: ./v32opt test_jmp_chain.asm -fopt_peephole_jmp_chain -v
; ===================================================================

; ===================================================================
; ✅ SCENARIO 1: Simple Chain (SHOULD OPTIMIZE)
; JMP L1; L1: JMP L2 → JMP L2
; ===================================================================
JMP L1
L1:
JMP L2
L2:
MOV R1, 42
RET

; ===================================================================
; ✅ SCENARIO 2: Chain with Comments (SHOULD OPTIMIZE)
; ===================================================================
JMP L3
; This is a comment
; Another comment
L3:
JMP L4
L4:
MOV R2, 100
RET

; ===================================================================
; ❌ SCENARIO 3: Mismatched Target (MUST NOT OPTIMIZE)
; JMP L5; L6: JMP L7 → Stay as-is (JMP targets L5, not L6)
; ===================================================================
JMP L5
L6:
JMP L7
L5:
MOV R3, 200
L7:
RET

; ===================================================================
; ❌ SCENARIO 4: Code Between Label and JMP (MUST NOT OPTIMIZE)
; ===================================================================
JMP L8
L8:
MOV R4, 50   ; Code between label and next JMP
JMP L9
L9:
RET

; ===================================================================
; ✅ SCENARIO 5: Multiple Chains (SHOULD OPTIMIZE BOTH)
; ===================================================================
JMP L10
L10:
JMP L11
L11:
JMP L12
L12:
RET

; ===================================================================
; ✅ SCENARIO 6: Return Label Chain (SHOULD OPTIMIZE)
; Matches compiler output pattern: JMP __func_return; __func_return: JMP L13
; ===================================================================
JMP __function_test_return
__function_test_return:
JMP L13
L13:
RET

; ===================================================================
; ✅ SCENARIO 7: Blank Lines Between (SHOULD OPTIMIZE)
; ===================================================================
JMP L14

L14:

JMP L15
L15:
RET
```

---
### **Expected Output After Optimization**
| Scenario | Result | Reason |
|----------|--------|--------|
| 1 | `JMP L2` (optimized) | Direct chain |
| 2 | `JMP L4` (optimized) | Comments are skipped |
| 3 | **Unchanged** | Target mismatch |
| 4 | **Unchanged** | Code between label and JMP |
| 5 | `JMP L12` (both optimized) | Cascading chains |
| 6 | `JMP L13` (optimized) | Return label pattern |
| 7 | `JMP L15` (optimized) | Blank lines are skipped |

---
### **How to Test**
1. Save as `test_jmp_chain.asm`
2. Run: `./v32opt test_jmp_chain.asm -fopt_peephole_jmp_chain -v`
3. Verify:
   - Scenarios 1,2,5,6,7 show optimizations
   - Scenarios 3,4 remain unchanged
   - No visual artifacts when executed
