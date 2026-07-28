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
// PEEPHOLE: Strength Reduction
// Replaces expensive operations with cheaper equivalents.
// Note: On Vircon32, all instructions are 1 cycle, so these are
// cost-neutral but may improve code clarity or reduce variety.
//   - IMUL r, 0 → MOV r, 0
//   - IMUL r, 1 → remove (identity)
//   - IMUL r, 2 → IADD r, r (cost-neutral, but idiomatic)
//   - IDIV r, 1 → remove (identity)
// ===================================================================
int peephole_reduce(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        // --- IMUL Strength Reduction ---
        if (curr->type == OP_IMUL && curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float)
        {
            int val = curr->src_op.immediate;

            // --- Multiply by 0 → MOV 0 ---
            // Any value * 0 = 0
            if (val == 0)
            {
                curr->type = OP_MOV;
                safe_str_copy(curr->mnemonic, "MOV", sizeof(curr->mnemonic));
                snprintf(curr->raw, sizeof(curr->raw), "    MOV %s, 0", curr->dst_op.raw);
                optimizations++;
                curr = curr->next;
                continue;
            }

            // --- Multiply by 1 → Remove ---
            // Any value * 1 = itself (identity operation)
            if (val == 1)
            {
                AsmNode *to_remove = curr;
                curr = curr->next;
                remove_node(to_remove);
                optimizations++;
                continue;
            }

            // --- Multiply by 2 → IADD r, r ---
            // x * 2 = x + x (cost-neutral on Vircon32)
            if (val == 2)
            {
                curr->type = OP_IADD;
                safe_str_copy(curr->mnemonic, "IADD", sizeof(curr->mnemonic));
                curr->src_op.mode = MODE_REG;
                safe_str_copy(curr->src_op.reg, curr->dst_op.reg, sizeof(curr->src_op.reg));
                safe_str_copy(curr->src_op.raw, curr->dst_op.raw, sizeof(curr->src_op.raw));
                snprintf(curr->raw, sizeof(curr->raw), "    IADD %s, %s", curr->dst_op.raw, curr->dst_op.raw);
                optimizations++;
                curr = curr->next;
                continue;
            }
        }

        // --- IDIV Strength Reduction ---
        if (curr->type == OP_IDIV && curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float)
        {
            int val = curr->src_op.immediate;

            // --- Divide by 1 → Remove ---
            // Any value / 1 = itself (identity operation)
            if (val == 1)
            {
                AsmNode *to_remove = curr;
                curr = curr->next;
                remove_node(to_remove);
                optimizations++;
                continue;
            }
        }

        curr = curr->next;
    }
    return optimizations;
}
