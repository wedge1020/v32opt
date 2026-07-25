#include "v32opt.h"

////////////////////////////////////////////////////////////////////////////////////////
//
// -------------------------------------------------------------------
// OPTIMIZATION CATEGORY: Peephole Optimizations
// Small-window (1-3 instruction) local transformations that improve
// code without global analysis.
// -------------------------------------------------------------------
// 
// peephole_pairs()          - adjacent instruction pair elimination
// peephole_algebra()        - algebraic simplifications
// peephole_forwarding()     - store-to-load forwarding
// peephole_jumps()          - redundant jump elimination
// peephole_movs()           - redundant MOV elimination
// peephole_immediates()     - combine immediates
// peephole_reduce()         - strength reduction
// peephole_shifts()         - shift optimizations
// peephole_dead_stores()    - dead store elimination
// peephole_loads()          - redundant load elimination
// peephole_immediate_prop() - immediate propagation
// peephole_jmp_chain()      - jump chain elimination
//
////////////////////////////////////////////////////////////////////////////////////////

// ===================================================================
// PEEPHOLE: Adjacent Instruction Pair Elimination
// Scans a sliding window of two consecutive instructions and removes
// redundant pairs:
//   - IEQ/INE followed by CIB on the same register (CIB is redundant)
//   - BNOT x; BNOT x → remove both (double negation cancels)
//   - PUSH r; POP r → remove both (no net effect)
// ===================================================================
int peephole_pairs(AsmNode *head)
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

