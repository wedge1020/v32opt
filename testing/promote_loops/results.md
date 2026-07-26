Here's a **comprehensive test program** for `pass_promote_loop_registers`:

```asm
; ===================================================================
; TEST: pass_promote_loop_registers - All Scenarios
; Run with: ./v32opt test_promote_loops.asm -fopt_promote_loops -v
; ===================================================================

; ===================================================================
; ✅ SCENARIO 1: Simple CALL-Free Loop (SHOULD PROMOTE)
; Basic loop with local variable access
; ===================================================================
__for_1_start:
    IADD R1, 1
    MOV [BP-4], R1
    MOV R2, [BP-4]
    IADD R3, R2
    JT R1, __for_1_start
__for_1_exit:
    RET

; ===================================================================
; ✅ SCENARIO 2: Multiple Local Variables (SHOULD PROMOTE)
; ===================================================================
__for_2_start:
    MOV [BP-4], R1
    MOV [BP-8], R2
    MOV R3, [BP-4]
    MOV R4, [BP-8]
    IADD R3, R4
    MOV [BP-4], R3
    JT R1, __for_2_start
__for_2_exit:
    RET

; ===================================================================
; ✅ SCENARIO 3: Nested Conditionals (SHOULD PROMOTE)
; All branches stay within loop
; ===================================================================
__while_1_start:
    MOV R1, [BP-4]
    IEQ R2, R1
    JT R2, __while_1_skip
    MOV [BP-4], R3
    JMP __while_1_continue
__while_1_skip:
    MOV [BP-4], R4
__while_1_continue:
    JT R1, __while_1_start
__while_1_exit:
    RET

; ===================================================================
; ✅ SCENARIO 4: Loop with Arithmetic on Locals (SHOULD PROMOTE)
; ===================================================================
__for_3_start:
    MOV R1, [BP-4]
    IADD R1, 5
    MOV [BP-4], R1
    MOV R2, [BP-4]
    ISUB R2, 2
    MOV [BP-4], R2
    JT R3, __for_3_start
__for_3_exit:
    RET

; ===================================================================
; ❌ SCENARIO 5: Loop with CALL (MUST NOT PROMOTE)
; ===================================================================
__for_4_start:
    CALL some_function
    MOV [BP-4], R1
    MOV R2, [BP-4]
    JT R3, __for_4_start
__for_4_exit:
    RET

; ===================================================================
; ❌ SCENARIO 6: Loop with RET (MUST NOT PROMOTE)
; ===================================================================
__for_5_start:
    MOV [BP-4], R1
    MOV R2, [BP-4]
    RET                    ; RET inside loop
    JT R3, __for_5_start
__for_5_exit:
    RET

; ===================================================================
; ❌ SCENARIO 7: Loop with HLT (MUST NOT PROMOTE)
; ===================================================================
__for_6_start:
    MOV [BP-4], R1
    MOV R2, [BP-4]
    HLT                    ; HLT inside loop
    JT R3, __for_6_start
__for_6_exit:
    RET

; ===================================================================
; ❌ SCENARIO 8: Loop with Direct BP Usage (MUST NOT PROMOTE)
; ===================================================================
__for_7_start:
    MOV R1, BP           ; Direct BP usage
    MOV [BP-4], R1
    MOV R2, [BP-4]
    JT R3, __for_7_start
__for_7_exit:
    RET

; ===================================================================
; ❌ SCENARIO 9: Loop with BP in Arithmetic (MUST NOT PROMOTE)
; ===================================================================
__for_8_start:
    IADD R1, BP         ; BP in arithmetic
    MOV [BP-4], R1
    MOV R2, [BP-4]
    JT R3, __for_8_start
__for_8_exit:
    RET

; ===================================================================
; ❌ SCENARIO 10: Loop with BP in Indirect (MUST NOT PROMOTE)
; ===================================================================
__for_9_start:
    MOV R1, [BP]        ; BP in indirect
    MOV [BP-4], R1
    MOV R2, [BP-4]
    JT R3, __for_9_start
__for_9_exit:
    RET

; ===================================================================
; ❌ SCENARIO 11: Loop with External Branch (MUST NOT PROMOTE)
; Branch jumps to label outside loop
; ===================================================================
__for_10_start:
    MOV [BP-4], R1
    MOV R2, [BP-4]
    JT R3, external_label  ; External jump
    JMP __for_10_start
__for_10_exit:
    RET
external_label:
    RET

; ===================================================================
; ❌ SCENARIO 12: Loop with PUSH/POP (MUST NOT PROMOTE)
; ===================================================================
__for_11_start:
    PUSH R1
    MOV [BP-4], R2
    MOV R3, [BP-4]
    POP R1
    JT R4, __for_11_start
__for_11_exit:
    RET

; ===================================================================
; ✅ SCENARIO 13: While Loop (SHOULD PROMOTE)
; ===================================================================
__while_2_start:
    MOV R1, [BP-4]
    IEQ R2, R1
    JF R2, __while_2_exit
    IADD R1, 1
    MOV [BP-4], R1
    JMP __while_2_start
__while_2_exit:
    RET

; ===================================================================
; ✅ SCENARIO 14: Loop with Multiple Exits (SHOULD PROMOTE)
; ===================================================================
__for_12_start:
    MOV [BP-4], R1
    MOV R2, [BP-4]
    IEQ R3, R2
    JT R3, __for_12_start
    JMP __for_12_exit1
__for_12_exit1:
    RET
__for_12_exit2:
    RET

; ===================================================================
; ❌ SCENARIO 15: Loop with Branch to Function (MUST NOT PROMOTE)
; ===================================================================
__for_13_start:
    MOV [BP-4], R1
    MOV R2, [BP-4]
    CALL some_function   ; CALL inside loop
    JT R3, __for_13_start
__for_13_exit:
    RET

; ===================================================================
; ✅ SCENARIO 16: Complex Safe Loop (SHOULD PROMOTE)
; Multiple slots, complex logic
; ===================================================================
__for_14_start:
    MOV [BP-4], R1
    MOV [BP-8], R2
    MOV R3, [BP-4]
    MOV R4, [BP-8]
    IADD R5, R3
    ISUB R5, R4
    MOV [BP-4], R5
    MOV R6, [BP-4]
    IEQ R7, R6
    JF R7, __for_14_exit
    JMP __for_14_start
__for_14_exit:
    RET
```

