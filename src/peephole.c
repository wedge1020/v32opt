#include "v32opt.h"

// -------------------------------------------------------------------
// OPTIMIZATION CATEGORY: Local Peephole Passes
// -------------------------------------------------------------------

int  pass_peephole_window2 (AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr && curr->next)
	{
        AsmNode *n1 = curr;
        AsmNode *n2 = curr->next;

        if ((n1->type == OP_IEQ || n1->type == OP_INE) && 
             n2->type == OP_CIB && 
             n1->dst_op.mode == MODE_REG && n2->dst_op.mode == MODE_REG &&
             str_case_eq(n1->dst_op.reg, n2->dst_op.reg) == 0) 
        {
            remove_node(n2);
            optimizations++;
            continue;
        }

        if (n1->type == OP_BNOT && n2->type == OP_BNOT && 
            n1->dst_op.mode == MODE_REG && n2->dst_op.mode == MODE_REG &&
            str_case_eq(n1->dst_op.reg, n2->dst_op.reg) == 0) 
        {
            AsmNode *next_iter = n2->next;
            remove_node(n1);
            remove_node(n2);
            curr = next_iter;
            optimizations += 2;
            continue;
        }

        if (n1->type == OP_PUSH && n2->type == OP_POP && 
            n1->dst_op.mode == MODE_REG && n2->dst_op.mode == MODE_REG &&
            str_case_eq(n1->dst_op.reg, n2->dst_op.reg) == 0) 
        {
            AsmNode *next_iter = n2->next;
            remove_node(n1);
            remove_node(n2);
            curr = next_iter;
            optimizations += 2;
            continue;
        }

        curr = curr->next;
    }

    return optimizations;
}

int  pass_algebraic_simplifications (AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr != NULL)
	{
        AsmNode *next = curr->next;

        if (curr->type == OP_MOV && 
            curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_REG && 
            str_case_eq(curr->dst_op.reg, curr->src_op.reg) == 0) 
        {
            remove_node(curr);
            optimizations++;
            curr = next;
            continue;
        }

        if ((curr->type == OP_IADD || curr->type == OP_ISUB) && 
            curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float && curr->src_op.immediate == 0) 
        {
            remove_node(curr);
            optimizations++;
            curr = next;
            continue;
        }

        // Add this check:
        if (curr->type == OP_IMUL &&
            curr->dst_op.mode == MODE_REG &&  // <-- MISSING
            curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float &&
            curr->src_op.immediate == 2)
        {
            curr->type = OP_IADD;
            strcpy(curr->mnemonic, "IADD");
            curr->src_op = curr->dst_op;
            snprintf(curr->raw, sizeof(curr->raw), "    IADD %s, %s", curr->dst_op.raw, curr->src_op.raw);
            optimizations++;
        }

        curr = next;
    }

    return optimizations;
}

int pass_store_to_load_forwarding(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr && curr->next) {
        AsmNode *n1 = curr;
        AsmNode *n2 = curr->next;

        if (n1->type == OP_MOV && n2->type == OP_MOV &&
            n1->dst_op.mode == MODE_INDIRECT && n1->src_op.mode == MODE_REG &&
            n2->dst_op.mode == MODE_REG      && n2->src_op.mode == MODE_INDIRECT) 
        {
            if (str_case_eq(n1->dst_op.reg, n2->src_op.reg) == 0 && 
                n1->dst_op.offset == n2->src_op.offset) 
            {
                n2->src_op = n1->src_op;
                snprintf(n2->raw, sizeof(n2->raw), "    MOV %s, %s", n2->dst_op.reg, n2->src_op.reg);
                optimizations++;
            }
        }

        curr = curr->next;
    }

    return optimizations;
}

// -------------------------------------------------------------------
// Pass: Redundant Jump Elimination
// -------------------------------------------------------------------
int pass_redundant_jumps(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr) {
        if (str_case_eq(curr->mnemonic, "JMP")) {
            // Find the next actual instruction or label (skip comments/blanks)
            AsmNode *next_node = curr->next;
            while (next_node && next_node->type == OP_OTHER && 
                  (next_node->raw[0] == '\0' || next_node->raw[0] == ';')) {
                next_node = next_node->next;
            }

            if (next_node && next_node->type == OP_LABEL) {
                char lbl[128] = {0};
                safe_str_copy(lbl, next_node->raw, sizeof(lbl));
                char *colon = strchr(lbl, ':');
                if (colon) *colon = '\0';
                
                if (str_case_eq(trim(lbl), trim(curr->dst_op.raw))) {
                    AsmNode *to_remove = curr;
                    curr = curr->next;
                    remove_node(to_remove);
                    optimizations++;
                    continue;
                }
            }
        }
        curr = curr->next;
    }
    return optimizations;
}

