#include "v32opt.h"

// ===================================================================
// PEEPHOLE: Immediate Folding (-fpeephole-immediate-prop)
// Handles (integer immediates only; register destinations only):
//   1. Identity math elimination:   IADD/ISUB R, 0   and  IMUL/IDIV R, 1
//   2. Constant folding (MOV + ALU): MOV R1, 10 ... IADD R1, 5
//                                    -> MOV R1, 15
//      (IADD/ISUB/IMUL; never across a read of R1, a write to it, a
//      conditional branch, or a control-flow boundary)
//   3. Sequential math combining:   IADD R1, 5 / ISUB R1, 3 -> IADD R1, 2
//      (adjacent; a pair that cancels is removed entirely)
// A fold whose result would not fit a 32-bit decimal literal
// (fits_int_literal) is skipped.
//
// NOTE: despite the pass name, it does NOT substitute a constant
// register into another instruction's operand ("MOV R1, 42 / IADD R2,
// R1" is left alone).
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

                    // Check for foldable ALU instruction first (before
                    // the read/modifies checks below: the ALU target itself
                    // reads def_reg read-modify-write style, which is fine
                    // -- it is the consumer being folded away).
                    if ((scan->type == OP_IADD || scan->type == OP_ISUB || scan->type == OP_IMUL) &&
                        scan->dst_op.mode == MODE_REG && str_case_eq(scan->dst_op.reg, def_reg) &&
                        is_numeric_immediate(&scan->src_op) && !scan->src_op.is_float)
                    {
                        long long v1 = curr->src_op.immediate;
                        long long v2 = scan->src_op.immediate;
                        long long result = 0;

                        if (scan->type == OP_IADD) result = v1 + v2;
                        else if (scan->type == OP_ISUB) result = v1 - v2;
                        else if (scan->type == OP_IMUL) result = v1 * v2;

                        // An overflowing fold ("MOV R1,100000 / IMUL R1,
                        // 100000") used to emit an out-of-range literal the
                        // assembler rejects. Leave such pairs alone.
                        if (!fits_int_literal(result)) {
                            break;
                        }

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
                            curr->src_op.immediate = (int)result;
                            snprintf(curr->src_op.raw, sizeof(curr->src_op.raw), "%lld", result);
                            snprintf(curr->raw, sizeof(curr->raw), "    MOV %s, %lld", curr->dst_op.raw, result);
                            optimizations++;
                            folded = true;
                        }
                        break;
                    }

                    // -------------------------------------------------------
                    // BUG FIX: stop at conditional branches and at READS of
                    // def_reg. The MOV being rewritten executes BEFORE this
                    // scan point on EVERY path that reaches it -- including
                    // the branch-taken side of a JT/JF -- so folding across
                    // a branch, or across an intervening instruction that
                    // observes def_reg's pre-ALU value (e.g. "MOV R2, R1"
                    // or "PUSH R1"), changes values other code can see.
                    // Reproduction: "MOV R1,10 / MOV R2,R1 / IADD R1,5"
                    // used to fold to "MOV R1,15 / MOV R2,R1", giving R2
                    // 15 instead of 10; same corruption on the taken side
                    // of a "JT Rx,label" sitting between the MOV and ALU.
                    // -------------------------------------------------------
                    if (scan->type == OP_JT || scan->type == OP_JF) {
                        break;
                    }
                    if (is_register_read(scan, def_reg)) {
                        break;
                    }

                    // Stop if def_reg is modified by any other instruction
                    if (modifies_register(scan, def_reg)) {
                        break;
                    }

                    scan = scan->next;
                }

                if (folded) {
                    // BUG FIX (use-after-free): do NOT resume at the saved
                    // 'next'. When the folded ALU instruction sat directly
                    // after this MOV, 'next' IS the node that was just
                    // removed and freed. curr itself is still live (only
                    // its text changed), so re-examine it: that is safe,
                    // and also picks up a chain ("MOV / IADD / IADD") in
                    // one pass. Terminates because every fold deletes a
                    // node. Exposed once %define resolution made
                    // "MOV R2, MEMCARD_END / ISUB R2, MEMCARD_DATA_BASE"
                    // foldable; adjacent literal pairs never occurred.
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
                long val1 = (curr->type == OP_IADD) ? (long)curr->src_op.immediate : -(long)curr->src_op.immediate;
                long val2 = (next_real->type == OP_IADD) ? (long)next_real->src_op.immediate : -(long)next_real->src_op.immediate;
                long combined = val1 + val2;

                if (!fits_int_literal(combined))
                {
                    // overflowed: no single literal says it -- keep both
                }
                else if (combined == 0)
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
