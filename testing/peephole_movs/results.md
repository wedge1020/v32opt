Here's a **comprehensive test program** for `peephole_movs`:

```asm
; ===================================================================
; TEST: peephole_movs - All Scenarios
; Run with: ./v32opt test_movs.asm -fopt_peephole_movs -v
; ===================================================================

; ===================================================================
; ✅ PATTERN 1: Duplicate Move Elimination (Register)
; ===================================================================

; --- Basic duplicate ---
__function_test_dup_reg:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R1, R2         ; Should be removed
    MOV SP, BP
    POP BP
    RET

; --- Multiple duplicates ---
__function_test_dup_reg_multiple:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R1, R2         ; Should be removed
    MOV R3, R4
    MOV R3, R4         ; Should be removed
    MOV SP, BP
    POP BP
    RET

; --- With comments ---
__function_test_dup_reg_comments:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    ; comment
    MOV R1, R2         ; Should be removed
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ PATTERN 1 GUARD: Different Sources
; ===================================================================

; --- Different source registers ---
__function_test_dup_diff_src:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R1, R3         ; Different source - keep both
    MOV SP, BP
    POP BP
    RET

; --- Different destination ---
__function_test_dup_diff_dst:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R3, R2         ; Different destination - keep both
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ PATTERN 1: Duplicate Move Elimination (Immediate)
; ===================================================================

; --- Immediate value ---
__function_test_dup_imm:
    PUSH BP
    MOV BP, SP
    MOV R1, 42
    MOV R1, 42         ; Should be removed
    MOV SP, BP
    POP BP
    RET

; --- Negative immediate ---
__function_test_dup_neg_imm:
    PUSH BP
    MOV BP, SP
    MOV R1, -5
    MOV R1, -5         ; Should be removed
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ PATTERN 1: Duplicate Move Elimination (Indirect)
; ===================================================================

; --- Indirect address ---
__function_test_dup_indirect:
    PUSH BP
    MOV BP, SP
    MOV R1, [R2]
    MOV R1, [R2]        ; Should be removed
    MOV SP, BP
    POP BP
    RET

; --- Indirect with offset ---
__function_test_dup_indirect_offset:
    PUSH BP
    MOV BP, SP
    MOV R1, [BP+4]
    MOV R1, [BP+4]      ; Should be removed
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ PATTERN 1 GUARD: Self-Referential Load
; ===================================================================

; --- MOV r, [r]; MOV r, [r] ---
__function_test_self_ref:
    PUSH BP
    MOV BP, SP
    MOV R1, [R1]
    MOV R1, [R1]        ; Should NOT be removed (self-referential)
    MOV SP, BP
    POP BP
    RET

; --- MOV r, [r]; MOV s, [r] ---
__function_test_self_ref_diff_dst:
    PUSH BP
    MOV BP, SP
    MOV R1, [R1]
    MOV R2, [R1]        ; Different destination - should be removed
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ PATTERN 2: Mirror Move Elimination
; ===================================================================

; --- Basic mirror ---
__function_test_mirror:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R2, R1          ; Should be removed (mirror)
    MOV SP, BP
    POP BP
    RET

; --- Multiple mirrors ---
__function_test_mirror_multiple:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R2, R1          ; Should be removed
    MOV R3, R4
    MOV R4, R3          ; Should be removed
    MOV SP, BP
    POP BP
    RET

; --- Mirror with comments ---
__function_test_mirror_comments:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    ; comment
    MOV R2, R1          ; Should be removed
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ❌ PATTERN 2 GUARD: Not Mirror
; ===================================================================

; --- Different registers ---
__function_test_mirror_diff:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R3, R4          ; Not mirror - keep both
    MOV SP, BP
    POP BP
    RET

; --- Same direction ---
__function_test_mirror_same_dir:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R1, R2          ; Duplicate, not mirror - removed by Pattern 1
    MOV SP, BP
    POP BP
    RET

; --- Partial mirror ---
__function_test_mirror_partial:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R2, R3          ; Not mirror (R1≠R3) - keep both
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ COMPLEX: Mixed Patterns
; ===================================================================

; --- Duplicate + Mirror ---
__function_test_mixed:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R1, R2          ; Duplicate - remove
    MOV R3, R4
    MOV R4, R3          ; Mirror - remove
    MOV R5, 10
    MOV R5, 10          ; Duplicate - remove
    MOV SP, BP
    POP BP
    RET
```

---
### **Expected Output After Optimization**

| Test | Expected Result | Reason |
|------|----------------|--------|
| `test_dup_reg` | First MOV only | Duplicate register |
| `test_dup_reg_multiple` | Two MOVs removed | Multiple duplicates |
| `test_dup_reg_comments` | First MOV only | Comments skipped |
| `test_dup_diff_src` | **Unchanged** | Different sources |
| `test_dup_diff_dst` | **Unchanged** | Different destinations |
| `test_dup_imm` | First MOV only | Duplicate immediate |
| `test_dup_neg_imm` | First MOV only | Duplicate negative immediate |
| `test_dup_indirect` | First MOV only | Duplicate indirect |
| `test_dup_indirect_offset` | First MOV only | Duplicate with offset |
| `test_self_ref` | **Unchanged** | Self-referential load |
| `test_self_ref_diff_dst` | First MOV only | Different destination, not self-ref |
| `test_mirror` | First MOV only | Mirror move |
| `test_mirror_multiple` | Two MOVs removed | Multiple mirrors |
| `test_mirror_comments` | First MOV only | Comments skipped |
| `test_mirror_diff` | **Unchanged** | Not mirror |
| `test_mirror_same_dir` | First MOV only | Duplicate (Pattern 1) |
| `test_mirror_partial` | **Unchanged** | Not mirror |
| `test_mixed` | Three MOVs removed | Mixed patterns |

---
### **How to Test**
1. Save as `test_movs.asm`
2. Run: `./v32opt test_movs.asm -fopt_peephole_movs -v`
3. Verify each function matches expected output
4. Confirm no instructions are incorrectly modified
