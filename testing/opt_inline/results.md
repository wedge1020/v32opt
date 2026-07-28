Here's a **comprehensive test program** for the inline optimization:

```asm
; ===================================================================
; TEST: inline - All Scenarios
; Run with: ./v32opt test_inline.asm -finline -v
; For aggressive inlining: ./v32opt test_inline.asm -finline_all -v
; ===================================================================

; ===================================================================
; ✅ SCENARIO 1: Simple Leaf Function (SHOULD INLINE)
; Small function, no CALL/HLT, called once
; ===================================================================
CALL __func_simple
RET

__func_simple:
    MOV R1, 42
    IADD R2, R1
    RET

; ===================================================================
; ✅ SCENARIO 2: Function with Parameters (SHOULD INLINE)
; ===================================================================
MOV R1, 10
CALL __func_params
RET

__func_params:
    IADD R1, 5
    IMUL R1, 2
    RET

; ===================================================================
; ✅ SCENARIO 3: Function with Return Value (SHOULD INLINE)
; ===================================================================
CALL __func_return
MOV R2, R0
RET

__func_return:
    MOV R0, 100
    RET

; ===================================================================
; ❌ SCENARIO 4: Function with CALL (MUST NOT INLINE)
; Contains a CALL instruction
; ===================================================================
CALL __func_with_call
RET

__func_with_call:
    CALL __helper
    MOV R1, 42
    RET

__helper:
    RET

; ===================================================================
; ❌ SCENARIO 5: Function with HLT (MUST NOT INLINE)
; ===================================================================
CALL __func_with_hlt
RET

__func_with_hlt:
    HLT
    RET

; ===================================================================
; ❌ SCENARIO 6: Large Function (MUST NOT INLINE)
; Exceeds MAX_INLINE_SIZE (default: 10 instructions)
; ===================================================================
CALL __func_large
RET

__func_large:
    MOV R1, 1
    MOV R2, 2
    MOV R3, 3
    MOV R4, 4
    MOV R5, 5
    MOV R6, 6
    MOV R7, 7
    MOV R8, 8
    MOV R9, 9
    MOV R10, 10
    MOV R11, 11
    RET

; ===================================================================
; ❌ SCENARIO 7: Function Called Multiple Times (MUST NOT INLINE)
; Without -finline_all, only inlined once
; ===================================================================
CALL __func_multi
CALL __func_multi
RET

__func_multi:
    MOV R1, 42
    RET

; ===================================================================
; ✅ SCENARIO 8: Nested Calls (SHOULD INLINE INNER)
; Outer function not inlined, inner function inlined
; ===================================================================
CALL __func_outer
RET

__func_outer:
    CALL __func_inner
    MOV R2, 42
    RET

__func_inner:
    MOV R1, 10
    RET

; ===================================================================
; ✅ SCENARIO 9: Function with Local Variables (SHOULD INLINE)
; ===================================================================
CALL __func_locals
RET

__func_locals:
    PUSH BP
    MOV BP, SP
    MOV [BP-4], R1
    MOV R2, [BP-4]
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ SCENARIO 10: Multiple Inlinable Functions
; ===================================================================
CALL __func_a
CALL __func_b
RET

__func_a:
    MOV R1, 1
    RET

__func_b:
    MOV R2, 2
    RET

; ===================================================================
; ✅ SCENARIO 11: Function with Conditional Logic (SHOULD INLINE)
; ===================================================================
MOV R1, 1
CALL __func_conditional
RET

__func_conditional:
    JT R1, skip
    MOV R2, 10
skip:
    MOV R3, 20
    RET

; ===================================================================
; ✅ SCENARIO 12: Function with Multiple Returns (SHOULD INLINE)
; ===================================================================
CALL __func_multi_ret
RET

__func_multi_ret:
    MOV R1, 1
    JT R1, exit_early
    MOV R2, 2
exit_early:
    RET

; ===================================================================
; ✅ SCENARIO 13: Inline All Mode (-finline_all)
; Multiple calls to same function SHOULD all be inlined
; Run with: -finline_all
; ===================================================================
CALL __func_inline_all
CALL __func_inline_all
RET

__func_inline_all:
    MOV R1, 42
    RET

; ===================================================================
; ❌ SCENARIO 14: Recursive Function (MUST NOT INLINE)
; Would cause infinite inlining
; ===================================================================
CALL __func_recursive
RET

__func_recursive:
    CALL __func_recursive
    RET

; ===================================================================
; ✅ SCENARIO 15: Function with Stack Operations
; ===================================================================
CALL __func_stack
RET

__func_stack:
    PUSH R1
    MOV R2, 42
    POP R1
    RET
```

---
### **Expected Output After Optimization**

| Scenario | Expected Result | Reason |
|----------|----------------|--------|
| 1 | CALL replaced with function body | Simple leaf function |
| 2 | CALL replaced with function body | Parameters handled |
| 3 | CALL replaced with function body | Return value preserved |
| 4 | **Unchanged** | Contains CALL |
| 5 | **Unchanged** | Contains HLT |
| 6 | **Unchanged** | Too large |
| 7 | **Unchanged** (first CALL only) | Multiple calls without -finline_all |
| 8 | Outer CALL unchanged, inner CALL inlined | Nested calls |
| 9 | CALL replaced with function body | Local variables handled |
| 10 | Both CALLs replaced | Multiple inlinable functions |
| 11 | CALL replaced with function body | Conditional logic handled |
| 12 | CALL replaced with function body | Multiple returns handled |
| 13 | Both CALLs replaced | -finline_all mode |
| 14 | **Unchanged** | Recursive function |
| 15 | CALL replaced with function body | Stack operations handled |

---
### **How to Test**
1. Save as `test_inline.asm`
2. **Standard inlining**: `./v32opt test_inline.asm -fopt_inline -v`
3. **Aggressive inlining**: `./v32opt test_inline.asm -fopt_inline_all -v` (doesn't exist)
4. Verify each scenario matches expected output
5. Confirm no functions are incorrectly inlined
