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
// PEEPHOLE: Algebraic Simplification
// Removes or replaces instructions that are algebraically redundant:
//   - MOV r, r → remove (no-op)
//   - IADD/ISUB r, 0 → remove (identity)
//   - IMUL r, 2 → replace with IADD r, r (cost-neutral on Vircon32, but idiomatic)
// ===================================================================
int peephole_algebra(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr != NULL)
    {
        AsmNode *next = curr->next;

        // --- MOV r, r (Self-Move Elimination) ---
        // Copying a register to itself is a no-op.
        if (curr->type == OP_MOV &&
            curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_REG &&
            str_case_eq(curr->dst_op.reg, curr->src_op.reg))
        {
            remove_node(curr);
            optimizations++;
            curr = next;
            continue;
        }

        // --- IADD/ISUB with Immediate 0 ---
        // Adding or subtracting 0 leaves the register unchanged.
        if ((curr->type == OP_IADD || curr->type == OP_ISUB) &&
            curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float && curr->src_op.immediate == 0)
        {
            remove_node(curr);
            optimizations++;
            curr = next;
            continue;
        }

        // --- IMUL by 2 Strength Reduction ---
        // IMUL r, 2 → IADD r, r
        // Note: Cost-neutral on Vircon32 (both are 1 cycle), but IADD may be
        // preferred for clarity or to reduce instruction variety.
        if (curr->type == OP_IMUL &&
            curr->dst_op.mode == MODE_REG &&
            curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float &&
            curr->src_op.immediate == 2)
        {
            curr->type = OP_IADD;
            strcpy(curr->mnemonic, "IADD");
            curr->src_op = curr->dst_op; // Source becomes same as destination
            snprintf(curr->raw, sizeof(curr->raw), "    IADD %s, %s", curr->dst_op.raw, curr->src_op.raw);
            optimizations++;
        }

        // --- IMUL by 0 → MOV r, 0 ---
        if (curr->type == OP_IMUL &&
            curr->dst_op.mode == MODE_REG &&
            curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float &&
            curr->src_op.immediate == 0)
        {
            insert_debug_comment(curr->prev, OPT_PEEPHOLE_ALGEBRA, curr->raw);
            curr->type = OP_MOV;
            strcpy(curr->mnemonic, "MOV");
            snprintf(curr->raw, sizeof(curr->raw), "    MOV %s, 0", curr->dst_op.raw);
            optimizations++;
            curr = next;
            continue;
        }

        // --- IMUL by 1 → Remove (identity) ---
        if (curr->type == OP_IMUL &&
            curr->dst_op.mode == MODE_REG &&
            curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float &&
            curr->src_op.immediate == 1)
        {
            AsmNode *nodes[] = {curr};
            remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_ALGEBRA);
            optimizations++;
            continue;
        }

        // --- IDIV by 1 → Remove (identity) ---
        if (curr->type == OP_IDIV &&
            curr->dst_op.mode == MODE_REG &&
            curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float &&
            curr->src_op.immediate == 1)
        {
            AsmNode *nodes[] = {curr};
            remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_ALGEBRA);
            optimizations++;
            continue;
        }

        curr = next;
    }

    return optimizations;
}
