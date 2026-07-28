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
// PEEPHOLE: Adjacent Instruction Pair Elimination
// Scans a sliding window of two consecutive instructions and removes
// redundant pairs:
//   - IEQ/INE followed by CIB on the same register (CIB is redundant)
//   - BNOT x; BNOT x → remove both (double negation cancels)
//   - PUSH r; POP r → remove both (no net effect)
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
        // If they target the same register, the CIB is redundant because
        // the flag is already set and the value is already boolean
        if ((n1->type == OP_IEQ || n1->type == OP_INE) &&
             n2->type == OP_CIB &&
             n1->dst_op.mode == MODE_REG && n2->dst_op.mode == MODE_REG &&
             str_case_eq(n1->dst_op.reg, n2->dst_op.reg))
        {
            // Don't remove CIB if next non-comment is JT/JF
            AsmNode *after_cib = n2->next;
            while (after_cib && after_cib->type == OP_OTHER &&
                   (after_cib->raw[0] == '\0' || after_cib->raw[0] == ';')) {
                after_cib = after_cib->next;
            }
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
        // Identical consecutive operations that cancel each other out:
        //   - BNOT R4 ; BNOT R4 -> (Bitwise NOT twice = Identity)
        //   - ISGN R4 ; ISGN R4 -> (Two's complement negate twice = Identity)
        //   - NOT R4  ; NOT R4  -> (Logical NOT twice = Identity)
        // ----------------------------------------------------------------
        if (str_case_eq(n1->mnemonic, "BNOT") ||
            str_case_eq(n1->mnemonic, "ISGN") ||
            str_case_eq(n1->mnemonic, "NEG")  ||
            str_case_eq(n1->mnemonic, "NOT"))
        {
            AsmNode *next = n1->next;
            // Safely skip any inline comments or blank lines between the pair
            while (next && next->type == OP_OTHER) next = next->next;

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
        // Toggling a register with the exact same value twice cancels out:
        //   - XOR R1, R2 ; XOR R1, R2 -> cancels out!
        //   - XOR R1, 42 ; XOR R1, 42 -> cancels out!
        // ----------------------------------------------------------------
        if (str_case_eq(n1->mnemonic, "XOR"))
        {
            AsmNode *next = n1->next;
            while (next && next->type == OP_OTHER) next = next->next;

            if (next && str_case_eq(next->mnemonic, "XOR"))
            {
                // Verify destination registers match
                if (n1->has_dst && next->has_dst && str_case_eq(n1->dst_op.reg, next->dst_op.reg))
                {
                    bool src_match = false;

                    // Check if both XOR with the same register
                    if (n1->src_op.mode == MODE_REG && next->src_op.mode == MODE_REG) {
                        if (str_case_eq(n1->src_op.reg, next->src_op.reg)) src_match = true;
                    }
                    // Check if both XOR with the exact same immediate value
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
        // Must skip comments between PUSH and POP
        AsmNode *pop_node = n1->next;
        while (pop_node && pop_node->type == OP_OTHER) {
            pop_node = pop_node->next;
        }
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