// ===================================================================
// PEEPHOLE: Algebraic Simplification
// Removes or replaces instructions that are algebraically redundant:
//   - MOV r, r → remove (no-op)
//   - IADD/ISUB r, 0 → remove (identity)
//   - IMUL r, 2 → replace with IADD r, r (strength reduction)
// ===================================================================
int peephole_algebra(AsmNode *head)
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

        if (curr->type == OP_IMUL &&
            curr->dst_op.mode == MODE_REG &&
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

// ===================================================================
// PEEPHOLE: Store-to-Load Forwarding
// Eliminates redundant memory loads by forwarding values directly
// from stores to subsequent loads:
//   - MOV [r1], r2; MOV r3, [r1] → MOV r3, r2
// ===================================================================
int peephole_forwarding(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr && curr->next)
    {
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

// ===================================================================
// PEEPHOLE: Redundant Jump Elimination
// Removes jumps that target the immediately following label:
//   - JMP L; L: → remove JMP
// ===================================================================
int peephole_jumps(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        if (str_case_eq(curr->mnemonic, "JMP"))
        {
            AsmNode *next_node = curr->next;
            while (next_node && next_node->type == OP_OTHER &&
                  (next_node->raw[0] == '\0' || next_node->raw[0] == ';'))
            {
                next_node = next_node->next;
            }

            if (next_node && next_node->type == OP_LABEL)
            {
                char lbl[128] = {0};
                safe_str_copy(lbl, next_node->raw, sizeof(lbl));
                char *colon = strchr(lbl, ':');
                if (colon) *colon = '\0';

                if (str_case_eq(trim(lbl), trim(curr->dst_op.raw)))
                {
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

// ===================================================================
// PEEPHOLE: Redundant & Mirror Move Elimination
// Removes redundant MOV pairs:
//   - MOV r1, X; MOV r1, X → remove second MOV
//   - MOV r1, r2; MOV r2, r1 → remove second MOV (mirror)
// ===================================================================
int peephole_movs(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        if (curr->type == OP_MOV)
        {
            AsmNode *n2 = curr->next;
            while (n2 && n2->type == OP_OTHER &&
                  (n2->raw[0] == '\0' || n2->raw[0] == ';'))
            {
                n2 = n2->next;
            }
            if (!n2) break;

            if (n2->type == OP_MOV)
            {
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

// ===================================================================
// PEEPHOLE: Immediate Math Combining
// Combines consecutive arithmetic operations with immediate operands:
//   - IADD r, 5; ISUB r, 3 → IADD r, 2
//   - IADD r, 5; ISUB r, 5 → remove both
// ===================================================================
int peephole_immediates(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        if ((curr->type == OP_IADD || curr->type == OP_ISUB) &&
            curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float)
        {
            AsmNode *n2 = curr->next;
            while (n2 && n2->type == OP_OTHER &&
                  (n2->raw[0] == '\0' || n2->raw[0] == ';'))
            {
                n2 = n2->next;
            }

            if (n2 && (n2->type == OP_IADD || n2->type == OP_ISUB) &&
                n2->dst_op.mode == MODE_REG && n2->src_op.mode == MODE_IMMEDIATE && !n2->src_op.is_float &&
                str_case_eq(curr->dst_op.reg, n2->dst_op.reg))
            {
                int val1 = (curr->type == OP_IADD) ? curr->src_op.immediate : -curr->src_op.immediate;
                int val2 = (n2->type == OP_IADD) ? n2->src_op.immediate : -n2->src_op.immediate;
                int combined = val1 + val2;

                if (combined == 0)
                {
                    AsmNode *next_iter = n2->next;
                    remove_node(curr);
                    remove_node(n2);
                    curr = next_iter;
                    optimizations += 2;
                    continue;
                }
                else if (combined > 0)
                {
                    curr->type = OP_IADD;
                    safe_str_copy(curr->mnemonic, "IADD", sizeof(curr->mnemonic));
                    curr->src_op.immediate = combined;
                    snprintf(curr->src_op.raw, sizeof(curr->src_op.raw), "%d", combined);
                    snprintf(curr->raw, sizeof(curr->raw), "    IADD %s, %d", curr->dst_op.raw, combined);
                }
                else
                {
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

// ===================================================================
// PEEPHOLE: Strength Reduction
// Replaces expensive operations with cheaper equivalents:
//   - IMUL r, 0 → MOV r, 0
//   - IMUL r, 1 → remove (identity)
//   - IMUL r, 2 → IADD r, r
//   - IDIV r, 1 → remove (identity)
// ===================================================================
int peephole_reduce(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        if (curr->type == OP_IMUL && curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float)
        {
            int val = curr->src_op.immediate;

            if (val == 0)
            {
                curr->type = OP_MOV;
                safe_str_copy(curr->mnemonic, "MOV", sizeof(curr->mnemonic));
                snprintf(curr->raw, sizeof(curr->raw), "    MOV %s, 0", curr->dst_op.raw);
                optimizations++;
                curr = curr->next;
                continue;
            }

            if (val == 1)
            {
                AsmNode *to_remove = curr;
                curr = curr->next;
                remove_node(to_remove);
                optimizations++;
                continue;
            }

            if (val == 2)
            {
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

        if (curr->type == OP_IDIV && curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float)
        {
            int val = curr->src_op.immediate;

            if (val == 1)
            {
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

// ===================================================================
// PEEPHOLE: Shift Optimizations
//   - SHL/SHR r, 0 → remove (no-op)
//   - SHL r, 1 → IADD r, r (strength reduction)
// ===================================================================
// ===================================================================
// PEEPHOLE: Shift Optimizations
//   - SHL r, 0 → remove (no-op; applies to both left and right shifts)
//   - SHL r, 1 → IADD r, r (strength reduction for left shift by 1)
//   - SHL r, -1 → right shift by 1; left unchanged (no cheaper equivalent)
// ===================================================================
int peephole_shifts(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        if (curr->type == OP_SHL &&
            curr->dst_op.mode == MODE_REG &&
            curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float)
        {
            int shift = curr->src_op.immediate;
            if (shift == 0)
            {
                // SHL by 0 is a no-op (left or right shift by 0 does nothing)
                AsmNode *to_remove = curr;
                curr = curr->next;
                remove_node(to_remove);
                optimizations++;
                continue;
            }
            else if (shift == 1)
            {
                // Left shift by 1 → IADD r, r
                curr->type = OP_IADD;
                safe_str_copy(curr->mnemonic, "IADD", sizeof(curr->mnemonic));
                curr->src_op = curr->dst_op;
                snprintf(curr->src_op.raw, sizeof(curr->src_op.raw), "%s", curr->dst_op.raw);
                snprintf(curr->raw, sizeof(curr->raw), "    IADD %s, %s", curr->dst_op.raw, curr->src_op.raw);
                optimizations++;
            }
            // Note: SHL r, -1 is a right shift by 1.
            // Could optimize to IDIV r, 2 if that's cheaper on Vircon32,
            // but without knowing relative costs, we leave it as-is.
        }
        curr = curr->next;
    }
    return optimizations;
}

// ===================================================================
// PEEPHOLE: Dead Store Elimination
// Removes stores that are immediately overwritten to the same address:
//   - MOV [r1+off], r2; MOV [r1+off], r3 → remove first MOV
// ===================================================================
int peephole_dead_stores(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr && curr->next)
    {
        AsmNode *n1 = curr;
        AsmNode *n2 = curr->next;

        // Skip non-MOV or non-indirect destinations
        if (n1->type != OP_MOV || n1->dst_op.mode != MODE_INDIRECT)
        {
            curr = curr->next;
            continue;
        }

        // Skip if n2 is not a MOV with indirect destination
        if (n2->type != OP_MOV || n2->dst_op.mode != MODE_INDIRECT)
        {
            curr = curr->next;
            continue;
        }

        // Check if both stores target the same memory location
        if (str_case_eq(n1->dst_op.reg, n2->dst_op.reg) == 0 &&
            n1->dst_op.offset == n2->dst_op.offset)
        {
            remove_node(n1);
            optimizations++;
            curr = n2; // Skip n2 in next iteration (it's now the first of the pair)
            continue;
        }

        curr = curr->next;
    }
    return optimizations;
}

// ===================================================================
// PEEPHOLE: Redundant Load Elimination
// Replaces a load from an address with a register if the value was
// just loaded into another register:
//   - MOV r1, [r2+off]; MOV r3, [r2+off] → MOV r3, r1
// ===================================================================
int peephole_loads(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr && curr->next)
    {
        AsmNode *n1 = curr;
        AsmNode *n2 = curr->next;

        // Skip non-MOV or non-register destinations
        if (n1->type != OP_MOV || n1->dst_op.mode != MODE_REG)
        {
            curr = curr->next;
            continue;
        }

        // Skip if n2 is not a MOV with register destination
        if (n2->type != OP_MOV || n2->dst_op.mode != MODE_REG)
        {
            curr = curr->next;
            continue;
        }

        // Check if both loads are from the same memory location
        if (n1->src_op.mode == MODE_INDIRECT && n2->src_op.mode == MODE_INDIRECT &&
            str_case_eq(n1->src_op.reg, n2->src_op.reg) == 0 &&
            n1->src_op.offset == n2->src_op.offset)
        {
            // Replace n2's source with n1's destination register
            n2->src_op = n1->dst_op;
            snprintf(n2->raw, sizeof(n2->raw), "    MOV %s, %s", n2->dst_op.raw, n2->src_op.raw);
            optimizations++;
        }

        curr = curr->next;
    }
    return optimizations;
}

// ===================================================================
// PEEPHOLE: Immediate Propagation
// Propagates immediate values through MOV into subsequent ops:
//   - MOV r1, 5; IADD r2, r1 → IADD r2, 5
//   - MOV r1, 5; ISUB r2, r1 → ISUB r2, 5
//   - MOV r1, 5; MOV r2, r1 → MOV r2, 5
// ===================================================================
int peephole_immediate_prop(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr && curr->next)
    {
        AsmNode *n1 = curr;
        AsmNode *n2 = curr->next;

        // Skip if n1 is not MOV with immediate source
        if (n1->type != OP_MOV || n1->src_op.mode != MODE_IMMEDIATE)
        {
            curr = curr->next;
            continue;
        }

        // Check if n2 uses n1's destination register as source
        if (n2->src_op.mode == MODE_REG &&
            str_case_eq(n2->src_op.reg, n1->dst_op.reg) == 0)
        {
            // Propagate the immediate value into n2
            if (n2->type == OP_IADD || n2->type == OP_ISUB || n2->type == OP_IMUL || n2->type == OP_IDIV)
            {
                n2->src_op = n1->src_op;
                snprintf(n2->src_op.raw, sizeof(n2->src_op.raw), "%d", n2->src_op.immediate);
                snprintf(n2->raw, sizeof(n2->raw), "    %s %s, %s",
                         n2->mnemonic, n2->dst_op.raw, n2->src_op.raw);
                optimizations++;
            }
            else if (n2->type == OP_MOV)
            {
                // MOV r1, 5; MOV r2, r1 → MOV r2, 5
                n2->src_op = n1->src_op;
                snprintf(n2->src_op.raw, sizeof(n2->src_op.raw), "%d", n2->src_op.immediate);
                snprintf(n2->raw, sizeof(n2->raw), "    MOV %s, %d", n2->dst_op.raw, n2->src_op.immediate);
                optimizations++;
            }
        }

        curr = curr->next;
    }
    return optimizations;
}

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
            // Find the target label of the JMP
            AsmNode *target = curr->next;
            while (target && target->type == OP_OTHER &&
                  (target->raw[0] == '\0' || target->raw[0] == ';'))
            {
                target = target->next;
            }

            // Check if the next non-comment node is a label
            if (target && target->type == OP_LABEL)
            {
                // Check if the label's next non-comment node is another JMP
                AsmNode *next_after_label = target->next;
                while (next_after_label && next_after_label->type == OP_OTHER &&
                      (next_after_label->raw[0] == '\0' || next_after_label->raw[0] == ';'))
                {
                    next_after_label = next_after_label->next;
                }

                if (next_after_label && str_case_eq(next_after_label->mnemonic, "JMP"))
                {
                    // Replace curr's target with next_after_label's target
                    safe_str_copy(curr->dst_op.raw, next_after_label->dst_op.raw, sizeof(curr->dst_op.raw));
                    snprintf(curr->raw, sizeof(curr->raw), "    JMP %s", curr->dst_op.raw);

                    // Remove the label and the chained JMP
                    remove_node(target);
                    remove_node(next_after_label);
                    optimizations += 2;
                }
            }
        }
        curr = curr->next;
    }
    return optimizations;
}
