#include "v32opt.h"

// ===================================================================
// PEEPHOLE: Adjacent Instruction Pair Elimination
//
// Scans a sliding window of two consecutive instructions and removes
// redundant pairs that cancel each other out.
//
// Patterns handled:
//   - IEQ/INE followed by CIB on same register (CIB redundant)
//   - BNOT x; BNOT x → remove both (double negation cancels)
//   - NOT x; NOT x → remove both (double negation cancels)
//   - ISGN x; ISGN x → remove both (sign flip twice = identity)
//   - NEG x; NEG x → remove both (negate twice = identity)
//   - XOR r1, r2; XOR r1, r2 → remove both (XOR twice = identity)
//   - PUSH r; POP r → remove both (no net stack effect)
//
// Example:
//   Input:  IEQ R1, R2
//           CIB R1
//   Output: (IEQ only, CIB removed)
//
//   Input:  BNOT R4
//           BNOT R4
//   Output: (both removed)
//
// Returns: Number of optimizations applied
// ===================================================================
int peephole_pairs(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr && curr->next)
    {
        AsmNode *n1 = curr;
        AsmNode *n2 = curr->next;

        // --- IEQ/INE + CIB Redundancy ---
        // IEQ/INE sets a flag; CIB converts an integer to boolean
        // If they target the same register, the CIB is redundant
        if ((n1->type == OP_IEQ || n1->type == OP_INE) &&
             n2->type == OP_CIB &&
             n1->dst_op.mode == MODE_REG && n2->dst_op.mode == MODE_REG &&
             str_case_eq(n1->dst_op.reg, n2->dst_op.reg))
        {
            // Don't remove CIB if next non-comment is JT/JF
            AsmNode *after_cib = skip_other_nodes(n2->next);
            if (after_cib && (str_case_eq(after_cib->mnemonic, "JT") ||
                               str_case_eq(after_cib->mnemonic, "JF"))) {
                curr = curr->next;
                continue;
            }

            AsmNode *nodes[] = {n2};
            remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_PAIRS);
            optimizations++;
            continue;
        }

        // ----------------------------------------------------------------
        // PATTERN: Self-Inverting Pairs (Involutions)
        // Identical consecutive operations that cancel each other out
        // ----------------------------------------------------------------
        if (str_case_eq(n1->mnemonic, "BNOT") ||
            str_case_eq(n1->mnemonic, "ISGN") ||
            str_case_eq(n1->mnemonic, "NEG")  ||
            str_case_eq(n1->mnemonic, "NOT"))
        {
            AsmNode *next = skip_other_nodes(n1->next);

            if (next && str_case_eq(next->mnemonic, n1->mnemonic))
            {
                char *reg1 = n1->has_dst ? n1->dst_op.reg : (n1->has_src ? n1->src_op.reg : NULL);
                char *reg2 = next->has_dst ? next->dst_op.reg : (next->has_src ? next->src_op.reg : NULL);

                if (reg1 && reg2 && str_case_eq(reg1, reg2))
                {
                    AsmNode *nodes[] = {n1, next};
                    remove_with_debug(&curr, nodes, 2, OPT_PEEPHOLE_PAIRS);
                    optimizations += 2;
                    continue;
                }
            }
        }

        // ----------------------------------------------------------------
        // BONUS PATTERN: Identical XOR Pairs
        // Toggling a register with the exact same value twice cancels out
        // ----------------------------------------------------------------
        if (str_case_eq(n1->mnemonic, "XOR"))
        {
            AsmNode *next = skip_other_nodes(n1->next);

            if (next && str_case_eq(next->mnemonic, "XOR"))
            {
                if (n1->has_dst && next->has_dst && str_case_eq(n1->dst_op.reg, next->dst_op.reg))
                {
                    bool src_match = false;

                    if (n1->src_op.mode == MODE_REG && next->src_op.mode == MODE_REG) {
                        if (str_case_eq(n1->src_op.reg, next->src_op.reg)) src_match = true;
                    }
                    else if (n1->src_op.mode == MODE_IMMEDIATE && next->src_op.mode == MODE_IMMEDIATE) {
                        if (n1->src_op.offset == next->src_op.offset &&
                            str_case_eq(n1->src_op.raw, next->src_op.raw)) src_match = true;
                    }

                    if (src_match)
                    {
                        AsmNode *nodes[] = {n1, next};
                        remove_with_debug(&curr, nodes, 2, OPT_PEEPHOLE_PAIRS);
                        optimizations += 2;
                        continue;
                    }
                }
            }
        }

        // --- PUSH/POP Pair Elimination ---
        // PUSH r; POP r → no net effect on the stack or register
        AsmNode *pop_node = skip_other_nodes(n1->next);

        if (n1->type == OP_PUSH && pop_node && pop_node->type == OP_POP &&
            n1->dst_op.mode == MODE_REG && pop_node->dst_op.mode == MODE_REG &&
            str_case_eq(n1->dst_op.reg, pop_node->dst_op.reg))
        {
            AsmNode *nodes[] = {n1, pop_node};
            remove_with_debug(&curr, nodes, 2, OPT_PEEPHOLE_PAIRS);
            optimizations += 2;
            continue;
        }

        curr = curr->next;
    }

    return optimizations;
}
