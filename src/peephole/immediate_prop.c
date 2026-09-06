#include "v32opt.h"

// ===================================================================
// PEEPHOLE: Immediate Propagation
// Propagates immediate values through registers:
//   - MOV R1, 42; ... OP R2, R1 → OP R2, 42 (if R1 not modified)
//   - IADD R1, 10; ISUB R2, R1 → ISUB R2, 10 (if R1 not modified)
//   - Handles arithmetic operations with immediate operands
//
// Examples:
//   MOV R1, 42    ->  (kept)
//   IADD R2, R1   ->  IADD R2, 42  (R1 replaced with immediate 42)
// ===================================================================
// ===================================================================
// PEEPHOLE: Immediate Propagation & Folding
// Handles:
//   1. Identity math elimination (IADD R, 0, IMUL R, 1, etc.)
//   2. Constant folding (MOV R1, val1 + ALU R1, val2 -> MOV R1, new_val)
//   3. Sequential math combining (ALU R1, val1 + ALU R1, val2 -> combined ALU)
// ===================================================================

int peephole_immediate_prop(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr != NULL)
    {
        AsmNode *next = curr->next;

        // ----------------------------------------------------------
        // PATTERN 1: Identity Math Elimination
        // ----------------------------------------------------------
        if ((curr->type == OP_IADD || curr->type == OP_ISUB) &&
            curr->dst_op.mode == MODE_REG &&
            is_numeric_immediate(&curr->src_op) && !curr->src_op.is_float &&
            curr->src_op.immediate == 0)
        {
            AsmNode *nodes[] = {curr};
            if (remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_IMMEDIATE_PROP)) optimizations++;
            continue;
        }

        if ((curr->type == OP_IMUL || curr->type == OP_IDIV) &&
            curr->dst_op.mode == MODE_REG &&
            is_numeric_immediate(&curr->src_op) && !curr->src_op.is_float &&
            curr->src_op.immediate == 1)
        {
            AsmNode *nodes[] = {curr};
            if (remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_IMMEDIATE_PROP)) optimizations++;
            continue;
        }

        // ----------------------------------------------------------
        // PATTERN 2: Constant Folding (MOV + ALU)
        // ----------------------------------------------------------
        if (curr->type == OP_MOV && curr->dst_op.mode == MODE_REG &&
            is_numeric_immediate(&curr->src_op) && !curr->src_op.is_float)
        {
            char *def_reg = curr->dst_op.reg;
            if (!str_case_eq(def_reg, "SP") && !str_case_eq(def_reg, "BP"))
            {
                AsmNode *scan = curr->next;
                bool folded = false;

                while (scan && !is_control_flow_boundary(scan))
                {
                    if (scan->type == OP_OTHER) {
                        scan = scan->next;
                        continue;
                    }

                    // Check for foldable ALU instruction first (before modifies_register check)
                    if ((scan->type == OP_IADD || scan->type == OP_ISUB || scan->type == OP_IMUL) &&
                        scan->dst_op.mode == MODE_REG && str_case_eq(scan->dst_op.reg, def_reg) &&
                        is_numeric_immediate(&scan->src_op) && !scan->src_op.is_float)
                    {
                        long v1 = curr->src_op.immediate;
                        long v2 = scan->src_op.immediate;
                        long result = 0;

                        if (scan->type == OP_IADD) result = v1 + v2;
                        else if (scan->type == OP_ISUB) result = v1 - v2;
                        else if (scan->type == OP_IMUL) result = v1 * v2;

                        // TRIGGER CAP: this fold is one atomic transform --
                        // curr's rewrite and scan's removal must both happen
                        // or neither must, or curr would end up "pre-folded"
                        // while the original ALU instruction that supplied
                        // the folded value is still there, silently applying
                        // v2 a second time. Attempt the removal FIRST and
                        // only rewrite curr if it actually commits, so
                        // remove_with_debug()'s own trigger_allowed() check
                        // is the single gate for the whole operation.
                        AsmNode *nodes[] = {scan};
                        AsmNode *dummy = scan;
                        if (remove_with_debug(&dummy, nodes, 1, OPT_PEEPHOLE_IMMEDIATE_PROP)) {
                            insert_debug_comment(curr->prev, OPT_PEEPHOLE_IMMEDIATE_PROP, curr->raw);
                            curr->src_op.immediate = result;
                            snprintf(curr->src_op.raw, sizeof(curr->src_op.raw), "%ld", result);
                            snprintf(curr->raw, sizeof(curr->raw), "    MOV %s, %ld", curr->dst_op.raw, result);
                            optimizations++;
                            folded = true;
                        }
                        break;
                    }

                    // Stop if def_reg is modified by any other instruction
                    if (modifies_register(scan, def_reg)) {
                        break;
                    }

                    scan = scan->next;
                }

                if (folded) {
                    curr = next;
                    continue;
                }
            }
        }

        // ----------------------------------------------------------
        // PATTERN 3: Sequential Math Combining (ALU + ALU)
        // ----------------------------------------------------------
        if ((curr->type == OP_IADD || curr->type == OP_ISUB) &&
            curr->dst_op.mode == MODE_REG &&
            is_numeric_immediate(&curr->src_op) && !curr->src_op.is_float)
        {
            AsmNode *next_real = skip_other_nodes(curr->next);
            if (next_real && (next_real->type == OP_IADD || next_real->type == OP_ISUB) &&
                next_real->dst_op.mode == MODE_REG &&
                str_case_eq(curr->dst_op.reg, next_real->dst_op.reg) &&
                is_numeric_immediate(&next_real->src_op) && !next_real->src_op.is_float)
            {
                long val1 = (curr->type == OP_IADD) ? curr->src_op.immediate : -curr->src_op.immediate;
                long val2 = (next_real->type == OP_IADD) ? next_real->src_op.immediate : -next_real->src_op.immediate;
                long combined = val1 + val2;

                if (combined == 0)
                {
                    AsmNode *nodes[] = {curr, next_real};
                    if (remove_with_debug(&curr, nodes, 2, OPT_PEEPHOLE_IMMEDIATE_PROP)) optimizations += 2;
                    continue;
                }
                else
                {
                    // TRIGGER CAP: same reasoning as Pattern 2's fold above --
                    // attempt next_real's removal first, and only rewrite
                    // curr into the combined instruction if that removal
                    // actually commits, so a capped budget can't leave curr
                    // "pre-merged" while next_real (still applying its own
                    // delta) is left behind.
                    AsmNode *nodes[] = {next_real};
                    AsmNode *dummy = next_real;
                    if (remove_with_debug(&dummy, nodes, 1, OPT_PEEPHOLE_IMMEDIATE_PROP)) {
                        if (config.debug) {
                            insert_debug_comment(curr->prev, OPT_PEEPHOLE_IMMEDIATE_PROP, curr->raw);
                        }
                        curr->type = (combined > 0) ? OP_IADD : OP_ISUB;
                        strcpy(curr->mnemonic, (combined > 0) ? "IADD" : "ISUB");
                        curr->src_op.immediate = labs(combined);
                        snprintf(curr->src_op.raw, sizeof(curr->src_op.raw), "%ld", labs(combined));
                        snprintf(curr->raw, sizeof(curr->raw), "    %s %s, %ld",
                                 curr->mnemonic, curr->dst_op.raw, labs(combined));
                        optimizations++;
                        continue;
                    }
                    // TRIGGER CAP: budget exhausted -- nothing was applied.
                    // Must NOT unconditionally "continue" here (that was fine
                    // in the original code, which always removed next_real
                    // and always made forward progress); with the removal
                    // now skipped, curr and next_real are both untouched, so
                    // retrying immediately would re-match the exact same
                    // pattern forever. Fall through to advance past curr
                    // instead, exactly like a non-match.
                }
            }
        }

        curr = next;
    }

    return optimizations;
}
