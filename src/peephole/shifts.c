// ===================================================================
// PEEPHOLE: Shift Optimizations
// Optimizes SHL (shift left) instructions on Vircon32:
//   - On Vircon32, SHL with negative value shifts RIGHT
//   - SHL with positive value shifts LEFT
//   - Removing SHL by 0 (no-op)
//   - Replacing SHL by 1 with IADD r, r (cost-neutral on Vircon32)
//   - Removing consecutive opposite shifts that cancel out (SHL r, N; SHL r, -N)
//   - Removing shifts with same src and dst register
//
// Examples:
//   SHL R1, 0     ->  (removed)
//   SHL R1, 1     ->  IADD R1, R1
//   SHL R1, 8     ->  (kept, no simpler form)
//   SHL R1, -1    ->  (kept, shift right by 1)
//   SHL R1, 4; SHL R1, -4  ->  (both removed, cancel out)
// ===================================================================
#include "v32opt.h"

int peephole_shifts(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr != NULL)
    {
        AsmNode *next = curr->next;

        // Guard with is_numeric_immediate
        if (curr->type == OP_SHL &&
            is_numeric_immediate(&curr->src_op) && !curr->src_op.is_float &&
            curr->src_op.immediate == 0)
        {
            AsmNode *nodes[] = {curr};
            remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_SHIFTS);
            optimizations++;
            continue;
        }

        // Guard with is_numeric_immediate
        if (curr->type == OP_SHL &&
            curr->dst_op.mode == MODE_REG &&
            is_numeric_immediate(&curr->src_op) && !curr->src_op.is_float &&
            curr->src_op.immediate == 1)
        {
            insert_debug_comment(curr->prev, OPT_PEEPHOLE_SHIFTS, curr->raw);
            curr->type = OP_IADD;
            strcpy(curr->mnemonic, "IADD");
            curr->src_op = curr->dst_op;
            snprintf(curr->raw, sizeof(curr->raw), "    IADD %s, %s",
                     curr->dst_op.raw, curr->src_op.raw);
            optimizations++;
            curr = next;
            continue;
        }

        if (curr->type == OP_SHL)
        {
            AsmNode *next_real = skip_other_nodes(curr->next);

            if (next_real && next_real->type == OP_SHL)
            {
                // Guard BOTH nodes with is_numeric_immediate
                if (curr->dst_op.mode == MODE_REG && next_real->dst_op.mode == MODE_REG &&
                    str_case_eq(curr->dst_op.reg, next_real->dst_op.reg) &&
                    is_numeric_immediate(&curr->src_op) && is_numeric_immediate(&next_real->src_op) &&
                    !curr->src_op.is_float && !next_real->src_op.is_float &&
                    curr->src_op.immediate == -next_real->src_op.immediate &&
                    curr->src_op.immediate != 0)
                {
                    AsmNode *nodes[] = {curr, next_real};
                    remove_with_debug(&curr, nodes, 2, OPT_PEEPHOLE_SHIFTS);
                    optimizations += 2;
                    continue;
                }
            }
        }

        if (curr->type == OP_SHL &&
            curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_REG &&
            str_case_eq(curr->dst_op.reg, curr->src_op.reg))
        {
            AsmNode *nodes[] = {curr};
            remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_SHIFTS);
            optimizations++;
            continue;
        }

        curr = next;
    }

    return optimizations;
}
/*
int peephole_shifts(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr != NULL)
    {
        AsmNode *next = curr->next;

        // ----------------------------------------------------------
        // PATTERN 1: Shift by 0 (Identity)
        // SHL R1, 0 → remove (shifting by 0 does nothing)
        // ----------------------------------------------------------
        if (curr->type == OP_SHL &&
            curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float &&
            curr->src_op.immediate == 0)
        {
            AsmNode *nodes[] = {curr};
            remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_SHIFTS);
            optimizations++;
            continue;
        }

        // ----------------------------------------------------------
        // PATTERN 2: SHL by 1 → IADD r, r
        // On Vircon32, both are 1 cycle, but IADD may be preferred
        // for clarity or to reduce instruction variety
        // ----------------------------------------------------------
        if (curr->type == OP_SHL &&
            curr->dst_op.mode == MODE_REG &&
            curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float &&
            curr->src_op.immediate == 1)
        {
            insert_debug_comment(curr->prev, OPT_PEEPHOLE_SHIFTS, curr->raw);
            curr->type = OP_IADD;
            strcpy(curr->mnemonic, "IADD");
            curr->src_op = curr->dst_op; // Source becomes same as destination
            snprintf(curr->raw, sizeof(curr->raw), "    IADD %s, %s",
                     curr->dst_op.raw, curr->src_op.raw);
            optimizations++;
            curr = next;
            continue;
        }

        // ----------------------------------------------------------
        // PATTERN 3: Consecutive Opposite Shifts (Cancel Out)
        // SHL R1, N; SHL R1, -N → both removed (results in original value)
        // Only applies when shift amounts are equal magnitude, opposite sign
        // ----------------------------------------------------------
        if (curr->type == OP_SHL)
        {
            // Skip OP_OTHER nodes to find the next real instruction
            AsmNode *next_real = skip_other_nodes(curr->next);

            if (next_real && next_real->type == OP_SHL)
            {
                // Check if both shifts target the same register with opposite amounts
                if (curr->dst_op.mode == MODE_REG && next_real->dst_op.mode == MODE_REG &&
                    str_case_eq(curr->dst_op.reg, next_real->dst_op.reg) &&
                    curr->src_op.mode == MODE_IMMEDIATE && next_real->src_op.mode == MODE_IMMEDIATE &&
                    !curr->src_op.is_float && !next_real->src_op.is_float &&
                    curr->src_op.immediate == -next_real->src_op.immediate &&
                    curr->src_op.immediate != 0)
                {
                    AsmNode *nodes[] = {curr, next_real};
                    remove_with_debug(&curr, nodes, 2, OPT_PEEPHOLE_SHIFTS);
                    optimizations += 2;
                    continue;
                }
            }
        }

        // ----------------------------------------------------------
        // PATTERN 4: Shift with Same Source and Destination
        // SHL R1, R1 → remove (undefined behavior)
        // ----------------------------------------------------------
        if (curr->type == OP_SHL &&
            curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_REG &&
            str_case_eq(curr->dst_op.reg, curr->src_op.reg))
        {
            AsmNode *nodes[] = {curr};
            remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_SHIFTS);
            optimizations++;
            continue;
        }

        curr = next;
    }

    return optimizations;
}*/
