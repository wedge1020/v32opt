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
// PEEPHOLE: Immediate Math Combining
// Combines consecutive arithmetic operations with immediate operands:
//   - IADD r, 5; ISUB r, 3 → IADD r, 2
//   - IADD r, 5; ISUB r, 5 → remove both (cancels out)
// ===================================================================
int peephole_immediates(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        // Only process IADD/ISUB with register destination and immediate source
        if ((curr->type == OP_IADD || curr->type == OP_ISUB) &&
            curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float)
        {
            // 🔥 FIX: Skip ALL OP_OTHER nodes (comments/blanks), not just those starting with ;
            AsmNode *n2 = curr->next;
            while (n2 && n2->type == OP_OTHER) {
                n2 = n2->next;
            }

            // Check if n2 is also an IADD/ISUB with same destination and immediate
            if (n2 && (n2->type == OP_IADD || n2->type == OP_ISUB) &&
                n2->dst_op.mode == MODE_REG && n2->src_op.mode == MODE_IMMEDIATE && !n2->src_op.is_float &&
                str_case_eq(curr->dst_op.reg, n2->dst_op.reg))
            {
                // Calculate effective values: IADD adds, ISUB subtracts
                int val1 = (curr->type == OP_IADD) ? curr->src_op.immediate : -curr->src_op.immediate;
                int val2 = (n2->type == OP_IADD) ? n2->src_op.immediate : -n2->src_op.immediate;
                int combined = val1 + val2;

                // --- Cancellation Case ---
                if (combined == 0)
                {
                    if (config.debug) {
                        insert_debug_comment(curr->prev, OPT_PEEPHOLE_IMMEDIATES, curr->raw);
                        insert_debug_comment(n2->prev, OPT_PEEPHOLE_IMMEDIATES, n2->raw);
                    }
                    AsmNode *nodes[] = {curr, n2};
                    remove_with_debug(&curr, nodes, 2, OPT_PEEPHOLE_IMMEDIATES);
                    optimizations += 2;
                    continue;
                }
                // --- Non-Zero Combination ---
                else
                {
                    if (config.debug) {
                        insert_debug_comment(curr->prev, OPT_PEEPHOLE_IMMEDIATES, curr->raw);
                    }
                    curr->type = (combined > 0) ? OP_IADD : OP_ISUB;
                    strcpy(curr->mnemonic, (combined > 0) ? "IADD" : "ISUB");
                    curr->src_op.immediate = abs(combined);
                    snprintf(curr->src_op.raw, sizeof(curr->src_op.raw), "%d", abs(combined));
                    snprintf(curr->raw, sizeof(curr->raw), "    %s %s, %d",
                             curr->mnemonic, curr->dst_op.raw, abs(combined));
                    if (config.debug) {
                        insert_debug_comment(n2->prev, OPT_PEEPHOLE_IMMEDIATES, n2->raw);
                    }
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
