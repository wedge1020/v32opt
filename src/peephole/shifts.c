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
// PEEPHOLE: Shift Optimizations
// On Vircon32, SHL with positive value = left shift, negative = right shift.
//   - SHL r, 0 → remove (no-op; applies to both left and right shifts)
//   - SHL r, 1 → IADD r, r (cost-neutral on Vircon32, but idiomatic)
//   - SHL r, -1 → right shift by 1; left unchanged (no benefit to converting to IDIV)
// ===================================================================
int peephole_shifts(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        if (curr->type == OP_SHL &&
            curr->dst_op.mode == MODE_REG &&
            curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float)
        {
            int shift = curr->src_op.immediate;

            // --- Shift by 0 (No-Op) ---
            // Left or right shift by 0 leaves the register unchanged.
            if (shift == 0)
            {
                AsmNode *to_remove = curr;
                curr = curr->next;
                remove_node(to_remove);
                optimizations++;
                continue;
            }

            // --- Left Shift by 1 ---
            // SHL r, 1 = r * 2 = r + r
            // Cost-neutral on Vircon32 (both SHL and IADD are 1 cycle),
            // but IADD may be preferred for consistency.
            else if (shift == 1)
            {
                curr->type = OP_IADD;
                safe_str_copy(curr->mnemonic, "IADD", sizeof(curr->mnemonic));
                curr->src_op = curr->dst_op; // Source becomes same as destination
                snprintf(curr->src_op.raw, sizeof(curr->src_op.raw), "%s", curr->dst_op.raw);
                snprintf(curr->raw, sizeof(curr->raw), "    IADD %s, %s", curr->dst_op.raw, curr->src_op.raw);
                optimizations++;
            }

            // Note: SHL r, -1 (right shift by 1) is left unchanged.
            // On Vircon32, SHL and IDIV are both 1 cycle, so converting
            // SHL r, -1 to IDIV r, 2 would be cost-neutral but adds no benefit.
        }
        curr = curr->next;
    }
    return optimizations;
}