// -------------------------------------------------------------------
// Pass: Redundant & Mirror Move Elimination
// -------------------------------------------------------------------
int pass_redundant_movs(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr) {
        if (curr->type == OP_MOV) {
            AsmNode *n2 = curr->next;
            while (n2 && n2->type == OP_OTHER && 
                  (n2->raw[0] == '\0' || n2->raw[0] == ';')) {
                n2 = n2->next;
            }
            if (!n2) break;

            if (n2->type == OP_MOV) {
                // Case 1: Duplicate Move (e.g., MOV R0, X followed by MOV R0, X)
                //
                // The two instructions are only safely removable when the
                // source doesn't depend on the register the first
                // instruction just wrote - i.e. skip this whenever the
                // source is an indirect operand whose base register is the
                // same register being written.
                bool self_referential_load =
                    (curr->src_op.mode == MODE_INDIRECT) &&
                    str_case_eq(curr->src_op.reg, curr->dst_op.reg);

                if (!self_referential_load &&
                    str_case_eq(curr->dst_op.raw, n2->dst_op.raw) &&
                    str_case_eq(curr->src_op.raw, n2->src_op.raw)) 
                {
                    remove_node(n2);
                    optimizations++;
                    continue;
                }

                // Case 2: Mirror Move (e.g., MOV R0, R1 followed by MOV R1, R0)
                if (curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_REG &&
                    n2->dst_op.mode == MODE_REG && n2->src_op.mode == MODE_REG) 
                {
                    if (str_case_eq(curr->dst_op.reg, n2->src_op.reg) &&
                        str_case_eq(curr->src_op.reg, n2->dst_op.reg)) 
                    {
                        remove_node(n2);
                        optimizations++;
                        continue;
                    }
                }
            }
        }
        curr = curr->next;
    }
    return optimizations;
}

// -------------------------------------------------------------------
// Pass: Immediate Math Combining
// -------------------------------------------------------------------
int pass_combine_immediates(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr) {
        if ((curr->type == OP_IADD || curr->type == OP_ISUB) &&
            curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float) 
        {
            AsmNode *n2 = curr->next;
            while (n2 && n2->type == OP_OTHER && 
                  (n2->raw[0] == '\0' || n2->raw[0] == ';')) {
                n2 = n2->next;
            }

            if (n2 && (n2->type == OP_IADD || n2->type == OP_ISUB) &&
                n2->dst_op.mode == MODE_REG && n2->src_op.mode == MODE_IMMEDIATE && !n2->src_op.is_float &&
                str_case_eq(curr->dst_op.reg, n2->dst_op.reg)) 
            {
                int val1 = (curr->type == OP_IADD) ? curr->src_op.immediate : -curr->src_op.immediate;
                int val2 = (n2->type == OP_IADD) ? n2->src_op.immediate : -n2->src_op.immediate;
                int combined = val1 + val2;

                if (combined == 0) {
                    // They canceled each other out completely!
                    AsmNode *next_iter = n2->next;
                    remove_node(curr);
                    remove_node(n2);
                    curr = next_iter;
                    optimizations += 2;
                    continue;
                } else if (combined > 0) {
                    curr->type = OP_IADD;
                    safe_str_copy(curr->mnemonic, "IADD", sizeof(curr->mnemonic));
                    curr->src_op.immediate = combined;
                    snprintf(curr->src_op.raw, sizeof(curr->src_op.raw), "%d", combined);
                    snprintf(curr->raw, sizeof(curr->raw), "    IADD %s, %d", curr->dst_op.raw, combined);
                } else {
                    curr->type = OP_ISUB;
                    safe_str_copy(curr->mnemonic, "ISUB", sizeof(curr->mnemonic));
                    curr->src_op.immediate = -combined;
                    snprintf(curr->src_op.raw, sizeof(curr->src_op.raw), "%d", -combined);
                    snprintf(curr->raw, sizeof(curr->raw), "    ISUB %s, %d", curr->dst_op.raw, -combined);
                }
                remove_node(n2);
                optimizations++;
                continue;
            }
        }
        curr = curr->next;
    }
    return optimizations;
}

// -------------------------------------------------------------------
// Pass: Strength Reduction (Multiplication & Division)
// -------------------------------------------------------------------
int pass_strength_reduction(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr) {
        // --- 1. Integer Multiplication (IMUL) ---
        if (curr->type == OP_IMUL && curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float) {
            int val = curr->src_op.immediate;

            // Case A: Multiply by 0 -> Replace with MOV dst, 0
            if (val == 0) {
                curr->type = OP_MOV;
                safe_str_copy(curr->mnemonic, "MOV", sizeof(curr->mnemonic));
                snprintf(curr->raw, sizeof(curr->raw), "    MOV %s, 0", curr->dst_op.raw);
                optimizations++;
                curr = curr->next;
                continue;
            }

            // Case B: Multiply by 1 -> Identity operation (Remove completely!)
            if (val == 1) {
                AsmNode *to_remove = curr;
                curr = curr->next;
                remove_node(to_remove);
                optimizations++;
                continue;
            }

            // Case C: Multiply by 2 -> Replace with IADD dst, dst
            if (val == 2) {
                curr->type = OP_IADD;
                safe_str_copy(curr->mnemonic, "IADD", sizeof(curr->mnemonic));
                curr->src_op.mode = MODE_REG;
                safe_str_copy(curr->src_op.reg, curr->dst_op.reg, sizeof(curr->src_op.reg));
                safe_str_copy(curr->src_op.raw, curr->dst_op.raw, sizeof(curr->src_op.raw));
                snprintf(curr->raw, sizeof(curr->raw), "    IADD %s, %s", curr->dst_op.raw, curr->dst_op.raw);
                optimizations++;
                curr = curr->next;
                continue;
            }
        }

        // --- 2. Integer Division (IDIV) ---
        if (curr->type == OP_IDIV && curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float) {
            int val = curr->src_op.immediate;

            // Case A: Divide by 1 -> Identity operation (Remove completely!)
            if (val == 1) {
                AsmNode *to_remove = curr;
                curr = curr->next;
                remove_node(to_remove);
                optimizations++;
                continue;
            }
        }

        curr = curr->next;
    }
    return optimizations;
}
