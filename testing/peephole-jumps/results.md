Here's a **comprehensive test program** for `peephole_jumps`:

```asm
; ===================================================================
; TEST: peephole_jumps - All Scenarios
; Run with: ./v32opt test_jumps.asm -fopt_peephole_jumps -v
; ===================================================================

; ===================================================================
; ✅ SCENARIO 1: Basic Jump to Next Label (SHOULD REMOVE JMP)
; ===================================================================
JMP L1
L1:
MOV R1, 42
RET

; ===================================================================
; ✅ SCENARIO 2: Jump with Comments (SHOULD REMOVE JMP)
; ===================================================================
JMP L2
; This is a comment
; Another comment
L2:
MOV R1, 42
RET

; ===================================================================
; ✅ SCENARIO 3: Jump with Blank Lines (SHOULD REMOVE JMP)
; ===================================================================
JMP L3

L3:
MOV R1, 42
RET

; ===================================================================
; ❌ SCENARIO 4: Jump to Different Label (MUST NOT REMOVE)
; ===================================================================
JMP L5
L4:
MOV R1, 42
L5:
MOV R2, 100
RET

; ===================================================================
; ❌ SCENARIO 5: Label Not Immediate Next (MUST NOT REMOVE)
; ===================================================================
JMP L6
MOV R1, 42
L6:
MOV R2, 100
RET

; ===================================================================
; ✅ SCENARIO 6: Multiple Redundant Jumps (SHOULD REMOVE BOTH)
; ===================================================================
JMP L7
L7:
JMP L8
L8:
MOV R1, 42
RET

; ===================================================================
; ✅ SCENARIO 7: Case-Insensitive Match (SHOULD REMOVE JMP)
; ===================================================================
JMP my_label
My_Label:
MOV R1, 42
RET

; ===================================================================
; ❌ SCENARIO 8: Jump to Register (MUST NOT REMOVE)
; ===================================================================
JMP R1
MOV R2, 42
RET

; ===================================================================
; ❌ SCENARIO 9: Jump to Immediate Address (MUST NOT REMOVE)
; ===================================================================
JMP 0x1000
MOV R1, 42
RET

; ===================================================================
; ✅ SCENARIO 10: Function Entry Pattern
; ===================================================================
__function_test_entry:
    JMP __function_test_entry_start
__function_test_entry_start:
    MOV R1, 42
    RET

; ===================================================================
; ✅ SCENARIO 11: Return Label Pattern
; ===================================================================
JMP __function_test_return
__function_test_return:
MOV R1, 42
RET

; ===================================================================
; ✅ SCENARIO 12: Mixed Valid and Invalid
; ===================================================================
JMP L10
L10:
MOV R1, 1
JMP L11
L12:
MOV R2, 2
JMP L12
L11:
MOV R3, 3
RET
```

---
### **Expected Output After Optimization**

| Test | Expected Result | Reason |
|------|----------------|--------|
| Scenario 1 | JMP removed, label remains | Basic case |
| Scenario 2 | JMP removed, comments + label remain | Comments skipped |
| Scenario 3 | JMP removed, blank lines + label remain | Blank lines skipped |
| Scenario 4 | **Unchanged** | Target doesn't match next label |
| Scenario 5 | **Unchanged** | Label not immediate next |
| Scenario 6 | Both JMPs removed | Cascading optimization |
| Scenario 7 | JMP removed | Case-insensitive match |
| Scenario 8 | **Unchanged** | Register target |
| Scenario 9 | **Unchanged** | Immediate target |
| Scenario 10 | JMP removed | Function entry pattern |
| Scenario 11 | JMP removed | Return label pattern |
| Scenario 12 | JMP L10 and JMP L12 removed, JMP L11 unchanged | Mixed valid/invalid |

---
### **How to Test**
1. Save as `test_jumps.asm`
2. Run: `./v32opt test_jumps.asm -fopt_peephole_jumps -v`
3. Verify each scenario matches expected output
4. Confirm no valid jumps are incorrectly removed
