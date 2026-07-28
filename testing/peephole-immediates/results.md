Here's a **comprehensive test program** for `peephole_immediates`:

```asm
; ===================================================================
; TEST: peephole_immediates - All Scenarios
; Run with: ./v32opt test_immediates.asm -fopt_peephole_immediates -v
; ===================================================================

; ===================================================================
; ✅ SCENARIO 1: Cancellation (SHOULD REMOVE BOTH)
; ===================================================================

; --- IADD + ISUB cancel ---
__function_test_cancel_add_sub:
    PUSH BP
    MOV BP, SP
    IADD R1, 5
    ISUB R1, 5          ; Both should be removed (cancels to 0)
    MOV SP, BP
    POP BP
    RET

; --- ISUB + IADD cancel ---
__function_test_cancel_sub_add:
    PUSH BP
    MOV BP, SP
    ISUB R1, 10
    IADD R1, 10         ; Both should be removed (cancels to 0)
    MOV SP, BP
    POP BP
    RET

; --- Multiple cancellations ---
__function_test_cancel_multiple:
    PUSH BP
    MOV BP, SP
    IADD R1, 5
    ISUB R1, 5          ; Both removed
    IADD R2, 10
    ISUB R2, 10         ; Both removed
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 2: Positive Combination (SHOULD COMBINE)
; ===================================================================

; --- IADD + IADD ---
__function_test_combine_add_add:
    PUSH BP
    MOV BP, SP
    IADD R1, 5
    IADD R1, 3          ; Should become: IADD R1, 8
    MOV SP, BP
    POP BP
    RET

; --- ISUB + ISUB (negative + negative = more negative) ---
__function_test_combine_sub_sub:
    PUSH BP
    MOV BP, SP
    ISUB R1, 5
    ISUB R1, 3          ; Should become: ISUB R1, 8
    MOV SP, BP
    POP BP
    RET

; --- IADD + IADD with negative ---
__function_test_combine_add_add_neg:
    PUSH BP
    MOV BP, SP
    IADD R1, 5
    IADD R1, -3         ; Should become: IADD R1, 2
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 3: Negative Combination (SHOULD COMBINE)
; ===================================================================

; --- IADD + ISUB (result negative) ---
__function_test_combine_add_sub_neg:
    PUSH BP
    MOV BP, SP
    IADD R1, 5
    ISUB R1, 10         ; Should become: ISUB R1, 5
    MOV SP, BP
    POP BP
    RET

; --- ISUB + IADD (result negative) ---
__function_test_combine_sub_add_neg:
    PUSH BP
    MOV BP, SP
    ISUB R1, 10
    IADD R1, 3          ; Should become: ISUB R1, 7
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 4: Different Registers (MUST NOT COMBINE)
; ===================================================================

; --- Different destination registers ---
__function_test_diff_reg:
    PUSH BP
    MOV BP, SP
    IADD R1, 5
    IADD R2, 3          ; Different registers - unchanged
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 5: Non-Consecutive (MUST NOT COMBINE)
; ===================================================================

; --- Code between operations ---
__function_test_non_consecutive:
    PUSH BP
    MOV BP, SP
    IADD R1, 5
    MOV R2, 10          ; Breaks consecutiveness
    IADD R1, 3          ; Not consecutive - unchanged
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 6: With Comments (SHOULD COMBINE)
; ===================================================================

; --- Comments between ---
__function_test_comments:
    PUSH BP
    MOV BP, SP
    IADD R1, 5
    ; comment
    IADD R1, 3          ; Should become: IADD R1, 8
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 7: Three in a Row (SHOULD COMBINE)
; ===================================================================

; --- IADD + IADD + IADD ---
__function_test_three_add:
    PUSH BP
    MOV BP, SP
    IADD R1, 2
    IADD R1, 3          ; Should become: IADD R1, 5
    IADD R1, 1          ; Then combine with previous: IADD R1, 6
    MOV SP, BP
    POP BP
    RET

; --- Mixed operations ---
__function_test_three_mixed:
    PUSH BP
    MOV BP, SP
    IADD R1, 10
    ISUB R1, 3          ; Should become: IADD R1, 7
    IADD R1, 2          ; Then combine: IADD R1, 9
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 8: Non-Immediate Operands (MUST NOT COMBINE)
; ===================================================================

; --- Register source ---
__function_test_reg_src:
    PUSH BP
    MOV BP, SP
    IADD R1, R2
    IADD R1, 5          ; First has register source - unchanged
    MOV SP, BP
    POP BP
    RET

; --- Indirect source ---
__function_test_indirect_src:
    PUSH BP
    MOV BP, SP
    IADD R1, [R2]
    IADD R1, 5          ; First has indirect source - unchanged
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 9: Zero Values
; ===================================================================

; --- IADD + ISUB with zero ---
__function_test_zero:
    PUSH BP
    MOV BP, SP
    IADD R1, 0          ; This is handled by peephole_algebra, not here
    ISUB R1, 5
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 10: Complex Combination
; ===================================================================

; --- Mixed positive and negative ---
__function_test_complex:
    PUSH BP
    MOV BP, SP
    IADD R1, 10
    ISUB R1, 4          ; Should become: IADD R1, 6
    IADD R2, 5
    IADD R2, -2         ; Should become: IADD R2, 3
    ISUB R3, 8
    ISUB R3, 3          ; Should become: ISUB R3, 11
    MOV SP, BP
    POP BP
    RET
```

---
### **Expected Output After Optimization**

| Test | Expected Result | Reason |
|------|----------------|--------|
| `test_cancel_add_sub` | Both removed | Cancellation (5 - 5 = 0) |
| `test_cancel_sub_add` | Both removed | Cancellation (-10 + 10 = 0) |
| `test_cancel_multiple` | Both pairs removed | Multiple cancellations |
| `test_combine_add_add` | `IADD R1, 8` | Positive combination |
| `test_combine_sub_sub` | `ISUB R1, 8` | Negative combination |
| `test_combine_add_add_neg` | `IADD R1, 2` | Mixed signs, positive result |
| `test_combine_add_sub_neg` | `ISUB R1, 5` | Mixed signs, negative result |
| `test_combine_sub_add_neg` | `ISUB R1, 7` | Mixed signs, negative result |
| `test_diff_reg` | **Unchanged** | Different destination registers |
| `test_non_consecutive` | **Unchanged** | Non-consecutive operations |
| `test_comments` | `IADD R1, 8` | Comments skipped |
| `test_three_add` | `IADD R1, 6` | Cascading combination |
| `test_three_mixed` | `IADD R1, 9` | Cascading with mixed ops |
| `test_reg_src` | **Unchanged** | Register source operand |
| `test_indirect_src` | **Unchanged** | Indirect source operand |
| `test_zero` | `ISUB R1, 5` | Zero handled by algebra, not here |
| `test_complex` | `IADD R1, 6` + `IADD R2, 3` + `ISUB R3, 11` | Multiple combinations |

---
### **How to Test**
1. Save as `test_immediates.asm`
2. Run: `./v32opt test_immediates.asm -fopt_peephole_immediates -v`
3. Verify each function matches expected output
4. Confirm no instructions are incorrectly modified
