Here's a **comprehensive test program** for `peephole_loads`:

```asm
; ===================================================================
; TEST: peephole_loads - All Scenarios
; Run with: ./v32opt test_loads.asm -fopt_peephole_loads -v
; ===================================================================

; ===================================================================
; ✅ SCENARIO 1: Basic Consecutive Loads (SHOULD OPTIMIZE)
; ===================================================================
__function_test_basic:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2]
    MOV R3, [R2]        ; Should become: MOV R3, R1
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 2: Loads with Offset (SHOULD OPTIMIZE)
; ===================================================================
__function_test_offset:
    PUSH BP
    MOV BP, SP
    MOV R1, [BP+4]
    MOV R3, [BP+4]      ; Should become: MOV R3, R1
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 3: Negative Offset (SHOULD OPTIMIZE)
; ===================================================================
__function_test_neg_offset:
    PUSH BP
    MOV BP, SP
    MOV R1, [BP-8]
    MOV R3, [BP-8]      ; Should become: MOV R3, R1
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 4: Different Registers (MUST NOT OPTIMIZE)
; ===================================================================
__function_test_diff_reg:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2]
    MOV R3, [R4]        ; Different source registers
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 5: Different Offsets (MUST NOT OPTIMIZE)
; ===================================================================
__function_test_diff_offset:
    PUSH BP
    MOV BP, SP
    MOV R1, [BP+4]
    MOV R3, [BP+8]      ; Different offsets
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 6: Comments Between (MUST NOT OPTIMIZE)
; ===================================================================
__function_test_comments:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2]
    ; This comment breaks consecutiveness
    MOV R3, [R2]        ; Not consecutive with first MOV
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 7: Three Consecutive Loads (SHOULD OPTIMIZE BOTH)
; ===================================================================
__function_test_three:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2]
    MOV R3, [R2]        ; Should become: MOV R3, R1
    MOV R4, [R2]        ; Should become: MOV R4, R1 (not R3!)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 8: Non-Register Destination (MUST NOT OPTIMIZE)
; ===================================================================
__function_test_non_reg_dst:
    PUSH BP
    MOV BP, SP
    MOV [R1], [R2]      ; First MOV has indirect destination
    MOV R3, [R2]
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 9: Non-Indirect Source (MUST NOT OPTIMIZE)
; ===================================================================
__function_test_non_indirect:
    PUSH BP
    MOV BP, SP
    MOV R1, R2          ; First MOV has register source
    MOV R3, [R2]
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 10: Mixed Registers and Offsets
; ===================================================================
__function_test_mixed:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2+4]
    MOV R3, [R2+4]      ; Should become: MOV R3, R1
    MOV R5, [R6-8]
    MOV R7, [R6-8]      ; Should become: MOV R7, R5
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 11: BP-Based Loads (Common in Functions)
; ===================================================================
__function_test_bp:
    PUSH BP
    MOV BP, SP
    MOV R1, [BP+4]
    MOV R2, [BP+4]      ; Should become: MOV R2, R1
    MOV R3, [BP+8]
    MOV R4, [BP+8]      ; Should become: MOV R4, R3
    MOV SP, BP
    POP BP
    RET
```

---
### **Expected Output After Optimization**

| Test | Expected Result | Reason |
|------|----------------|--------|
| `test_basic` | `MOV R3, R1` | Basic consecutive loads |
| `test_offset` | `MOV R3, R1` | With positive offset |
| `test_neg_offset` | `MOV R3, R1` | With negative offset |
| `test_diff_reg` | **Unchanged** | Different source registers |
| `test_diff_offset` | **Unchanged** | Different offsets |
| `test_comments` | **Unchanged** | Comments break consecutiveness |
| `test_three` | `MOV R3, R1` + `MOV R4, R1` | Both second and third optimized |
| `test_non_reg_dst` | **Unchanged** | First MOV has non-register destination |
| `test_non_indirect` | **Unchanged** | First MOV has non-indirect source |
| `test_mixed` | `MOV R3, R1` + `MOV R7, R5` | Both pairs optimized |
| `test_bp` | `MOV R2, R1` + `MOV R4, R3` | BP-based loads |

---
### **How to Test**
1. Save as `test_loads.asm`
2. Run: `./v32opt test_loads.asm -fopt_peephole_loads -v`
3. Verify each function matches expected output
4. Confirm no instructions are incorrectly modified
