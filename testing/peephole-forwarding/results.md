Here's a **rigorous test program** for `peephole_forwarding` covering all rules and edge cases:

```asm
; ===================================================================
; TEST: peephole_forwarding - All Scenarios
; Run with: ./v32opt test_forwarding.asm -fopt_peephole_forwarding -v
; ===================================================================

; ===================================================================
; ✅ RULE 1: Store-to-Load Forwarding
; ===================================================================

; --- Basic store-to-load ---
__function_test_store_load:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2
    MOV R3, [R1]        ; Should become: MOV R3, R2
    MOV SP, BP
    POP BP
    RET

; --- With offset ---
__function_test_store_load_offset:
    PUSH BP
    MOV BP, SP
    MOV [BP+4], R2
    MOV R3, [BP+4]     ; Should become: MOV R3, R2
    MOV SP, BP
    POP BP
    RET

; --- Memory overwritten (SHOULD NOT forward) ---
__function_test_store_load_overwrite:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2
    MOV [R1], R3       ; Overwrites memory
    MOV R4, [R1]       ; Should NOT forward (memory changed)
    MOV SP, BP
    POP BP
    RET

; --- Across control flow (SHOULD NOT forward) ---
__function_test_store_load_cfb:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2
    JMP skip
    MOV R3, [R1]       ; Should NOT forward (across JMP)
skip:
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ RULE 2: Copy Propagation (Register)
; ===================================================================

; --- Basic register propagation ---
__function_test_copy_prop_reg:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R3, R1          ; Should become: MOV R3, R2
    MOV SP, BP
    POP BP
    RET

; --- Through comments ---
__function_test_copy_prop_comments:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    ; comment
    MOV R3, R1          ; Should become: MOV R3, R2
    MOV SP, BP
    POP BP
    RET

; --- Source register modified (SHOULD NOT propagate) ---
__function_test_copy_prop_modified:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R1, R3         ; Overwrites R1
    MOV R4, R1         ; Should NOT propagate (R1 changed)
    MOV SP, BP
    POP BP
    RET

; --- Multiple propagations ---
__function_test_copy_prop_multiple:
    PUSH BP
    MOV BP, SP
    MOV R1, R2
    MOV R3, R1         ; Should become: MOV R3, R2
    MOV R4, R3         ; Should become: MOV R4, R2 (via R3→R2)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ RULE 2: Copy Propagation (Immediate)
; ===================================================================

; --- Basic immediate propagation ---
__function_test_copy_prop_imm:
    PUSH BP
    MOV BP, SP
    MOV R1, 42
    MOV R2, R1         ; Should become: MOV R2, 42
    MOV SP, BP
    POP BP
    RET

; --- Immediate to ALU ---
__function_test_copy_prop_alu:
    PUSH BP
    MOV BP, SP
    MOV R1, 10
    IADD R2, R1        ; Should become: IADD R2, 10
    ISUB R3, R1        ; Should become: ISUB R3, 10
    IMUL R4, R1        ; Should become: IMUL R4, 10
    MOV SP, BP
    POP BP
    RET

; --- Immediate to label (SHOULD propagate) ---
__function_test_copy_prop_label:
    PUSH BP
    MOV BP, SP
    MOV R1, my_label
    JMP R1             ; Should become: JMP my_label
    MOV SP, BP
    POP BP
    RET
my_label:
    HLT

; ===================================================================
; ❌ GUARDS: Should NOT Propagate
; ===================================================================

; --- Into POW (SHOULD NOT propagate) ---
__function_test_guard_pow:
    PUSH BP
    MOV BP, SP
    MOV R1, 5
    POW R2, R1         ; Should NOT propagate immediate into POW
    MOV SP, BP
    POP BP
    RET

; --- Into ATAN2 (SHOULD NOT propagate) ---
__function_test_guard_atan2:
    PUSH BP
    MOV BP, SP
    MOV R1, 5
    ATAN2 R2, R1       ; Should NOT propagate immediate into ATAN2
    MOV SP, BP
    POP BP
    RET

; --- Into JT with numeric immediate (SHOULD NOT propagate) ---
__function_test_guard_jt:
    PUSH BP
    MOV BP, SP
    MOV R1, 0
    JT R2, R1           ; Should NOT propagate 0 into JT target
    MOV SP, BP
    POP BP
    RET

; --- Into JF with numeric immediate (SHOULD NOT propagate) ---
__function_test_guard_jf:
    PUSH BP
    MOV BP, SP
    MOV R1, 0
    JF R2, R1           ; Should NOT propagate 0 into JF target
    MOV SP, BP
    POP BP
    RET

; --- SP/BP protection ---
__function_test_guard_sp_bp:
    PUSH BP
    MOV BP, SP
    MOV SP, R1
    MOV R2, SP          ; Should NOT propagate (SP protected)
    MOV BP, R3
    MOV R4, BP          ; Should NOT propagate (BP protected)
    MOV SP, BP
    POP BP
    RET

; ===================================================================
; ✅ COMPLEX: Combined Scenarios
; ===================================================================

; --- Store-to-load + copy propagation ---
__function_test_combined:
    PUSH BP
    MOV BP, SP
    MOV [R1], R2
    MOV R3, [R1]        ; Rule 1: → MOV R3, R2
    MOV R4, R3          ; Rule 2: → MOV R4, R2 (propagates R3→R2)
    MOV SP, BP
    POP BP
    RET
```

---
### **Expected Output After Optimization**

| Test | Expected Result | Reason |
|------|----------------|--------|
| `test_store_load` | `MOV R3, R2` | Store-to-load forwarding |
| `test_store_load_offset` | `MOV R3, R2` | Offset forwarding |
| `test_store_load_overwrite` | **Unchanged** | Memory overwritten |
| `test_store_load_cfb` | **Unchanged** | Across control flow |
| `test_copy_prop_reg` | `MOV R3, R2` | Register propagation |
| `test_copy_prop_comments` | `MOV R3, R2` | Through comments |
| `test_copy_prop_modified` | **Unchanged** | Source modified |
| `test_copy_prop_multiple` | `MOV R3, R2` + `MOV R4, R2` | Cascading propagation |
| `test_copy_prop_imm` | `MOV R2, 42` | Immediate propagation |
| `test_copy_prop_alu` | `IADD R2, 10` + `ISUB R3, 10` + `IMUL R4, 10` | ALU propagation |
| `test_copy_prop_label` | `JMP my_label` | Label propagation |
| `test_guard_pow` | **Unchanged** | POW guard |
| `test_guard_atan2` | **Unchanged** | ATAN2 guard |
| `test_guard_jt` | **Unchanged** | JT numeric immediate guard |
| `test_guard_jf` | **Unchanged** | JF numeric immediate guard |
| `test_guard_sp_bp` | **Unchanged** | SP/BP protection |
| `test_combined` | `MOV R3, R2` + `MOV R4, R2` | Combined rules |

---
### **How to Test**
1. Save as `test_forwarding.asm`
2. Run: `./v32opt test_forwarding.asm -fopt_peephole_forwarding -v`
3. Verify each function matches expected output
4. Confirm no instructions are incorrectly modified
