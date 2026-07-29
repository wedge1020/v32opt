#include "v32opt.h"

// ===================================================================
// PEEPHOLE: Immediate Math Combining
//
// Combines consecutive arithmetic operations with immediate operands:
//
// Patterns handled:
//   - IADD r, 5; ISUB r, 3 → IADD r, 2
//   - IADD r, 5; ISUB r, 5 → remove both (cancels out)
//   - IADD r, -3; IADD r, 5 → IADD r, 2
//   - ISUB r, 3; ISUB r, 2 → ISUB r, 5
//
// Example:
//   Input:  IADD R1, 10
//           ISUB R1, 3
//   Output: IADD R1, 7
//
//   Input:  IADD R1, 5
//           ISUB R1, 5
//   Output: (both removed)
//
// Returns: Number of optimizations applied
// ===================================================================
int peephole_immediates(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        // Guard with is_numeric_immediate to prevent evaluating symbolic defines as 0
        if ((curr->type == OP_IADD || curr->type == OP_ISUB) &&
            curr->dst_op.mode == MODE_REG &&
            is_numeric_immediate(&curr->src_op) && !curr->src_op.is_float)
        {
            AsmNode *n2 = skip_other_nodes(curr->next);

            if (n2 && (n2->type == OP_IADD || n2->type == OP_ISUB) &&
                n2->dst_op.mode == MODE_REG &&
                is_numeric_immediate(&n2->src_op) && !n2->src_op.is_float &&
                str_case_eq(curr->dst_op.reg, n2->dst_op.reg))
            {
                int val1 = (curr->type == OP_IADD) ? curr->src_op.immediate : -curr->src_op.immediate;
                int val2 = (n2->type == OP_IADD) ? n2->src_op.immediate : -n2->src_op.immediate;
                int combined = val1 + val2;

                // --- Cancellation Case ---
                if (combined == 0)
                {
                    // Removed redundant debug comment insertions; remove_with_debug handles this
                    AsmNode *nodes[] = {curr, n2};
                    remove_with_debug(&curr, nodes, 2, OPT_PEEPHOLE_IMMEDIATES);
                    optimizations += 2;
                    continue;
                }
                // --- Non-Zero Combination ---
                else
                {
                    // Keep comment for curr (it is mutating), but not for n2 (it is being removed)
                    if (config.debug) {
                        insert_debug_comment(curr->prev, OPT_PEEPHOLE_IMMEDIATES, curr->raw);
                    }
                    curr->type = (combined > 0) ? OP_IADD : OP_ISUB;
                    strcpy(curr->mnemonic, (combined > 0) ? "IADD" : "ISUB");
                    curr->src_op.immediate = abs(combined);
                    snprintf(curr->src_op.raw, sizeof(curr->src_op.raw), "%d", abs(combined));
                    snprintf(curr->raw, sizeof(curr->raw), "    %s %s, %d",
                             curr->mnemonic, curr->dst_op.raw, abs(combined));

                    AsmNode *nodes[] = {n2};
                    remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_IMMEDIATES);
                    optimizations++;
                    continue;
                }
            }
        }
        curr = curr->next;
    }
    return optimizations;
}