---
### **Expected Output After Optimization**

| Test | Expected Result | Reason |
|------|----------------|--------|
| Scenario 1 | Local variable promoted to register | Safe loop |
| Scenario 2 | Multiple locals promoted | Safe loop |
| Scenario 3 | Locals promoted despite nested conditionals | All branches stay in loop |
| Scenario 4 | Locals promoted with arithmetic | Safe loop |
| Scenario 5 | **Unchanged** | CALL in loop |
| Scenario 6 | **Unchanged** | RET in loop |
| Scenario 7 | **Unchanged** | HLT in loop |
| Scenario 8 | **Unchanged** | Direct BP usage |
| Scenario 9 | **Unchanged** | BP in arithmetic |
| Scenario 10 | **Unchanged** | BP in indirect |
| Scenario 11 | **Unchanged** | External branch |
| Scenario 12 | **Unchanged** | PUSH/POP in loop |
| Scenario 13 | Locals promoted | While loop pattern |
| Scenario 14 | Locals promoted, stores at all exits | Multiple exits |
| Scenario 15 | **Unchanged** | CALL in loop |
| Scenario 16 | Multiple locals promoted | Complex but safe |

---
### **How to Test**
1. Save as `test_promote_loops.asm`
2. Run: `./v32opt test_promote_loops.asm -fopt_promote_loops -v`
3. Verify each function matches expected output
4. Confirm:
   - Pre-header loads are inserted after loop label
   - Exit stores are inserted before all loop exits
   - All `[BP-N]` references are replaced with assigned registers
   - No unsafe loops are modified
