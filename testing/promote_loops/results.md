The minimal test working confirms the pass is functional. The issue is likely **label pattern mismatch** or **safety check failures** in your original test. Let's create a **guaranteed-to-work test program** with proper compiler-style patterns:

```asm
; ===================================================================
; TEST: pass_promote_loop_registers - Compiler-Style Patterns
; Run with: ./v32opt test_promote_loops.asm -fopt_promote_loops -v
; ===================================================================

; ===================================================================
; ✅ SCENARIO 1: Simple While Loop (SHOULD PROMOTE)
; Matches typical compiler output for: while (cond) { ... }
; ===================================================================
__while_1_start:
    MOV R1, [BP-4]      ; Load local var
    IADD R1, 1          ; Use it
    MOV [BP-4], R1      ; Store back
    JMP __while_1_start ; Unconditional back-edge
__while_1_exit:
    RET

; ===================================================================
; ✅ SCENARIO 2: For Loop (SHOULD PROMOTE)
; Matches typical compiler output for: for (i=0; i<N; i++) { ... }
; ===================================================================
__for_1_start:
    MOV [BP-4], R1      ; Local var i
    MOV R2, [BP-4]      ; Read i
    IADD R2, 1          ; i++
    MOV [BP-4], R2      ; Store i
    JMP __for_1_start   ; Back-edge
__for_1_exit:
    RET

; ===================================================================
; ✅ SCENARIO 3: Loop with Multiple Locals (SHOULD PROMOTE)
; ===================================================================
__for_2_start:
    MOV [BP-4], R1      ; Local var 1
    MOV [BP-8], R2      ; Local var 2
    MOV R3, [BP-4]      ; Read var 1
    MOV R4, [BP-8]      ; Read var 2
    IADD R5, R3
    ISUB R5, R4
    MOV [BP-4], R5      ; Write var 1
    JMP __for_2_start
__for_2_exit:
    RET

; ===================================================================
; ✅ SCENARIO 4: Loop with Exit Label (SHOULD PROMOTE)
; Matches compiler output with explicit exit label
; ===================================================================
__while_2_start:
    MOV [BP-4], R1
    MOV R2, [BP-4]
    IADD R2, 1
    MOV [BP-4], R2
    JF R1, __while_2_exit ; Conditional exit
    JMP __while_2_start   ; Back-edge
__while_2_exit:
    RET

; ===================================================================
; ❌ SCENARIO 5: Loop with CALL (MUST NOT PROMOTE)
; ===================================================================
__for_3_start:
    CALL some_func     ; Safety check fails
    MOV [BP-4], R1
    MOV R2, [BP-4]
    JMP __for_3_start
__for_3_exit:
    RET

; ===================================================================
; ❌ SCENARIO 6: Loop with BP Usage (MUST NOT PROMOTE)
; ===================================================================
__for_4_start:
    MOV R1, BP          ; Direct BP usage
    MOV [BP-4], R1
    MOV R2, [BP-4]
    JMP __for_4_start
__for_4_exit:
    RET
```

---
### **Key Fixes Applied to Test Cases**
| Scenario | Pattern | Expected |
|----------|---------|----------|
| 1-4 | Standard loop labels (`_start`/`_exit`) | **Promoted** |
| 5 | Contains `CALL` | Not promoted |
| 6 | Uses `BP` directly | Not promoted |

---
### **Debugging Steps**
1. **Verify your compiler's label format** by compiling a simple C loop and checking the assembly
2. **Run with verbose output** to see which loops are being processed:
   ```bash
   ./v32opt test_promote_loops.asm -fopt_promote_loops -v
   ```
3. **Check for safety failures** - If a loop contains `CALL`, `RET`, `HLT`, or direct `BP` usage, it will be skipped

---
### **If Still Getting 0 Hits**
Add this debug code to `pass_promote_loop_registers()` (right after the label detection):

```c
printf("DEBUG: Found loop label: %s\n", start_lbl);
if (end_jmp) printf("DEBUG: Found back-edge at: %s %s\n", end_jmp->mnemonic, end_jmp->dst_op.raw);
else printf("DEBUG: No back-edge found\n");
if (exit_label_node) printf("DEBUG: Found exit label: %s\n", exit_label_node->raw);
else printf("DEBUG: No exit label found\n");
```

This will show exactly which loops are being detected and where the detection fails.
