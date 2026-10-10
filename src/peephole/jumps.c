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

        // --- PATTERN 1: Redundant Jump to Next Label ---
        if (!did_optimize && (curr->type == OP_JMP || curr->type == OP_JT || curr->type == OP_JF))
        {
            const char *target_label = (curr->type == OP_JMP)
                ? (curr->has_dst ? curr->dst_op.raw : curr->src_op.raw)
                : curr->src_op.raw;

            if (target_label && target_label[0] != '\0')
            {
                AsmNode *next_non_comment = skip_other_nodes(curr->next);

                if (next_non_comment && next_non_comment->type == OP_LABEL)
                {
                    char lbl_name[128];
                    get_label_name(next_non_comment, lbl_name, sizeof(lbl_name));

                    if (str_case_eq(lbl_name, target_label))
                    {
                        // Removed redundant debug comment insertion here
                        AsmNode *nodes[] = {curr};
                        if (remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_JUMPS)) optimizations++;
                        did_optimize = true;
                    }
                }
            }
        }

        // --- PATTERN 2: Branch Over Jump (Condition Inversion) ---
        if (!did_optimize && (curr->type == OP_JT || curr->type == OP_JF))
        {
            const char *branch_target = curr->src_op.raw;
            AsmNode *next_jmp = skip_other_nodes(curr->next);

            if (next_jmp && next_jmp->type == OP_JMP)
            {
                const char *jmp_target = next_jmp->has_dst ? next_jmp->dst_op.raw : next_jmp->src_op.raw;
                AsmNode *next_lbl = skip_other_nodes(next_jmp->next);

                if (next_lbl && next_lbl->type == OP_LABEL && jmp_target && jmp_target[0] != '\0')
                {
                    char lbl_name[128];
                    get_label_name(next_lbl, lbl_name, sizeof(lbl_name));

                    if (str_case_eq(lbl_name, branch_target))
                    {
                        // BUG FIX: jmp_target previously pointed directly into
                        // next_jmp->dst_op.raw / src_op.raw. remove_with_debug()
                        // below frees next_jmp (via remove_node()), so using
                        // jmp_target AFTER that call is a use-after-free --
                        // confirmed by a real corrupted-output report ("JF R2,"
                        // with garbage/blank trailing it, i.e. reading freed
                        // heap memory). Snapshot it into a local buffer first,
                        // independent of next_jmp's lifetime.
                        char jmp_target_copy[128];
                        safe_str_copy(jmp_target_copy, jmp_target, sizeof(jmp_target_copy));

                        // TRIGGER CAP: this inversion is NOT safe to split --
                        // if curr is inverted but next_jmp is left behind,
                        // the true-condition path falls straight into that
                        // still-present unconditional JMP instead of
                        // falling through to branch_target's label.
                        // Removal must commit before curr is touched.
                        AsmNode *nodes[] = {next_jmp};
                        AsmNode *dummy = next_jmp;
                        if (remove_with_debug(&dummy, nodes, 1, OPT_PEEPHOLE_JUMPS)) {
                            // Keep this comment; curr is being mutated, not removed
                            insert_debug_comment(curr->prev, OPT_PEEPHOLE_JUMPS, curr->raw);

                            if (curr->type == OP_JT) {
                                curr->type = OP_JF;
                                strcpy(curr->mnemonic, "JF");
                            } else {
                                curr->type = OP_JT;
                                strcpy(curr->mnemonic, "JT");
                            }

                            safe_str_copy(curr->src_op.raw, jmp_target_copy, sizeof(curr->src_op.raw));
                            snprintf(curr->raw, sizeof(curr->raw), "    %s %s, %s",
                                     curr->mnemonic, curr->dst_op.raw, jmp_target_copy);

                            optimizations++;
                            did_optimize = true;
                        }
                    }
                }
            }
        }

        // --- PATTERN 3: Unreachable Code Elimination ---
        if (!did_optimize && curr->type == OP_JMP)
        {
            const char *target_label = curr->has_dst ? curr->dst_op.raw : curr->src_op.raw;
            if (!target_label || target_label[0] == '\0') {
                curr = curr->next;
                continue;
            }

            if (!is_register_operand(target_label) && !is_immediate_string(target_label))
            {
                AsmNode *to_remove[256];
                int remove_count = 0;
                int instr_count  = 0;
                AsmNode *scan = curr->next;

                while (scan && remove_count < 256)
                {
                    // The dead range ends at the next label: that is where
                    // control can enter again.
                    if (scan->type == OP_LABEL) break;

                    // BUG FIX: stop at assembler directives. Only real
                    // instructions can be "unreachable"; a directive is not
                    // executed at all, so control flow says nothing about
                    // it. The scan used to sweep everything up to the next
                    // label, so a "%define" block sitting after a JMP was
                    // deleted as dead code, leaving every later use of
                    // those symbols undefined. Seen in celeste.asm:
                    // "JMP __ipairs_iter_error" followed by the
                    // PICO8_* %defines -> assembler error "expected basic
                    // value" at the first PICO8_SWATCH_REGION_BASE use.
                    // The same applies to %include and to unlabeled data
                    // (integer/float/string/pointer/datafile), which may
                    // be addressed relative to an earlier label.
                    // (remove_node() also refuses to delete directives,
                    // but stopping here matters: the group below must be
                    // exactly the nodes that really get unlinked.)
                    if (is_directive_node(scan)) break;

                    // -d annotations (this pass's own markers from an
                    // earlier round, or another pass's) are not program
                    // text: step over them and leave them where they are.
                    // Collecting them used to re-wrap them in new markers
                    // on every visit -- an infinite loop -- which had been
                    // "fixed" by splicing them out of the list unfreed, so
                    // -d output never showed the dead-code markers.
                    if (scan->debug_note) {
                        scan = scan->next;
                        continue;
                    }

                    // Ordinary comments and blank lines in the dead range
                    // are swept with it (as they always have been).
                    to_remove[remove_count++] = scan;
                    if (scan->type != OP_OTHER) instr_count++;
                    scan = scan->next;
                }

                // TRIGGER CAP: the whole group is ONE transform, gated by
                // exactly one trigger slot; if the budget is exhausted the
                // code is left exactly as found (no banner, no removal).
                if (remove_count > 0 && trigger_allowed())
                {
                    // One banner per eliminated group, directly after the
                    // JMP, then a marker in place of each removed
                    // instruction (removed comments/blank lines get none --
                    // a marker for a blank line would just be noise).
                    if (instr_count > 0) {
                        insert_debug_comment(curr, OPT_PEEPHOLE_JUMPS, "DEAD CODE ELIMINATED");
                    }
                    for (int i = 0; i < remove_count; i++) {
                        if (to_remove[i]->type != OP_OTHER) {
                            insert_debug_comment(to_remove[i]->prev, OPT_PEEPHOLE_JUMPS,
                                                 to_remove[i]->raw);
                        }
                        remove_node(to_remove[i]);
                    }
                    optimizations += remove_count;

                    // curr is NOT advanced (did_optimize): with the dead code
                    // gone, pattern 1 may now apply ("JMP L / L:"). The
                    // re-scan of pattern 3 finds nothing but the -d markers
                    // just inserted (stepped over above), so it terminates.
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
