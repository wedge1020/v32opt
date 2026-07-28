// ===================================================================
// PEEPHOLE: Redundant Load Elimination
// Eliminates redundant load instructions:
//   - MOV R1, [mem]; ... (no stores to mem) ... MOV R2, [mem] → replace second with MOV R2, R1
//   - MOV R1, [mem]; MOV R2, R1 → replace second with MOV R2, [mem] (if R1 not modified)
//   - Removes loads from memory that are immediately overwritten
//
// Examples:
//   MOV R1, [R0]  ->  (kept)
//   MOV R2, [R0]  ->  MOV R2, R1  (if [R0] unchanged between loads)
// ===================================================================
#include "v32opt.h"

int peephole_loads(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr != NULL)
    {
        AsmNode *next = curr->next;

        // ----------------------------------------------------------
        // PATTERN 1: Redundant Load from Same Memory Location
        // MOV R1, [mem]; ... MOV R2, [mem] → MOV R2, R1
        // Only if [mem] is not modified between the two loads
        // ----------------------------------------------------------
        if (curr->type == OP_MOV && curr->dst_op.mode == MODE_REG &&
            curr->src_op.mode == MODE_INDIRECT)
        {
            char *loaded_reg = curr->dst_op.reg;
            char *mem_reg = curr->src_op.reg;
            int mem_off = curr->src_op.offset;

            // Don't optimize if loading to SP or BP
            if (str_case_eq(loaded_reg, "SP") || str_case_eq(loaded_reg, "BP"))
            {
                curr = next;
                continue;
            }

            // Scan forward for another load from the same memory location
            AsmNode *scan = curr->next;
            while (scan && !is_control_flow_boundary(scan))
            {
                // Skip OP_OTHER nodes
                if (scan->type == OP_OTHER)
                {
                    scan = scan->next;
                    continue;
                }

                // Stop if memory base register is modified
                if (modifies_register(scan, mem_reg))
                {
                    break;
                }

                // Stop if there's a store to this memory location
                if (scan->type == OP_MOV && scan->dst_op.mode == MODE_INDIRECT &&
                    str_case_eq(scan->dst_op.reg, mem_reg) && scan->dst_op.offset == mem_off)
                {
                    break;
                }

                // Found another load from the same memory location
                if (scan->type == OP_MOV && scan->dst_op.mode == MODE_REG &&
                    scan->src_op.mode == MODE_INDIRECT &&
                    str_case_eq(scan->src_op.reg, mem_reg) && scan->src_op.offset == mem_off)
                {
                    // Check that loaded_reg hasn't been modified
                    bool reg_modified = false;
                    AsmNode *check = curr->next;
                    while (check != scan)
                    {
                        if (check->type != OP_OTHER && modifies_register(check, loaded_reg))
                        {
                            reg_modified = true;
                            break;
                        }
                        check = check->next;
                    }

                    if (!reg_modified)
                    {
                        // 🔧 NEW GUARD: Skip if replacing with itself (no-op)
                        if (str_case_eq(scan->dst_op.reg, loaded_reg)) {
                            scan = scan->next;
                            continue;
                        }

                        insert_debug_comment(scan->prev, OPT_PEEPHOLE_LOADS, scan->raw);
                        // Replace MOV R2, [mem] with MOV R2, R1
                        scan->src_op = curr->dst_op;
                        scan->src_op.mode = MODE_REG;
                        snprintf(scan->raw, sizeof(scan->raw), "    MOV %s, %s",
                                 scan->dst_op.raw, scan->src_op.raw);
                        optimizations++;
                        break;
                    }
                }

                // Stop at control flow boundaries
                if (is_control_flow_boundary(scan))
                {
                    break;
                }

                scan = scan->next;
            }
        }

        // ----------------------------------------------------------
        // PATTERN 2: Load Immediately Overwritten
        // MOV R1, [mem]; MOV R1, val → remove first MOV
        // The load is dead because R1 is overwritten before use
        // ----------------------------------------------------------
        if (curr->type == OP_MOV && curr->dst_op.mode == MODE_REG &&
            curr->src_op.mode == MODE_INDIRECT)
        {
            char *target_reg = curr->dst_op.reg;

            // Skip special registers
            if (str_case_eq(target_reg, "SP") || str_case_eq(target_reg, "BP"))
            {
                curr = next;
                continue;
            }

            // Look at the very next non-OP_OTHER instruction
            AsmNode *next_real = skip_other_nodes(curr->next);

            if (next_real && next_real->type == OP_MOV &&
                next_real->dst_op.mode == MODE_REG &&
                str_case_eq(next_real->dst_op.reg, target_reg))
            {
                // Found immediate overwrite - remove the load
                AsmNode *nodes[] = {curr};
                remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_LOADS);
                optimizations++;
                continue;
            }
        }

        curr = next;
    }

    return optimizations;
}
