#include "v32opt.h"

////////////////////////////////////////////////////////////////////////////////////////
//
// -------------------------------------------------------------------
// OPTIMIZATION CATEGORY: Peephole Optimizations
// Small-window (1-3 instruction) local transformations that improve
// code without global analysis.
// -------------------------------------------------------------------
//
// Note: On Vircon32, ALL instructions are 1 cycle, so many transformations
// are cost-neutral. We keep them for code clarity, size reduction, or
// idiomatic style.
//
// peephole_pairs()          - adjacent instruction pair elimination (DEBUG)
// peephole_algebra()        - algebraic simplifications (DEBUG)
// peephole_forwarding()     - store-to-load forwarding (DEBUG)
// peephole_jumps()          - redundant jump elimination (DEBUG, broken)
// peephole_movs()           - redundant MOV elimination (DEBUG)
// peephole_immediates()     - combine immediates (DEBUG)
// peephole_reduce()         - strength reduction (cost-neutral on Vircon32)
// peephole_shifts()         - shift optimizations
// peephole_dead_stores()    - dead store elimination
// peephole_loads()          - redundant load elimination (DEBUG)
// peephole_immediate_prop() - immediate propagation (DEBUG)
// peephole_jmp_chain()      - jump chain elimination
//
////////////////////////////////////////////////////////////////////////////////////////

// ===================================================================
// PEEPHOLE: Redundant & Mirror Move Elimination
// Scans forward within basic blocks to remove redundant MOV instructions:
//   - Duplicate moves: MOV r1, X;  ... MOV r1, X  → remove second MOV
//   - Mirror moves:    MOV r1, r2; ... MOV r2, r1 → remove second MOV
// ===================================================================
int peephole_movs(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        if (curr->type == OP_MOV)
        {
            // --- Self-Referential Load Check ---
            // If MOV loads from [r1] into r1, r1 is clobbered with the loaded
            // value. Subsequent moves cannot treat r1 as the same address pointer.
            bool self_referential_load =
                (curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_INDIRECT) &&
                str_case_eq(curr->dst_op.reg, curr->src_op.reg);

            // Identify registers and memory usage for hazard checking
            const char *dst_reg = (curr->dst_op.mode == MODE_REG || curr->dst_op.mode == MODE_INDIRECT) ? curr->dst_op.reg : NULL;
            const char *src_reg = (curr->src_op.mode == MODE_REG || curr->src_op.mode == MODE_INDIRECT) ? curr->src_op.reg : NULL;
            bool touches_memory = (curr->dst_op.mode == MODE_INDIRECT || curr->src_op.mode == MODE_INDIRECT);

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

                // If either instruction accesses memory, stop on any memory write or modification
                if (touches_memory)
                {
                    if (scan->type == OP_PUSH || scan->type == OP_POP ||
                        scan->type == OP_MOVS || scan->type == OP_SETS ||
                        (scan->has_dst && scan->dst_op.mode == MODE_INDIRECT))
                    {
                        break;
                    }
                }

                // Check if scan is a MOV instruction we can optimize
                if (scan->type == OP_MOV)
                {
                    // --- Duplicate Move Elimination ---
                    // MOV r1, X; ... MOV r1, X → second MOV is redundant
                    if (!self_referential_load &&
                        operands_equal(&curr->dst_op, &scan->dst_op) &&
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

                // Stop scanning if any register used by curr is modified by scan
                if ((dst_reg && modifies_register(scan, dst_reg)) ||
                    (src_reg && modifies_register(scan, src_reg)))
                {
                    break;
                }

                scan = scan->next;
            }
        }
        curr = curr->next;
    }

    return optimizations;
}
