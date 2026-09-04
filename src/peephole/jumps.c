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
                AsmNode *scan = curr->next;

                while (scan && remove_count < 256)
                {
                    if (scan->type == OP_LABEL)
                    {
                        char lbl_name[128];
                        get_label_name(scan, lbl_name, sizeof(lbl_name));
                        if (str_case_eq(lbl_name, target_label)) break;
                        else break;
                    }

                    to_remove[remove_count++] = scan;
                    scan = scan->next;
                }

                if (remove_count > 0)
                {
                    // TRIGGER CAP: check up front, before writing the banner
                    // comment below -- if the budget is exhausted this whole
                    // elimination is skipped, so nothing about it (comment
                    // included) should appear either. remove_with_debug()
                    // itself still re-checks the cap (it's the shared choke
                    // point for every pass), but bailing here too avoids
                    // committing the banner comment for a removal that then
                    // doesn't happen.
                    if (trigger_allowed()) {
                        // Refund the slot remove_with_debug() is about to
                        // consume again for the SAME group of nodes -- this
                        // call already spent one just to decide "yes, do
                        // this whole group", and remove_with_debug() spends
                        // its own on top of that; without the refund a
                        // single dead-code-elimination group would cost 2
                        // triggers instead of 1, breaking the "Nth transform"
                        // bisection this flag exists for.
                        g_trigger_count--;

                        // This custom structural comment is fine to keep as a banner
                        insert_debug_comment(curr, OPT_PEEPHOLE_JUMPS, "DEAD CODE ELIMINATED");
                        // Unlike the peephole/movs.c call sites, this one genuinely
                        // needs curr->next updated to the real next_after: did_optimize
                        // is true here, so the outer loop does NOT advance curr, and
                        // on the very next iteration pattern-3 re-scans starting at
                        // curr->next. If that scan re-discovers the just-inserted
                        // debug comments (because curr->next still threads through
                        // them instead of skipping to real code), it would re-wrap
                        // them in new comments forever -- a reproduced infinite loop.
                        // So the write-through IS required here; do it explicitly,
                        // after remove_with_debug() has returned (no race with its
                        // internal splice), and fix up BOTH directions of the link --
                        // the original code only ever wrote curr->next, never
                        // resume->prev, which is exactly the asymmetry described in
                        // tools.c.patch.c.
                        AsmNode *resume;
                        remove_with_debug(&resume, to_remove, remove_count, OPT_PEEPHOLE_JUMPS);
                        curr->next = resume;
                        if (resume) resume->prev = curr;
                        optimizations += remove_count;
                        did_optimize = true;
                    }
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
