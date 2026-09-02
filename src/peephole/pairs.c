#include "v32opt.h"

// ---------------------------------------------------------------
// Returns true if 'node' makes it unsafe to remove an in-flight
// "PUSH Rn ... POP Rn" span at this point: any control transfer, any
// OTHER push/pop (they move SP, and something later in the span --
// or after it -- may depend on exactly how many words are currently
// pushed), any direct read/write of SP or BP, or a write to the
// register the span is trying to protect. A plain READ of that
// register inside the span is fine: nothing here overwrites it, so a
// read sees the same value with or without the push/pop.
// ---------------------------------------------------------------
static bool blocks_push_pop_removal(AsmNode *node, const char *reg_name)
{
    if (!node) return true;
    if (node->type == OP_LABEL) return true;
    if (str_case_eq(node->mnemonic, "JMP") || str_case_eq(node->mnemonic, "JT") ||
        str_case_eq(node->mnemonic, "JF") || str_case_eq(node->mnemonic, "CALL") ||
        str_case_eq(node->mnemonic, "RET") || str_case_eq(node->mnemonic, "HLT")) return true;
    if (node->type == OP_PUSH || node->type == OP_POP) return true;
    if (modifies_register(node, "SP") || modifies_register(node, "BP")) return true;
    if (node->has_dst && (node->dst_op.mode == MODE_REG || node->dst_op.mode == MODE_INDIRECT) &&
        str_case_eq(node->dst_op.reg, "SP")) return true;
    if (node->has_src && (node->src_op.mode == MODE_REG || node->src_op.mode == MODE_INDIRECT) &&
        str_case_eq(node->src_op.reg, "SP")) return true;
    if (modifies_register(node, reg_name)) return true;
    return false;
}

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

        // --- PUSH/POP Pair Elimination (adjacent, OR across a safe span) ---
        // PUSH r; ...; POP r -> remove both, PROVIDED nothing in between
        // writes r, touches SP/BP, transfers control, or itself pushes/pops
        // (see blocks_push_pop_removal()). This subsumes the old
        // adjacent-only match (that's just the zero-instructions-in-between
        // case) and additionally catches the extremely common compiler
        // idiom of "PUSH left-operand; evaluate right-operand; POP
        // left-operand" that v32lua emits around essentially every binary
        // operator -- unconditionally, even when the right-hand side
        // provably makes no CALL and so never needed the protection in the
        // first place.
        if (n1->type == OP_PUSH && n1->dst_op.mode == MODE_REG)
        {
            char *reg_name = n1->dst_op.reg;
            AsmNode *scan = n1->next;
            AsmNode *match = NULL;

            while (scan)
            {
                if (scan->type == OP_OTHER) { scan = scan->next; continue; }

                if (scan->type == OP_POP && scan->dst_op.mode == MODE_REG &&
                    str_case_eq(scan->dst_op.reg, reg_name))
                {
                    match = scan;
                    break;
                }

                if (blocks_push_pop_removal(scan, reg_name)) break;

                scan = scan->next;
            }

            if (match)
            {
                AsmNode *nodes[] = {n1, match};
                remove_with_debug(&curr, nodes, 2, OPT_PEEPHOLE_PAIRS);
                optimizations += 2;
                continue;
            }
        }

        curr = curr->next;
    }

    return optimizations;
}
