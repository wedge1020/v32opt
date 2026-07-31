#include "v32opt.h"

// ===================================================================
// PEEPHOLE: Compiler Myopia (Redundant Store-Then-Load Elimination)
//
// Addresses short-sighted compiler patterns where a register value is 
// stored to memory and immediately reloaded back into the same register 
// on the very next instruction.
//
// Pattern handled:
//   MOV [mem], R1
//   MOV R1, [mem]    -->  [REMOVED]
//
// Returns: Number of optimizations applied
// ===================================================================
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

            // Retrieve the immediate next non-comment/non-blank instruction
            AsmNode *scan = skip_comments_and_blanks(curr->next);

            // Ensure scan exists, is not a control boundary/label, and is an OP_MOV
            if (scan && !is_control_flow_boundary(scan) && scan->type == OP_MOV)
            {
                // Match: MOV R_src, [mem] (Load from same address into the same register)
                if (scan->dst_op.mode == MODE_REG &&
                    scan->src_op.mode == MODE_INDIRECT &&
                    str_case_eq(scan->dst_op.reg, src_reg) &&
                    str_case_eq(scan->src_op.reg, mem_reg) &&
                    scan->src_op.offset == mem_off)
                {
                    // Optionally log debug comment before removing
                    insert_debug_comment(scan->prev, OPT_PEEPHOLE_MOVS, scan->raw);

                    AsmNode *to_remove = scan;
                    scan = scan->next;
                    
                    // Delete the redundant load instruction
                    remove_node(to_remove);
                    optimizations++;
                }
            }
        }
        curr = curr->next;
    }

    return optimizations;
}
