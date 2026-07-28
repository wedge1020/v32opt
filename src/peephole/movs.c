#include "v32opt.h"

// ===================================================================
// PEEPHOLE: Redundant & Mirror Move Elimination
//
// Scans forward within basic blocks to remove redundant MOV instructions:
//
// Patterns handled:
//   - Duplicate moves: MOV r1, X; ... MOV r1, X → remove second MOV
//   - Mirror moves:    MOV r1, r2; ... MOV r2, r1 → remove second MOV
//
// Example:
//   Input:  MOV R1, 42
//           MOV R1, 42
//   Output: MOV R1, 42
//
//   Input:  MOV R1, R2
//           MOV R2, R1
//   Output: MOV R1, R2
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
            // 🔧 NEW: Skip SP and BP entirely (special registers)
            if ((curr->dst_op.mode == MODE_REG && (str_case_eq(curr->dst_op.reg, "SP") || str_case_eq(curr->dst_op.reg, "BP"))) ||
                (curr->src_op.mode == MODE_REG && (str_case_eq(curr->src_op.reg, "SP") || str_case_eq(curr->src_op.reg, "BP"))))
            {
                curr = curr->next;
                continue;
            }

            // 🔧 NEW: Skip MOVs with immediate operands (avoid re-exposing patterns)
            if (curr->src_op.mode == MODE_IMMEDIATE || curr->dst_op.mode == MODE_IMMEDIATE)
            {
                curr = curr->next;
                continue;
            }

            // 🔧 NEW: Skip MOVs involving memory operands (avoid memory hazards)
            if (curr->dst_op.mode == MODE_INDIRECT || curr->src_op.mode == MODE_INDIRECT)
            {
                curr = curr->next;
                continue;
            }

            // Identify registers for hazard checking
            const char *dst_reg = curr->dst_op.reg;
            const char *src_reg = curr->src_op.reg;

            AsmNode *scan = curr->next;
            while (scan)
            {
                // Skip inline comments and blank lines
                if (scan->type == OP_OTHER)
                {
                    scan = scan->next;
                    continue;
                }

                // Stop on control flow boundaries, jumps, calls, or labels
                if (is_control_flow_boundary(scan) ||
                    str_case_eq(scan->mnemonic, "CALL") ||
                    str_case_eq(scan->mnemonic, "JT")   ||
                    str_case_eq(scan->mnemonic, "JF")   ||
                    scan->type == OP_LABEL)
                {
                    break;
                }

                // 🔧 NEW: Stop if either register is clobbered by scan
                if (modifies_register(scan, dst_reg) || modifies_register(scan, src_reg))
                {
                    break;
                }

                // Check if scan is a MOV instruction we can optimize
                if (scan->type == OP_MOV)
                {
                    // 🔧 NEW: Skip MOVs with immediate operands in scan
                    if (scan->src_op.mode == MODE_IMMEDIATE || scan->dst_op.mode == MODE_IMMEDIATE)
                    {
                        scan = scan->next;
                        continue;
                    }

                    // 🔧 NEW: Skip MOVs involving memory operands in scan
                    if (scan->dst_op.mode == MODE_INDIRECT || scan->src_op.mode == MODE_INDIRECT)
                    {
                        scan = scan->next;
                        continue;
                    }

                    // --- Duplicate Move Elimination ---
                    // MOV r1, r2; ... MOV r1, r2 → second MOV is redundant
                    if (operands_equal(&curr->dst_op, &scan->dst_op) &&
                        operands_equal(&curr->src_op, &scan->src_op))
                    {
                        AsmNode *next_scan = scan->next;
                        AsmNode *nodes[] = {scan};
                        remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_MOVS);
                        optimizations++;
                        scan = next_scan;
                        continue;
                    }

                    // --- Mirror Move Elimination ---
                    // MOV r1, r2; ... MOV r2, r1 → second MOV is redundant
                    if (curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_REG &&
                        scan->dst_op.mode == MODE_REG && scan->src_op.mode == MODE_REG)
                    {
                        if (str_case_eq(curr->dst_op.reg, scan->src_op.reg) &&
                            str_case_eq(curr->src_op.reg, scan->dst_op.reg))
                        {
                            AsmNode *next_scan = scan->next;
                            AsmNode *nodes[] = {scan};
                            remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_MOVS);
                            optimizations++;
                            scan = next_scan;
                            continue;
                        }
                    }
                }

                scan = scan->next;
            }
        }
        curr = curr->next;
    }

    return optimizations;
}
