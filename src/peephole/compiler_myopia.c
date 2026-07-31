#include "v32opt.h"

int peephole_compiler_myopia(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        // Target: MOV [mem], R_src (Store to indirect memory from register)
        if (curr->type == OP_MOV &&
            curr->dst_op.mode == MODE_INDIRECT &&
            curr->src_op.mode == MODE_REG)
        {
            char *mem_reg = curr->dst_op.reg;
            int mem_off   = curr->dst_op.offset;
            char *src_reg = curr->src_op.reg;

            // Scan forward to find a redundant load
            AsmNode *scan = curr->next;
            while (scan)
            {
                // Stop immediately if we hit a label or branch
                if (is_control_flow_boundary(scan)) {
                    break;
                }

                // Safely skip ALL comments and blank lines (fixes MATCH 5)
                if (scan->type == OP_OTHER) {
                    scan = scan->next;
                    continue;
                }

                // Match: MOV R_src, [mem] (Load from same address into the same register)
                if (scan->type == OP_MOV &&
                    scan->dst_op.mode == MODE_REG &&
                    scan->src_op.mode == MODE_INDIRECT &&
                    str_case_eq(scan->dst_op.reg, src_reg) &&
                    str_case_eq(scan->src_op.reg, mem_reg) &&
                    scan->src_op.offset == mem_off)
                {
                    insert_debug_comment(scan->prev, OPT_PEEPHOLE_COMPILER_MYOPIA, scan->raw);

                    AsmNode *to_remove = scan;
                    scan = scan->next; // Advance before deleting
                    remove_node(to_remove);
                    optimizations++;
                    break; // Successfully optimized, break inner loop
                }

                // If it's not the redundant load, verify it doesn't clobber our state
                bool clobbers = false;

                // 1. Modifies the tracked value register or base address register
                if (modifies_register(scan, src_reg)) clobbers = true;
                if (modifies_register(scan, mem_reg)) clobbers = true;

                // 2. Modifies memory (indirect stores, pushes, or calls)
                // Note: CALL writes to stack memory and potentially global memory
                if (scan->has_dst && scan->dst_op.mode == MODE_INDIRECT) clobbers = true;
                if (scan->type == OP_PUSH || scan->type == OP_CALL) clobbers = true;

                // If state is invalidated, abort the forward scan
                if (clobbers) {
                    break;
                }

                scan = scan->next;
            }
        }
        curr = curr->next;
    }

    return optimizations;
}
