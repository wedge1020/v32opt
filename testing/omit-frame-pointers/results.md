Here's a **rigorous test program** for `omit_frame_pointers` covering all edge cases:

```asm
; ===================================================================
; TEST: omit_frame_pointers - All Scenarios
; Run with: ./v32opt test_frame.asm -fopt_omit_frame_pointers -v
; ===================================================================

; ===================================================================
; ✅ SCENARIO 1: Simple Function (SHOULD ELIMINATE)
; No BP usage, standard prologue/epilogue
; ===================================================================
__function_simple:
    PUSH BP
    MOV BP, SP
    MOV R1, 42
    MOV R2, 100
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 2: With Return Label (SHOULD ELIMINATE)
; Return label between body and epilogue
; ===================================================================
__function_with_return_label:
    PUSH BP
    MOV BP, SP
    MOV R1, 42
    MOV R2, 100
__function_with_return_label_return:
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 3: Local Variables (SHOULD ELIMINATE)
; Uses [BP-N] but not BP register directly
; ===================================================================
__function_local_vars:
    PUSH BP
    MOV BP, SP
    MOV [BP-4], R1
    MOV R2, [BP-4]
    MOV [BP-8], R3
    MOV R4, [BP-8]
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 4: Multiple Local Variables (SHOULD ELIMINATE)
; ===================================================================
__function_many_locals:
    PUSH BP
    MOV BP, SP
    MOV [BP-4], R1
    MOV [BP-8], R2
    MOV [BP-12], R3
    MOV R4, [BP-4]
    MOV R5, [BP-8]
    MOV R6, [BP-12]
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 5: Direct BP Usage (MUST NOT ELIMINATE)
; Uses BP register directly
; ===================================================================
__function_direct_bp:
    PUSH BP
    MOV BP, SP
    MOV R1, BP          ; Direct BP usage
    MOV R2, 42
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 6: BP in Arithmetic (MUST NOT ELIMINATE)
; ===================================================================
__function_bp_arith:
    PUSH BP
    MOV BP, SP
    IADD R1, BP        ; BP in arithmetic
    ISUB R2, BP
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 7: BP in Indirect (MUST NOT ELIMINATE)
; ===================================================================
__function_bp_indirect:
    PUSH BP
    MOV BP, SP
    MOV R1, [BP]       ; BP in indirect
    MOV [BP+4], R2
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 8: Nested Control Flow (MUST NOT ELIMINATE)
; Has internal labels
; ===================================================================
__function_nested:
    PUSH BP
    MOV BP, SP
    MOV R1, 1
    JT R1, loop_start
    JMP loop_end
loop_start:
    IADD R1, 1
    JT R1, loop_start
loop_end:
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 9: Missing Prologue (MUST NOT ELIMINATE)
; ===================================================================
__function_no_prologue:
    MOV R1, 42
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 10: Missing Epilogue (MUST NOT ELIMINATE)
; ===================================================================
__function_no_epilogue:
    PUSH BP
    MOV BP, SP
    MOV R1, 42
    RET

; ===================================================================
; ❌ SCENARIO 11: Partial Prologue (MUST NOT ELIMINATE)
; Only PUSH BP, missing MOV BP, SP
; ===================================================================
__function_partial_prologue:
    PUSH BP
    MOV R1, 42
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 12: Partial Epilogue (MUST NOT ELIMINATE)
; Only POP BP, missing MOV SP, BP
; ===================================================================
__function_partial_epilogue:
    PUSH BP
    MOV BP, SP
    MOV R1, 42
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 13: Empty Function (SHOULD ELIMINATE)
; ===================================================================
__function_empty:
    PUSH BP
    MOV BP, SP
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 14: Function with CALL (SHOULD ELIMINATE)
; CALL doesn't use BP directly
; ===================================================================
__function_with_call:
    PUSH BP
    MOV BP, SP
    CALL some_func
    MOV R1, 42
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ SCENARIO 15: Function Modifying BP (MUST NOT ELIMINATE)
; ===================================================================
__function_modify_bp:
    PUSH BP
    MOV BP, SP
    MOV BP, R1          ; Modifies BP
    MOV R2, 42
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 16: Complex Function (SHOULD ELIMINATE)
; Multiple operations, no BP usage
; ===================================================================
__function_complex:
    PUSH BP
    MOV BP, SP
    MOV R1, 10
    IADD R2, R1
    ISUB R3, 5
    IMUL R4, 2
    IDIV R5, 2
    MOV SP, BP
    POP BP
    RET
```

---
### **Expected Output After Optimization**

| Test | Expected Result | Reason |
|------|----------------|--------|
| Scenario 1 | All frame instructions commented out | Simple case |
| Scenario 2 | All frame instructions commented out | Return label handled |
| Scenario 3 | All frame instructions commented out | Local vars don't use BP register |
| Scenario 4 | All frame instructions commented out | Multiple local vars |
| Scenario 5 | **Unchanged** | Direct BP usage |
| Scenario 6 | **Unchanged** | BP in arithmetic |
| Scenario 7 | **Unchanged** | BP in indirect addressing |
| Scenario 8 | **Unchanged** | Nested control flow |
| Scenario 9 | **Unchanged** | Missing prologue |
| Scenario 10 | **Unchanged** | Missing epilogue |
| Scenario 11 | **Unchanged** | Partial prologue |
| Scenario 12 | **Unchanged** | Partial epilogue |
| Scenario 13 | All frame instructions commented out | Empty function |
| Scenario 14 | All frame instructions commented out | CALL doesn't use BP |
| Scenario 15 | **Unchanged** | Modifies BP register |
| Scenario 16 | All frame instructions commented out | Complex but no BP usage |

---
### **How to Test**
1. Save as `test_frame.asm`
2. Run: `./v32opt test_frame.asm -fopt_omit_frame_pointers -v`
3. Verify each function matches expected output
4. Confirm no functions are incorrectly modified
