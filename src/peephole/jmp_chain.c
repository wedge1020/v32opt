#include "v32opt.h"

// ===================================================================
// PEEPHOLE: Jump Chain Elimination
// Chains consecutive jumps to avoid indirection:
//   - JMP L1; L1: JMP L2 → JMP L2
// ===================================================================
int peephole_jmp_chain(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr && curr->next)
    {
        if (str_case_eq(curr->mnemonic, "JMP"))
        {
            // Skip over comments/blank lines to find the target label
            AsmNode *target = curr->next;
            while (target && target->type == OP_OTHER &&
                  (target->raw[0] == '\0' || target->raw[0] == ';'))
            {
                target = target->next;
            }

            // --- Jump to Label Followed by Another Jump ---
            if (target && target->type == OP_LABEL)
            {
                // Extract and verify the JMP's target matches this label
                char lbl[128] = {0};
                safe_str_copy(lbl, target->raw, sizeof(lbl));
                char *colon = strchr(lbl, ':');
                if (colon) *colon = '\0';
                trim(lbl);

                // Only chain if JMP targets this specific label
                if (!str_case_eq(trim(curr->dst_op.raw), lbl)) {
                    curr = curr->next;
                    continue;
                }

                // Find the instruction after the label (skip comments)
                AsmNode *next_after_label = target->next;
                while (next_after_label && next_after_label->type == OP_OTHER &&
                      (next_after_label->raw[0] == '\0' || next_after_label->raw[0] == ';'))
                {
                    next_after_label = next_after_label->next;
                }

                if (next_after_label && str_case_eq(next_after_label->mnemonic, "JMP"))
                {
                    // Inject debug comment before modifying the instruction
                    insert_debug_comment(curr->prev, OPT_PEEPHOLE_JMP_CHAIN, curr->raw);

                    // Update curr to jump directly to next_after_label's target
                    safe_str_copy(curr->dst_op.raw, next_after_label->dst_op.raw, sizeof(curr->dst_op.raw));
                    snprintf(curr->raw, sizeof(curr->raw), "    JMP %s", curr->dst_op.raw);

                    // Remove the redundant intermediate JMP using standard debug removal
                    AsmNode *nodes[] = {next_after_label};
                    AsmNode *dummy = next_after_label;
                    remove_with_debug(&dummy, nodes, 1, OPT_PEEPHOLE_JMP_CHAIN);

                    optimizations++;
                }
            }
        }
        curr = curr->next;
    }
    return optimizations;
}
