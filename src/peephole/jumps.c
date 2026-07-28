#include "v32opt.h"

// ===================================================================
// PEEPHOLE: Jump Optimizations
//
// Handles three patterns for optimizing control flow:
//
// PATTERN 1: Redundant Jump to Next Label
//   - Removes JMP/JT/JF when target label is immediately next
//   - Handles comments/blanks between jump and label
//   Example: JMP _L1; _L1: → (JMP removed)
//
// PATTERN 2: Branch Over Jump (Condition Inversion)
//   - Transforms JF R1, L1; JMP L2; L1: → JT R1, L2; L1:
//   - Handles comments/blanks between instructions
//   Example: JF R0, _else; JMP _then; _else: → JT R0, _then; _else:
//
// PATTERN 3: Unreachable Code Elimination (JMP to label only)
//   - Removes code between JMP and its target label
//   - Does NOT apply to indirect jumps (JMP R0) or immediate jumps (JMP 0x1000)
//   - Does NOT apply to RET/HLT (prevents cross-scenario removal)
//   Example: JMP _L8; MOV R1, 99; MOV R2, 100; _L8: → JMP _L8; _L8:
//
// Returns: Number of optimizations applied
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
        // Eliminates JMP/JT/JF where the target label follows,
        // possibly with comments/blanks in between.
        // ----------------------------------------------------------
        if (!did_optimize && (curr->type == OP_JMP || curr->type == OP_JT || curr->type == OP_JF))
        {
            const char *target_label = (curr->type == OP_JMP)
                ? (curr->has_dst ? curr->dst_op.raw : curr->src_op.raw)
                : curr->src_op.raw;

            if (target_label && target_label[0] != '\0')
            {
                // Skip OP_OTHER nodes to find the next non-comment node
                AsmNode *next_non_comment = skip_other_nodes(curr->next);

                // Check if the next non-comment node is the target label
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
        // PATTERN 2: Branch Over Jump (Condition Inversion)
        // Transforms:
        //     JF R1, __else_label
        //     JMP __end_label
        //     __else_label:
        // Into:
        //     JT R1, __end_label
        //     __else_label:
        // ----------------------------------------------------------
        if (!did_optimize && (curr->type == OP_JT || curr->type == OP_JF))
        {
            const char *branch_target = curr->src_op.raw;
            AsmNode *next_jmp = skip_other_nodes(curr->next);

            // Next non-comment instruction MUST be an unconditional jump
            if (next_jmp && next_jmp->type == OP_JMP)
            {
                const char *jmp_target = next_jmp->has_dst ? next_jmp->dst_op.raw : next_jmp->src_op.raw;
                AsmNode *next_lbl = skip_other_nodes(next_jmp->next);

                // The instruction after the JMP MUST be the branch target label
                if (next_lbl && next_lbl->type == OP_LABEL && jmp_target && jmp_target[0] != '\0')
                {
                    char lbl_name[128];
                    get_label_name(next_lbl, lbl_name, sizeof(lbl_name));

                    if (str_case_eq(lbl_name, branch_target))
                    {
                        // Inject debug comment prior to mutating the instruction
                        insert_debug_comment(curr->prev, OPT_PEEPHOLE_JUMPS, curr->raw);

                        // Invert conditional jump mnemonic and node type
                        if (curr->type == OP_JT) {
                            curr->type = OP_JF;
                            strcpy(curr->mnemonic, "JF");
                        } else {
                            curr->type = OP_JT;
                            strcpy(curr->mnemonic, "JT");
                        }

                        // Rewrite target operand to point directly to the JMP's destination
                        safe_str_copy(curr->src_op.raw, jmp_target, sizeof(curr->src_op.raw));
                        snprintf(curr->raw, sizeof(curr->raw), "    %s %s, %s",
                                 curr->mnemonic, curr->dst_op.raw, jmp_target);

                        // Remove the redundant unconditional JMP instruction
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
        // After JMP to label: remove code until target label ONLY if no
        //   other labels are encountered first.
        // Does NOT apply to JMP Rn or JMP immediate.
        // Does NOT apply to RET/HLT (prevents cross-scenario removal)
        // ----------------------------------------------------------
        if (!did_optimize && curr->type == OP_JMP)
        {
            const char *target_label = curr->has_dst ? curr->dst_op.raw : curr->src_op.raw;
            if (!target_label || target_label[0] == '\0') {
                curr = curr->next;
                continue;
            }

            // Only apply to label targets (not registers or immediates)
            if (!is_register_operand(target_label) && !is_immediate_string(target_label))
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
                        {
                            break; // Found target, stop here (don't remove target label)
                        }
                        else
                        {
                            break; // Found DIFFERENT label, stop here (don't remove it)
                        }
                    }

                    to_remove[remove_count++] = scan;
                    scan = scan->next;
                }

                if (remove_count > 0)
                {
                    insert_debug_comment(curr, OPT_PEEPHOLE_JUMPS, "DEAD CODE ELIMINATED");
                    remove_with_debug(&curr->next, to_remove, remove_count, OPT_PEEPHOLE_JUMPS);
                    optimizations += remove_count;
                    did_optimize = true;
                }
            }
        }

        if (!did_optimize)
        {
            curr = curr->next;
        }
    }

    return optimizations;
}
