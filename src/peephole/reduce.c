#include "v32opt.h"

// ===================================================================
// PEEPHOLE: Strength Reduction
//
// Replaces expensive operations with cheaper equivalents.
// Note: On Vircon32, all instructions are 1 cycle, so these are
// cost-neutral but may improve code clarity or reduce instruction variety.
//
// Patterns handled:
//   - IMUL r, 0 → MOV r, 0
//   - IMUL r, 1 → remove (identity)
//   - IMUL r, 2 → IADD r, r (cost-neutral, but more idiomatic)
//   - IDIV r, 1 → remove (identity)
//
// Example:
//   Input:  IMUL R1, 0
//   Output: MOV R1, 0
//
//   Input:  IMUL R1, 2
//   Output: IADD R1, R1
//
// Returns: Number of optimizations applied
// ===================================================================
int peephole_reduce(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        // Guard with is_numeric_immediate
        if (curr->type == OP_IMUL && curr->dst_op.mode == MODE_REG &&
            is_numeric_immediate(&curr->src_op) && !curr->src_op.is_float)
        {
            int val = curr->src_op.immediate;

            if (val == 0)
            {
                insert_debug_comment(curr->prev, OPT_PEEPHOLE_REDUCE, curr->raw);
                curr->type = OP_MOV;
                strcpy(curr->mnemonic, "MOV");
                snprintf(curr->raw, sizeof(curr->raw), "    MOV %s, 0", curr->dst_op.raw);
                optimizations++;
                curr = curr->next;
                continue;
            }

            if (val == 1)
            {
                // Removed redundant debug comment
                AsmNode *nodes[] = {curr};
                remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_REDUCE);
                optimizations++;
                continue;
            }

            if (val == 2)
            {
                insert_debug_comment(curr->prev, OPT_PEEPHOLE_REDUCE, curr->raw);
                curr->type = OP_IADD;
                strcpy(curr->mnemonic, "IADD");
                curr->src_op.mode = MODE_REG;
                safe_str_copy(curr->src_op.reg, curr->dst_op.reg, sizeof(curr->src_op.reg));
                safe_str_copy(curr->src_op.raw, curr->dst_op.raw, sizeof(curr->src_op.raw));
                snprintf(curr->raw, sizeof(curr->raw), "    IADD %s, %s",
                         curr->dst_op.raw, curr->dst_op.raw);
                optimizations++;
                curr = curr->next;
                continue;
            }
        }

        // Guard with is_numeric_immediate
        if (curr->type == OP_IDIV && curr->dst_op.mode == MODE_REG &&
            is_numeric_immediate(&curr->src_op) && !curr->src_op.is_float)
        {
            if (curr->src_op.immediate == 1)
            {
                // Removed redundant debug comment
                AsmNode *nodes[] = {curr};
                remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_REDUCE);
                optimizations++;
                continue;
            }
        }

        curr = curr->next;
    }

    return optimizations;
}
/*
int peephole_reduce(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        // --- IMUL Strength Reduction ---
        if (curr->type == OP_IMUL && curr->dst_op.mode == MODE_REG &&
            curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float)
        {
            int val = curr->src_op.immediate;

            // --- Multiply by 0 → MOV 0 ---
            if (val == 0)
            {
                insert_debug_comment(curr->prev, OPT_PEEPHOLE_REDUCE, curr->raw);
                curr->type = OP_MOV;
                strcpy(curr->mnemonic, "MOV");
                snprintf(curr->raw, sizeof(curr->raw), "    MOV %s, 0", curr->dst_op.raw);
                optimizations++;
                curr = curr->next;
                continue;
            }

            // --- Multiply by 1 → Remove (identity) ---
            if (val == 1)
            {
                insert_debug_comment(curr->prev, OPT_PEEPHOLE_REDUCE, curr->raw);
                AsmNode *nodes[] = {curr};
                remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_REDUCE);
                optimizations++;
                continue;
            }

            // --- Multiply by 2 → IADD r, r ---
            if (val == 2)
            {
                insert_debug_comment(curr->prev, OPT_PEEPHOLE_REDUCE, curr->raw);
                curr->type = OP_IADD;
                strcpy(curr->mnemonic, "IADD");
                curr->src_op.mode = MODE_REG;
                safe_str_copy(curr->src_op.reg, curr->dst_op.reg, sizeof(curr->src_op.reg));
                safe_str_copy(curr->src_op.raw, curr->dst_op.raw, sizeof(curr->src_op.raw));
                snprintf(curr->raw, sizeof(curr->raw), "    IADD %s, %s",
                         curr->dst_op.raw, curr->dst_op.raw);
                optimizations++;
                curr = curr->next;
                continue;
            }
        }

        // --- IDIV Strength Reduction ---
        if (curr->type == OP_IDIV && curr->dst_op.mode == MODE_REG &&
            curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float)
        {
            // --- Divide by 1 → Remove (identity) ---
            if (curr->src_op.immediate == 1)
            {
                insert_debug_comment(curr->prev, OPT_PEEPHOLE_REDUCE, curr->raw);
                AsmNode *nodes[] = {curr};
                remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_REDUCE);
                optimizations++;
                continue;
            }
        }

        curr = curr->next;
    }

    return optimizations;
}
*/
