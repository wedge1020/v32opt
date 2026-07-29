#include "v32opt.h"

// ===================================================================
// PEEPHOLE: Redundant & Mirror Move Elimination
//
// Scans forward within basic blocks to remove redundant MOV instructions:
//
// Patterns handled:
//   - Duplicate moves: MOV r1, X; ... MOV r1, X → remove second MOV (only if dst not modified)
//   - Mirror moves:    MOV r1, r2; ... MOV r2, r1 → remove second MOV (only if neither modified)
//
// Returns: Number of optimizations applied
// ===================================================================
int peephole_movs(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        if (curr->type == OP_MOV)
        {
            // Skip SP and BP entirely (special registers)
            if ((curr->dst_op.mode == MODE_REG && (str_case_eq(curr->dst_op.reg, "SP") || str_case_eq(curr->dst_op.reg, "BP"))) ||
                (curr->src_op.mode == MODE_REG && (str_case_eq(curr->src_op.reg, "SP") || str_case_eq(curr->src_op.reg, "BP"))))
            {
                curr = curr->next;
                continue;
            }

            AsmNode *scan = curr->next;
            while (scan && !is_control_flow_boundary(scan))
            {
                AsmNode *next_scan = scan->next;

                if (scan->type != OP_MOV) {
                    scan = next_scan;
                    continue;
                }

                // Skip SP/BP in the scanned instruction
                if ((scan->dst_op.mode == MODE_REG && (str_case_eq(scan->dst_op.reg, "SP") || str_case_eq(scan->dst_op.reg, "BP"))) ||
                    (scan->src_op.mode == MODE_REG && (str_case_eq(scan->src_op.reg, "SP") || str_case_eq(scan->src_op.reg, "BP"))))
                {
                    scan = next_scan;
                    continue;
                }

                bool removed = false;

                // --- Duplicate Move Elimination ---
                // MOV dst, src; ... MOV dst, src → second MOV is redundant
                // ONLY if destination register was NOT modified between them
                if (curr->dst_op.mode == MODE_REG &&
                    scan->dst_op.mode == MODE_REG &&
                    operands_equal(&curr->dst_op, &scan->dst_op) &&
                    operands_equal(&curr->src_op, &scan->src_op))
                {
                    // Check if destination register was modified between curr and scan
                    bool dst_modified = false;
                    AsmNode *check = curr->next;
                    while (check != scan && check != NULL)
                    {
                        if (modifies_register(check, curr->dst_op.reg))
                        {
                            dst_modified = true;
                            break;
                        }
                        check = check->next;
                    }

                    if (!dst_modified)
                    {
                        AsmNode *nodes[] = {scan};
                        remove_with_debug(&scan->prev->next, nodes, 1, OPT_PEEPHOLE_MOVS);
                        optimizations++;
                        removed = true;
                    }
                }
                // --- Mirror Move Elimination (register-to-register only) ---
                // MOV r1, r2; ... MOV r2, r1 → second MOV is redundant
                // ONLY if neither r1 nor r2 were modified between them
                else if (curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_REG &&
                         scan->dst_op.mode == MODE_REG && scan->src_op.mode == MODE_REG)
                {
                    if (str_case_eq(curr->dst_op.reg, scan->src_op.reg) &&
                        str_case_eq(curr->src_op.reg, scan->dst_op.reg))
                    {
                        // Check if either register was modified between curr and scan
                        bool reg1_modified = false;
                        bool reg2_modified = false;
                        AsmNode *check = curr->next;
                        while (check != scan && check != NULL)
                        {
                            if (modifies_register(check, curr->dst_op.reg)) reg1_modified = true;
                            if (modifies_register(check, curr->src_op.reg)) reg2_modified = true;
                            if (reg1_modified && reg2_modified) break;
                            check = check->next;
                        }

                        if (!reg1_modified && !reg2_modified)
                        {
                            AsmNode *nodes[] = {scan};
                            remove_with_debug(&scan->prev->next, nodes, 1, OPT_PEEPHOLE_MOVS);
                            optimizations++;
                            removed = true;
                        }
                    }
                }

                if (removed) {
                    scan = next_scan;
                } else {
                    scan = next_scan;
                }
            }
        }
        curr = curr->next;
    }

    return optimizations;
}
