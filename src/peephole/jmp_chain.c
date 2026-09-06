#include "v32opt.h"

// ===================================================================
// PEEPHOLE: Jump Chain Elimination
// Eliminates indirect jump chains by making jumps direct:
//   - JMP L1; L1: JMP L2 → JMP L2 (direct jump to final destination)
//   - JMP L1; L1: JMP R0 → JMP R0 (preserve indirect if target is register)
//   - Handles chains of any length
//
// Examples:
//   JMP _label1    ->  JMP _label2
//   _label1:
//   JMP _label2
//
//   JMP _label1   ->  JMP _label3  (chained through multiple labels)
//   _label1:
//   JMP _label2
//   _label2:
//   JMP _label3
//
// NOTE: We use remove_with_debug for intermediate JMPs to properly handle
// debug comments while preserving the optimization structure.
// ===================================================================

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
                        !is_immediate_string(final_target) &&
                        trigger_allowed())
                    {
                        // Inject debug comment before mutating the instruction
                        insert_debug_comment(curr->prev, OPT_PEEPHOLE_JMP_CHAIN, curr->raw);

                        // Update curr to jump directly to the final target
                        safe_str_copy(curr->dst_op.raw, final_target, sizeof(curr->dst_op.raw));
                        snprintf(curr->raw, sizeof(curr->raw), "    JMP %s", final_target);

                        // Use remove_with_debug to properly handle the intermediate JMP
                        AsmNode *nodes[] = {next_after_label};
                        AsmNode *dummy = next_after_label;
                        if (remove_with_debug(&dummy, nodes, 1, OPT_PEEPHOLE_JMP_CHAIN)) optimizations++;
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
