#include "v32opt.h"

// ===================================================================
// PEEPHOLE: Shift Optimizations
// Optimizes SHL (shift left) instructions on Vircon32:
//   - On Vircon32, SHL with positive value shifts LEFT
//   - On Vircon32, SHL with negative value shifts RIGHT
//   - Removing SHL by 0 (no-op)
//   - Replacing SHL by 1 with IADD r, r (cost-neutral on Vircon32)
//
// Examples:
//   SHL R1, 0     ->  (removed)
//   SHL R1, 1     ->  IADD R1, R1
//   SHL R1, 8     ->  (kept, no simpler form)
//   SHL R1, -1    ->  (kept, shift right by 1)
//
// DELIBERATELY NOT DONE (both removed as unsound):
//   - "SHL r,N; SHL r,-N" cancel-pair removal: NOT an identity. Bits
//     shifted out of the word are gone, so (x << N) >> N != x in general
//     (e.g. x = 0x80000000, N = 4: original ends at 0, the "optimized"
//     version keeps 0x80000000). Only valid with a proven-zero top bits,
//     which no pass here tracks.
//   - "SHL r,r" (dst == src register) removal: not a no-op -- it shifts
//     r by r's own value (e.g. R1=2 -> R1 = 2<<2 = 8, not 2).
// ===================================================================

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
            if (remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_SHIFTS)) optimizations++;
            continue;
        }

        // Guard with is_numeric_immediate
        if (curr->type == OP_SHL &&
            curr->dst_op.mode == MODE_REG &&
            is_numeric_immediate(&curr->src_op) && !curr->src_op.is_float &&
            curr->src_op.immediate == 1 &&
            trigger_allowed())
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

        curr = next;
    }

    return optimizations;
}
