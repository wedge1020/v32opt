#include "v32opt.h"

// ===================================================================
// PEEPHOLE: Dead Store Elimination
// Removes MOV instructions that store values which are never read:
//   - MOV R1, val; MOV R1, val2 → remove first (overwritten before use)
//   - MOV [mem], R1; MOV [mem], R2 → remove first (overwritten before use)
//   - MOV R1, val; ... (no reads of R1) ... MOV R1, val2 → remove first
//
// On Vircon32: All instructions are 1 cycle, but immediates and dereferencing
// add extra words to the instruction payload. This optimization reduces
// code size by eliminating redundant stores.
//
// Returns: Number of optimizations applied
// ===================================================================
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
                if (is_register_read(scan, target_reg))
                {
                    break;
                }

                // Found another MOV to the same register
                if (scan->type == OP_MOV && scan->dst_op.mode == MODE_REG &&
                    str_case_eq(scan->dst_op.reg, target_reg))
                {
                    found_dead_store = true;
                    break;
                }

                scan = scan->next;
            }

            if (found_dead_store)
            {
                insert_debug_comment(curr->prev, OPT_PEEPHOLE_DEAD_STORES, curr->raw);
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

                scan = scan->next;
            }

            if (found_dead_store)
            {
                insert_debug_comment(curr->prev, OPT_PEEPHOLE_DEAD_STORES, curr->raw);
                AsmNode *nodes[] = {curr};
                remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_DEAD_STORES);
                optimizations++;
                continue;
            }
        }

        // ----------------------------------------------------------
        // PATTERN 3: Store followed by store to same location with no reads
        // Extended version: handles cases where there are non-reading
        // instructions between the stores
        // ----------------------------------------------------------
        if (curr->type == OP_MOV)
        {
            bool is_reg_store = (curr->dst_op.mode == MODE_REG);
            bool is_mem_store = (curr->dst_op.mode == MODE_INDIRECT);

            if (is_reg_store || is_mem_store)
            {
                char *target = is_reg_store ? curr->dst_op.reg : curr->dst_op.reg;
                int offset = is_mem_store ? curr->dst_op.offset : 0;
                //bool is_memory = is_mem_store;

                // Skip special registers
                if (is_reg_store && (str_case_eq(target, "SP") || str_case_eq(target, "BP")))
                {
                    curr = next;
                    continue;
                }

                // Look ahead more aggressively for overwriting store
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

                    // Check if this instruction reads our stored value
                    bool reads_target = false;

                    if (is_reg_store)
                    {
                        reads_target = is_register_read(scan, target);
                    }
                    else if (is_mem_store)
                    {
                        // Check for loads from this memory location
                        if (scan->type == OP_MOV && scan->src_op.mode == MODE_INDIRECT &&
                            str_case_eq(scan->src_op.reg, target) && scan->src_op.offset == offset)
                        {
                            reads_target = true;
                        }
                        // Check if base register is modified (would change the address)
                        else if (modifies_register(scan, target))
                        {
                            break;
                        }
                    }

                    if (reads_target)
                    {
                        break;
                    }

                    // Check if this is an overwriting store
                    bool is_overwriting_store = false;

                    if (is_reg_store && scan->type == OP_MOV &&
                        scan->dst_op.mode == MODE_REG &&
                        str_case_eq(scan->dst_op.reg, target))
                    {
                        is_overwriting_store = true;
                    }
                    else if (is_mem_store && scan->type == OP_MOV &&
                             scan->dst_op.mode == MODE_INDIRECT &&
                             str_case_eq(scan->dst_op.reg, target) &&
                             scan->dst_op.offset == offset)
                    {
                        is_overwriting_store = true;
                    }

                    if (is_overwriting_store)
                    {
                        found_dead_store = true;
                        break;
                    }

                    scan = scan->next;
                }

                if (found_dead_store)
                {
                    insert_debug_comment(curr->prev, OPT_PEEPHOLE_DEAD_STORES, curr->raw);
                    AsmNode *nodes[] = {curr};
                    remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_DEAD_STORES);
                    optimizations++;
                    continue;
                }
            }
        }

        curr = next;
    }

    return optimizations;
}
