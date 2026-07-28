#include "v32opt.h"

// ===================================================================
// PEEPHOLE: Algebraic Simplification
//
// Removes or replaces instructions that are algebraically redundant
// or can be simplified to cheaper equivalents.
//
// Patterns handled:
//   - MOV r, r → remove (no-op, self-move)
//   - IADD/ISUB r, 0 → remove (identity operation)
//   - IMUL r, 2 → replace with IADD r, r (cost-neutral, more idiomatic)
//   - IMUL r, 0 → replace with MOV r, 0
//   - IMUL r, 1 → remove (identity)
//   - IDIV r, 1 → remove (identity)
//
// Example:
//   Input:  MOV R1, R1
//   Output: (removed)
//
//   Input:  IMUL R2, 2
//   Output: IADD R2, R2
//
// Returns: Number of optimizations applied
// ===================================================================
int peephole_algebra(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr != NULL)
    {
        AsmNode *next = curr->next;

        // --- MOV r, r (Self-Move Elimination) ---
        // Copying a register to itself is a no-op
        if (curr->type == OP_MOV &&
            curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_REG &&
            str_case_eq(curr->dst_op.reg, curr->src_op.reg))
        {
            insert_debug_comment(curr->prev, OPT_PEEPHOLE_ALGEBRA, curr->raw);
            AsmNode *nodes[] = {curr};
            remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_ALGEBRA);
            optimizations++;
            continue;
        }

        // --- IADD/ISUB with Immediate 0 ---
        // Adding or subtracting 0 leaves the register unchanged
        if ((curr->type == OP_IADD || curr->type == OP_ISUB) &&
            curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float &&
            curr->src_op.immediate == 0)
        {
            insert_debug_comment(curr->prev, OPT_PEEPHOLE_ALGEBRA, curr->raw);
            AsmNode *nodes[] = {curr};
            remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_ALGEBRA);
            optimizations++;
            continue;
        }

        // --- IMUL by 2 Strength Reduction ---
        // IMUL r, 2 → IADD r, r (cost-neutral on Vircon32, but more idiomatic)
        if (curr->type == OP_IMUL &&
            curr->dst_op.mode == MODE_REG &&
            curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float &&
            curr->src_op.immediate == 2)
        {
            insert_debug_comment(curr->prev, OPT_PEEPHOLE_ALGEBRA, curr->raw);
            curr->type = OP_IADD;
            strcpy(curr->mnemonic, "IADD");
            curr->src_op = curr->dst_op;
            snprintf(curr->raw, sizeof(curr->raw), "    IADD %s, %s",
                     curr->dst_op.raw, curr->src_op.raw);
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
            insert_debug_comment(curr->prev, OPT_PEEPHOLE_ALGEBRA, curr->raw);
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
            insert_debug_comment(curr->prev, OPT_PEEPHOLE_ALGEBRA, curr->raw);
            AsmNode *nodes[] = {curr};
            remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_ALGEBRA);
            optimizations++;
            continue;
        }

        curr = next;
    }

    return optimizations;
}
