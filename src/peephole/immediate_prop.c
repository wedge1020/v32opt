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
// PEEPHOLE: Immediate Propagation & Constant Folding
// ===================================================================
int peephole_immediate_prop(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        // ----------------------------------------------------------
        // PATTERN 1: Identity Math Elimination (e.g., IADD R1, 0)
        // ----------------------------------------------------------
        if (curr->type == OP_IADD || curr->type == OP_ISUB || curr->type == OP_IMUL || curr->type == OP_IDIV)
        {
            if (curr->has_src && curr->has_dst &&
                curr->src_op.mode == MODE_IMMEDIATE && curr->dst_op.mode == MODE_REG)
            {
                long val = parse_imm_val(curr->src_op.raw);
                bool is_identity = false;

                if ((curr->type == OP_IADD || curr->type == OP_ISUB) && val == 0) {
                    is_identity = true;
                } else if ((curr->type == OP_IMUL || curr->type == OP_IDIV) && val == 1) {
                    is_identity = true;
                }

                if (is_identity) {
                    AsmNode *nodes[] = {curr};
                    remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_IMMEDIATE_PROP);
                    optimizations++;
                    continue; // curr is automatically updated to next_after by remove_with_debug
                }
            }
        }

        // ----------------------------------------------------------
        // PATTERN 2: Constant Folding (MOV Reg, Imm -> next ALU Reg, Imm)
        // ----------------------------------------------------------
        if (curr->type == OP_MOV && curr->has_dst && curr->has_src &&
            curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_IMMEDIATE)
        {
            char *target_reg = curr->dst_op.reg;
            long imm1 = parse_imm_val(curr->src_op.raw);

            // Get the very next non-comment/blank instruction
            // 🔥 FIX: Skip ALL OP_OTHER nodes regardless of indentation or raw[0]
            AsmNode *next_real = curr->next;
            while (next_real && next_real->type == OP_OTHER) {
                next_real = next_real->next;
            }

            if (!next_real || next_real->type == OP_OTHER || next_real->type == OP_LABEL) {
                curr = curr->next;
                continue;
            }

            // Only fold if it's IADD/ISUB/IMUL on target_reg with immediate
            if (next_real->has_dst && next_real->has_src &&
                next_real->dst_op.mode == MODE_REG &&
                str_case_eq(next_real->dst_op.reg, target_reg) &&
                next_real->src_op.mode == MODE_IMMEDIATE)
            {
                // Skip POW/ATAN2 (require register operands only)
                if (str_case_eq(next_real->mnemonic, "POW") ||
                    str_case_eq(next_real->mnemonic, "ATAN2")) {
                    curr = curr->next;
                    continue;
                }

                // Only fold IADD/ISUB/IMUL
                if (next_real->type != OP_IADD && next_real->type != OP_ISUB && next_real->type != OP_IMUL) {
                    curr = curr->next;
                    continue;
                }

                long imm2 = parse_imm_val(next_real->src_op.raw);
                long folded_val = 0;
                bool folded = false;

                if (next_real->type == OP_IADD) { folded_val = imm1 + imm2; folded = true; }
                else if (next_real->type == OP_ISUB) { folded_val = imm1 - imm2; folded = true; }
                else if (next_real->type == OP_IMUL) { folded_val = imm1 * imm2; folded = true; }

                if (folded) {
                    insert_debug_comment(next_real->prev, OPT_PEEPHOLE_IMMEDIATE_PROP, next_real->raw);
                    next_real->type = OP_MOV;
                    strcpy(next_real->mnemonic, "MOV");
                    next_real->has_dst = true;
                    next_real->has_src = true;
                    next_real->src_op.mode = MODE_IMMEDIATE;
                    next_real->src_op.offset = (int)folded_val;
                    snprintf(next_real->src_op.raw, sizeof(next_real->src_op.raw), "%ld", folded_val);
                    snprintf(next_real->raw, sizeof(next_real->raw), "    MOV %s, %ld",
                             next_real->dst_op.raw, folded_val);
                    optimizations++;
                }
            }
        }

        // ----------------------------------------------------------
        // PATTERN 3: Sequential Math Combining (IADD/ISUB Reg, Imm -> next IADD/ISUB Reg, Imm)
        // ----------------------------------------------------------
        else if ((curr->type == OP_IADD || curr->type == OP_ISUB) &&
                 curr->has_dst && curr->has_src &&
                 curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_IMMEDIATE)
        {
            char *target_reg = curr->dst_op.reg;
            long imm1 = parse_imm_val(curr->src_op.raw);
            if (curr->type == OP_ISUB) imm1 = -imm1;

            // Get the very next non-comment/blank instruction
            // 🔥 FIX: Skip ALL OP_OTHER nodes
            AsmNode *next_real = curr->next;
            while (next_real && next_real->type == OP_OTHER) {
                next_real = next_real->next;
            }

            if (!next_real || next_real->type == OP_OTHER || next_real->type == OP_LABEL) {
                curr = curr->next;
                continue;
            }

            // Only combine if it's IADD/ISUB on same register with immediate
            if (next_real->has_dst && next_real->has_src &&
                next_real->dst_op.mode == MODE_REG &&
                str_case_eq(next_real->dst_op.reg, target_reg) &&
                next_real->src_op.mode == MODE_IMMEDIATE &&
                (next_real->type == OP_IADD || next_real->type == OP_ISUB))
            {
                long imm2 = parse_imm_val(next_real->src_op.raw);
                if (next_real->type == OP_ISUB) imm2 = -imm2;

                long combined = imm1 + imm2;

                // Update curr instruction in place
                insert_debug_comment(curr->prev, OPT_PEEPHOLE_IMMEDIATE_PROP, curr->raw);
                if (combined >= 0) {
                    curr->type = OP_IADD;
                    strcpy(curr->mnemonic, "IADD");
                } else {
                    curr->type = OP_ISUB;
                    strcpy(curr->mnemonic, "ISUB");
                    combined = -combined;
                }
                curr->src_op.offset = (int)combined;
                snprintf(curr->src_op.raw, sizeof(curr->src_op.raw), "%ld", combined);
                snprintf(curr->raw, sizeof(curr->raw), "    %s %s, %ld",
                         curr->mnemonic, curr->dst_op.raw, combined);

                // Remove next_real using standard debugging removal without advancing curr
                AsmNode *nodes[] = {next_real};
                AsmNode *dummy = next_real;
                remove_with_debug(&dummy, nodes, 1, OPT_PEEPHOLE_IMMEDIATE_PROP);
                optimizations++;
                continue; // Stay on curr to chain 3+ sequential operations!
            }
        }

        curr = curr->next;
    }

    return optimizations;
}
