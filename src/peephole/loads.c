#include "v32opt.h"

// ===================================================================
// PEEPHOLE: Redundant Load Elimination (Vircon32-Optimized)
// //
// Removes redundant loads from the same memory location:
//   - MOV R1, [R2+X]; MOV R3, [R2+X] → MOV R1, [R2+X]; MOV R3, R1 (saves 1 word)
//   - MOV R1, [R2+X]; FADD R3, [R2+X] → MOV R1, [R2+X]; FADD R3, R1 (floating-point)
// //
// Guards:
//   - Different offsets: MOV R1, [R2+4]; MOV R3, [R2+8] → KEEP
//   - Base register modified: MOV R1, [R2]; IADD R2, 4; MOV R3, [R2] → KEEP
//   - Memory store in between: MOV R1, [R2]; MOV [R3], R4; MOV R5, [R2] → KEEP
// ===================================================================
int peephole_loads(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr) {
        if (curr->type == OP_MOV && curr->dst_op.mode == MODE_REG &&
            curr->src_op.mode == MODE_INDIRECT) {
            
            char *base_reg = curr->src_op.reg;
            int offset = curr->src_op.offset;
            char *dst_reg = curr->dst_op.reg;

            // Skip if destination is SP/BP (special registers)
            if (str_case_eq(dst_reg, "SP") || str_case_eq(dst_reg, "BP")) {
                curr = curr->next;
                continue;
            }

            AsmNode *scan = curr->next;
            while (scan && !is_control_flow_boundary(scan)) {
                if (scan->type == OP_OTHER && (scan->raw[0] == '\0' || scan->raw[0] == ';')) {
                    scan = scan->next;
                    continue;
                }

                // --- GUARD: Base register modified ---
                if (modifies_register(scan, base_reg)) break;

                // --- GUARD: Destination register modified ---
                if (modifies_register(scan, dst_reg)) break;

                // --- GUARD: Memory store to any location (could alias) ---
                if (scan->type == OP_MOV && scan->dst_op.mode == MODE_INDIRECT) break;

                // --- GUARD: Function call (clobbers memory) ---
                if (scan->type == OP_CALL) break;

                // --- MATCH: Same memory load ---
                if (scan->type == OP_MOV && scan->dst_op.mode == MODE_REG &&
                    scan->src_op.mode == MODE_INDIRECT &&
                    str_case_eq(scan->src_op.reg, base_reg) &&
                    scan->src_op.offset == offset) {
                    
                    // --- GUARD: Self-referential (MOV R1, [R1]; MOV R2, [R1]) ---
                    if (curr->src_op.mode == MODE_INDIRECT &&
                        str_case_eq(curr->src_op.reg, curr->dst_op.reg)) {
                        break;
                    }

                    // --- GUARD: Destination is SP/BP ---
                    if (str_case_eq(scan->dst_op.reg, "SP") || str_case_eq(scan->dst_op.reg, "BP")) {
                        scan = scan->next;
                        continue;
                    }

                    // --- APPLY: Replace second load with register move ---
                    insert_debug_comment(scan->prev, OPT_PEEPHOLE_LOADS, scan->raw);
                    scan->src_op.mode = MODE_REG;
                    scan->src_op.reg = strdup(dst_reg);
                    snprintf(scan->src_op.raw, sizeof(scan->src_op.raw), "%s", dst_reg);
                    snprintf(scan->raw, sizeof(scan->raw), "    MOV %s, %s",
                             scan->dst_op.raw, scan->src_op.raw);
                    optimizations++;
                    // Continue scanning for more redundant loads
                    scan = scan->next;
                } else {
                    scan = scan->next;
                }
            }
        }
        curr = curr->next;
    }
    return optimizations;
}
