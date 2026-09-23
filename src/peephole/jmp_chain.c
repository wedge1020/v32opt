#include "v32opt.h"

// ===================================================================
// PEEPHOLE: Jump Chain Elimination
// Eliminates indirect jump chains by making jumps direct:
//   - JMP L1; L1: JMP L2 → JMP L2 (direct jump to final destination)
//   - JMP L1; L1: JMP R0 → JMP R0 (preserve indirect if target is register)
//   - Handles adjacent chains of any length (a later fixed-point
//     iteration retargets the just-rewritten jump again if it now sits
//     adjacent to its new target label)
//
// Examples:
//   JMP _label1    ->  JMP _label2
//   _label1:
//   JMP _label2
//
//   JMP _label1   ->  JMP _label3  (chained through multiple labels,
//   _label1:                        one hop per iteration, when each
//   JMP _label2                    jump is adjacent to its target)
//   _label2:
//   JMP _label3
//
// BUG FIX (intermediate jump removal): the intermediate "JMP L2" at
// label L1 is only removed when NOTHING else can ever reach L1:
//   (a) no other JMP/JT/JF/CALL in the whole file targets L1 (this
//       jump, being retargeted, no longer counts), and
//   (b) nothing can FALL INTO L1 -- i.e. the instruction physically
//       preceding the label is itself an unconditional transfer
//       (JMP/RET/HLT) or the label starts the file.
// Before this guard the intermediate JMP was deleted unconditionally,
// so any other branch still aiming at L1 fell through into whatever
// code followed it. Reproduction:
//     JT R0, mid
//     JMP mid
//   mid:
//     JMP L2
//     MOV R9, 77      ; must stay unreachable via mid
//   L2: ...
// used to lose its intermediate JMP, letting "JT R0, mid" run the MOV.
//
// TRIGGER CAP: this whole transform (retarget + optional intermediate
// removal) is ONE atomic unit and consumes exactly one trigger slot.
// It used to burn two (one explicit check plus one inside
// remove_with_debug), which skewed --trigger-max bisection by one.
// ===================================================================

// May anything other than 'except' reach label 'lbl' (by name)?
static bool label_has_other_users(AsmNode *head, const char *lbl, AsmNode *except) {
    for (AsmNode *n = head->next; n != NULL; n = n->next) {
        if (n == except || n->type == OP_OTHER || n->type == OP_LABEL) continue;
        if (n->type == OP_JMP || n->type == OP_JT || n->type == OP_JF || n->type == OP_CALL) {
            const char *tgt = (n->type == OP_JMP || n->type == OP_CALL)
                ? (n->has_dst ? n->dst_op.raw : n->src_op.raw)
                : n->src_op.raw;
            if (tgt && tgt[0] != '\0') {
                char tbuf[128];
                safe_str_copy(tbuf, tgt, sizeof(tbuf));
                if (str_case_eq(trim(tbuf), lbl)) return true;
            }
        }
    }
    return false;
}

// Can anything fall INTO the node 'label' from the preceding code?
// Only if the physically preceding real instruction is NOT itself an
// unconditional transfer (JMP/RET/HLT), and the label isn't at file
// start. (Directives and comments are not executable, so they are
// skipped when looking for the preceding instruction.)
static bool label_reachable_by_fallthrough(AsmNode *label) {
    AsmNode *prev = label->prev;
    while (prev && prev->type == OP_OTHER) prev = prev->prev;
    if (!prev) return false;   // nothing executable precedes it
    return !(prev->type == OP_JMP || prev->type == OP_RET || prev->type == OP_HLT);
}

int peephole_jmp_chain(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr && curr->next)
    {
        // Only process JMP instructions
        if (str_case_eq(curr->mnemonic, "JMP"))
        {
            // Skip over comments/blank lines to find the target label
            AsmNode *target = skip_other_nodes(curr->next);

            // ----------------------------------------------------------
            // PATTERN: Jump to Label Followed by Another Jump
            // JMP L1; L1: JMP L2 → JMP L2
            // ----------------------------------------------------------
            if (target && target->type == OP_LABEL)
            {
                // Get the label name without colon
                char lbl_name[128];
                get_label_name(target, lbl_name, sizeof(lbl_name));

                // Verify the JMP targets this specific label
                char *jmp_target = curr->has_dst ? curr->dst_op.raw : curr->src_op.raw;
                if (!str_case_eq(trim(jmp_target), lbl_name))
                {
                    curr = curr->next;
                    continue;
                }

                // Find the instruction after the label (skip comments)
                AsmNode *next_after_label = skip_other_nodes(target->next);

                if (next_after_label && str_case_eq(next_after_label->mnemonic, "JMP"))
                {
                    // Check if the second JMP targets a label
                    char *final_target = next_after_label->has_dst ?
                        next_after_label->dst_op.raw : next_after_label->src_op.raw;

                    // Only chain if the second JMP targets a label (not a register or immediate)
                    if (final_target && final_target[0] != '\0' &&
                        !is_register_operand(final_target) &&
                        !is_immediate_string(final_target))
                    {
                        // TRIGGER CAP: one atomic transform = one slot.
                        // If the budget is exhausted, leave everything
                        // exactly as found (same as no match).
                        if (!trigger_allowed()) {
                            curr = curr->next;
                            continue;
                        }

                        // Inject debug comment before mutating the instruction
                        insert_debug_comment(curr->prev, OPT_PEEPHOLE_JMP_CHAIN, curr->raw);

                        // Update curr to jump directly to the final target
                        safe_str_copy(curr->dst_op.raw, final_target, sizeof(curr->dst_op.raw));
                        snprintf(curr->raw, sizeof(curr->raw), "    JMP %s", final_target);

                        // The retarget itself is always sound. Removing the
                        // intermediate JMP is sound ONLY if nothing else can
                        // reach its label (see the header comment); keep it
                        // otherwise -- it still routes everyone correctly.
                        bool remove_intermediate =
                            !label_has_other_users(head, lbl_name, curr) &&
                            !label_reachable_by_fallthrough(target);

                        if (remove_intermediate) {
                            if (config.debug) {
                                insert_debug_comment(next_after_label->prev,
                                                     OPT_PEEPHOLE_JMP_CHAIN,
                                                     next_after_label->raw);
                            }
                            remove_node(next_after_label);
                        }
                        optimizations++;
                        // Don't advance curr - we might be able to chain further
                        continue;
                    }
                }
            }
        }

        curr = curr->next;
    }

    return optimizations;
}
