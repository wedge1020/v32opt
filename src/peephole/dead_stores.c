// ===================================================================
// PEEPHOLE: Dead Store Elimination
// Removes MOV instructions that store values which are never read:
//   - MOV R1, val; MOV R1, val2 → remove first (overwritten before use)
//   - MOV [mem], R1; MOV [mem], R2 → remove first (overwritten before use)
//   - MOV R1, val; ... (no reads of R1) ... MOV R1, val2 → remove first
//
// Examples:
//   MOV R1, 10   ->  (removed if R1 is overwritten before being read)
//   MOV R1, 20
//
//   MOV [R0], R1 ->  (removed if [R0] is overwritten before being read)
//   MOV [R0], R2
// ===================================================================
#include "v32opt.h"

int peephole_dead_stores(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr != NULL)
    {
        AsmNode *next = curr->next;

        // ----------------------------------------------------------
        // PATTERN 1: Consecutive Stores to Same Register
        // MOV R1, 10; MOV R1, 20 → remove first MOV
        // Only if the first value is not read between the two MOVs
        // ----------------------------------------------------------
        if (curr->type == OP_MOV && curr->dst_op.mode == MODE_REG)
        {
            char *target_reg = curr->dst_op.reg;

            // Skip special registers that shouldn't be optimized
            if (str_case_eq(target_reg, "SP") || str_case_eq(target_reg, "BP"))
            {
                curr = next;
                continue;
            }

            // Look ahead for another MOV to the same register
            AsmNode *scan = curr->next;
            bool found_dead_store = false;

            while (scan && !is_control_flow_boundary(scan))
            {
                // Skip OP_OTHER nodes (comments, blanks)
                if (scan->type == OP_OTHER)
                {
                    scan = scan->next;
                    continue;
                }

                // If we find a read of target_reg before the next store, stop
                if (scan->type != OP_MOV)
                {
                    // Check if this instruction reads target_reg
                    if ((scan->has_src && scan->src_op.mode == MODE_REG &&
                         str_case_eq(scan->src_op.reg, target_reg)) ||
                        (scan->has_dst && scan->dst_op.mode == MODE_REG &&
                         str_case_eq(scan->dst_op.reg, target_reg)))
                    {
                        // If it's not a MOV, it might be reading the register
                        // But for JMP, the dst_op is the target, not a read
                        if (scan->type != OP_JMP && scan->type != OP_JT && scan->type != OP_JF)
                        {
                            break;
                        }
                    }
                }

                // Found another MOV to the same register
                if (scan->type == OP_MOV && scan->dst_op.mode == MODE_REG &&
                    str_case_eq(scan->dst_op.reg, target_reg))
                {
                    found_dead_store = true;
                    break;
                }

                // Stop at control flow boundaries
                if (is_control_flow_boundary(scan))
                {
                    break;
                }

                scan = scan->next;
            }

            if (found_dead_store)
            {
                AsmNode *nodes[] = {curr};
                remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_DEAD_STORES);
                optimizations++;
                continue;
            }
        }

        // ----------------------------------------------------------
        // PATTERN 2: Consecutive Stores to Same Memory Location
        // MOV [R0], R1; MOV [R0], R2 → remove first MOV
        // ----------------------------------------------------------
        if (curr->type == OP_MOV && curr->dst_op.mode == MODE_INDIRECT)
        {
            char *mem_reg = curr->dst_op.reg;
            int mem_off = curr->dst_op.offset;

            // Look ahead for another store to the same memory location
            AsmNode *scan = curr->next;
            bool found_dead_store = false;

            while (scan && !is_control_flow_boundary(scan))
            {
                // Skip OP_OTHER nodes
                if (scan->type == OP_OTHER)
                {
                    scan = scan->next;
                    continue;
                }

                // If we find a load from this memory location, stop
                if (scan->type == OP_MOV && scan->src_op.mode == MODE_INDIRECT &&
                    str_case_eq(scan->src_op.reg, mem_reg) && scan->src_op.offset == mem_off)
                {
                    break;
                }

                // Found another store to the same memory location
                if (scan->type == OP_MOV && scan->dst_op.mode == MODE_INDIRECT &&
                    str_case_eq(scan->dst_op.reg, mem_reg) && scan->dst_op.offset == mem_off)
                {
                    found_dead_store = true;
                    break;
                }

                // Stop if memory base register is modified
                if (modifies_register(scan, mem_reg))
                {
                    break;
                }

                // Stop at control flow boundaries
                if (is_control_flow_boundary(scan))
                {
                    break;
                }

                scan = scan->next;
            }

            if (found_dead_store)
            {
                AsmNode *nodes[] = {curr};
                remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_DEAD_STORES);
                optimizations++;
                continue;
            }
        }

        curr = next;
    }

    return optimizations;
}
