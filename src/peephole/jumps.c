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
// PEEPHOLE: Jump Optimizations
// ===================================================================
int peephole_jumps(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        bool did_optimize = false;

        // ----------------------------------------------------------
        // PATTERN 1: Redundant Jump to Next Label
        // ----------------------------------------------------------
        if (!did_optimize && (curr->type == OP_JMP || curr->type == OP_JT || curr->type == OP_JF))
        {
            const char *target_label = (curr->type == OP_JMP)
                ? (curr->has_dst ? curr->dst_op.raw : curr->src_op.raw)
                : curr->src_op.raw;

            if (target_label && target_label[0] != '\0')
            {
                AsmNode *next_non_comment = curr->next;
                while (next_non_comment && next_non_comment->type == OP_OTHER)
                    next_non_comment = next_non_comment->next;

                if (next_non_comment && next_non_comment->type == OP_LABEL)
                {
                    char lbl_name[128];
                    get_label_name(next_non_comment, lbl_name, sizeof(lbl_name));
                    if (str_case_eq(lbl_name, target_label))
                    {
                        insert_debug_comment(curr->prev, OPT_PEEPHOLE_JUMPS, curr->raw);
                        AsmNode *nodes[] = {curr};
                        remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_JUMPS);
                        optimizations++;
                        did_optimize = true;
                    }
                }
            }
        }

        // ----------------------------------------------------------
        // PATTERN 2: Branch Over Jump
        // ----------------------------------------------------------
        if (!did_optimize && (curr->type == OP_JT || curr->type == OP_JF))
        {
            const char *branch_target = curr->src_op.raw;
            AsmNode *next_jmp = curr->next;
            while (next_jmp && next_jmp->type == OP_OTHER)
                next_jmp = next_jmp->next;

            if (next_jmp && next_jmp->type == OP_JMP)
            {
                const char *jmp_target = next_jmp->has_dst ? next_jmp->dst_op.raw : next_jmp->src_op.raw;
                AsmNode *next_lbl = next_jmp->next;
                while (next_lbl && next_lbl->type == OP_OTHER)
                    next_lbl = next_lbl->next;

                if (next_lbl && next_lbl->type == OP_LABEL && jmp_target && jmp_target[0] != '\0')
                {
                    char lbl_name[128];
                    get_label_name(next_lbl, lbl_name, sizeof(lbl_name));
                    if (str_case_eq(lbl_name, branch_target))
                    {
                        insert_debug_comment(curr->prev, OPT_PEEPHOLE_JUMPS, curr->raw);
                        if (curr->type == OP_JT) {
                            curr->type = OP_JF; strcpy(curr->mnemonic, "JF");
                        } else {
                            curr->type = OP_JT; strcpy(curr->mnemonic, "JT");
                        }
                        safe_str_copy(curr->src_op.raw, jmp_target, sizeof(curr->src_op.raw));
                        snprintf(curr->raw, sizeof(curr->raw), "    %s %s, %s",
                                curr->mnemonic, curr->dst_op.raw, jmp_target);
                        AsmNode *nodes[] = {next_jmp};
                        AsmNode *dummy = next_jmp;
                        remove_with_debug(&dummy, nodes, 1, OPT_PEEPHOLE_JUMPS);
                        optimizations++;
                        did_optimize = true;
                    }
                }
            }
        }

        // ----------------------------------------------------------
        // PATTERN 3: Unreachable Code Elimination
        // ONLY for JMP to label (not RET/HLT) - prevents cross-scenario removal
        // ----------------------------------------------------------
        if (!did_optimize && curr->type == OP_JMP)
        {
            const char *target_label = curr->has_dst ? curr->dst_op.raw : curr->src_op.raw;
            if (!target_label || target_label[0] == '\0') {
                curr = curr->next;
                continue;
            }

            // Only apply to label targets (not registers or immediates)
            bool is_reg = (str_case_eq(target_label, "R0")  || str_case_eq(target_label, "R1")  ||
                          str_case_eq(target_label, "R2")  || str_case_eq(target_label, "R3")  ||
                          str_case_eq(target_label, "R4")  || str_case_eq(target_label, "R5")  ||
                          str_case_eq(target_label, "R6")  || str_case_eq(target_label, "R7")  ||
                          str_case_eq(target_label, "R8")  || str_case_eq(target_label, "R9")  ||
                          str_case_eq(target_label, "R10") || str_case_eq(target_label, "R11") ||
                          str_case_eq(target_label, "R12") || str_case_eq(target_label, "R13") ||
                          str_case_eq(target_label, "R14") || str_case_eq(target_label, "R15") ||
                          str_case_eq(target_label, "SP")  || str_case_eq(target_label, "BP"));
            bool is_imm = (isdigit((unsigned char)target_label[0]) ||
                          (target_label[0] == '-' && isdigit((unsigned char)target_label[1])) ||
                          (target_label[0] == '0' && target_label[1] == 'x'));

            if (!is_reg && !is_imm)
            {
                AsmNode *to_remove[256];
                int remove_count = 0;
                AsmNode *scan = curr->next;

                while (scan && remove_count < 256)
                {
                    if (scan->type == OP_LABEL)
                    {
                        char lbl_name[128];
                        get_label_name(scan, lbl_name, sizeof(lbl_name));
                        if (str_case_eq(lbl_name, target_label))
                            break; // Found target
                        else
                            break; // Found different label - STOP
                    }
                    to_remove[remove_count++] = scan;
                    scan = scan->next;
                }

                if (remove_count > 0)
                {
                    if (config.debug)
                        insert_debug_comment(curr, OPT_PEEPHOLE_JUMPS, "DEAD CODE ELIMINATED");
                    remove_with_debug(&curr->next, to_remove, remove_count, OPT_PEEPHOLE_JUMPS);
                    optimizations += remove_count;
                    did_optimize = true;
                }
            }
        }

        if (!did_optimize)
            curr = curr->next;
    }

    return optimizations;
}
