#include "v32opt.h"

////////////////////////////////////////////////////////////////////////////////////////
//
// -------------------------------------------------------------------
// OPTIMIZATION CATEGORY: Peephole Optimizations
// Small-window (1-3 instruction) local transformations that improve
// code without global analysis.
// -------------------------------------------------------------------
//
// Note: On Vircon32, ALL instructions are 1 cycle, so many transformations
// are cost-neutral. We keep them for code clarity, size reduction, or
// idiomatic style.
//
// peephole_pairs()          - adjacent instruction pair elimination (DEBUG)
// peephole_algebra()        - algebraic simplifications (DEBUG)
// peephole_forwarding()     - store-to-load forwarding (DEBUG)
// peephole_jumps()          - redundant jump elimination (DEBUG, broken)
// peephole_movs()           - redundant MOV elimination (DEBUG)
// peephole_immediates()     - combine immediates (DEBUG)
// peephole_reduce()         - strength reduction (cost-neutral on Vircon32)
// peephole_shifts()         - shift optimizations
// peephole_dead_stores()    - dead store elimination
// peephole_loads()          - redundant load elimination (DEBUG)
// peephole_immediate_prop() - immediate propagation (DEBUG)
// peephole_jmp_chain()      - jump chain elimination
//
////////////////////////////////////////////////////////////////////////////////////////

// ===================================================================
// PEEPHOLE: Forward-Scanning Store & Copy Propagation
// Scans forward within basic blocks to forward:
//   1. Store-to-Load: MOV [mem], R1; ... MOV R2, [mem] -> MOV R2, R1
//   2. Copy Prop:     MOV R1, val;   ... OP R2, R1     -> OP R2, val
// ===================================================================
int peephole_forwarding(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        if (curr->type == OP_MOV)
        {
            // =========================================================
            // RULE 1: Store-to-Load Forwarding
            // =========================================================
            if (curr->dst_op.mode == MODE_INDIRECT && curr->src_op.mode == MODE_REG)
            {
                char *mem_reg = curr->dst_op.reg;
                int mem_off = curr->dst_op.offset;
                char *src_reg = curr->src_op.reg;

                AsmNode *scan = curr->next;
                while (scan)
                {
                    if (scan->type == OP_OTHER && (scan->raw[0] == '\0' || scan->raw[0] == ';')) {
                        scan = scan->next;
                        continue;
                    }

                    // Stop if memory base register or source register is modified
                    if (modifies_register(scan, mem_reg) || modifies_register(scan, src_reg)) break;

                    // 🔥 STOP on ANY store to memory with same base register
                    if (scan->type == OP_MOV && scan->dst_op.mode == MODE_INDIRECT &&
                        str_case_eq(scan->dst_op.reg, mem_reg)) {
                        break;
                    }

                    if (scan->type == OP_MOV && scan->dst_op.mode == MODE_REG &&
                        scan->src_op.mode == MODE_INDIRECT &&
                        str_case_eq(scan->src_op.reg, mem_reg) && scan->src_op.offset == mem_off)
                    {
                        insert_debug_comment(scan->prev, OPT_PEEPHOLE_FORWARDING, scan->raw);
                        scan->src_op = curr->src_op;
                        snprintf(scan->raw, sizeof(scan->raw), "    MOV %s, %s",
                                 scan->dst_op.raw, scan->src_op.raw);
                        optimizations++;
                        break;
                    }

                    // Evaluate control flow boundaries AFTER checking for optimization
                    if (is_control_flow_boundary(scan)) break;

                    scan = scan->next;
                }
            }

            // =========================================================
            // RULE 2: Copy Propagation
            // =========================================================
            if (curr->dst_op.mode == MODE_REG)
            {
                char *def_reg = curr->dst_op.reg;

                if (str_case_eq(def_reg, "SP") || str_case_eq(def_reg, "BP")) {
                    curr = curr->next;
                    continue;
                }

                AsmNode *scan = curr->next;
                while (scan)
                {
                    if (scan->type == OP_OTHER && (scan->raw[0] == '\0' || scan->raw[0] == ';')) {
                        scan = scan->next;
                        continue;
                    }

                    if (modifies_register(scan, def_reg)) break;
                    if (curr->src_op.mode == MODE_REG && modifies_register(scan, curr->src_op.reg)) {
                        break;
                    }

                    bool uses_def_reg = false;
                    Operand *target_op = NULL;

                    // Two-operand instructions: check src_op
                    if (scan->has_src && scan->src_op.mode == MODE_REG &&
                        str_case_eq(scan->src_op.reg, def_reg)) {
                        uses_def_reg = true;
                        target_op = &scan->src_op;
                    }
                    // 🔥 FIX: Single-operand instructions (like JMP) are parsed into dst_op!
                    else if (str_case_eq(scan->mnemonic, "JMP") && scan->has_dst &&
                             scan->dst_op.mode == MODE_REG &&
                             str_case_eq(scan->dst_op.reg, def_reg)) {
                        uses_def_reg = true;
                        target_op = &scan->dst_op;
                    }

                    if (uses_def_reg)
                    {
                        // Guard: Block numeric immediates into JT/JF
                        if ((str_case_eq(scan->mnemonic, "JT") || str_case_eq(scan->mnemonic, "JF")) &&
                            curr->src_op.mode == MODE_IMMEDIATE &&
                            (isdigit((unsigned char)curr->src_op.raw[0]) ||
                             (curr->src_op.raw[0] == '-' && isdigit((unsigned char)curr->src_op.raw[1])))) {
                            break;
                        }

                        // Guard: Block POW/ATAN2
                        if (str_case_eq(scan->mnemonic, "POW") || str_case_eq(scan->mnemonic, "ATAN2")) {
                            break;
                        }

                        // Guard: Block illegal immediate stores
                        bool is_illegal_imm_store = (curr->src_op.mode == MODE_IMMEDIATE &&
                                                     scan->has_dst && scan->dst_op.mode != MODE_REG &&
                                                     !str_case_eq(scan->mnemonic, "JMP"));
                        if (is_illegal_imm_store) break;

                        insert_debug_comment(scan->prev, OPT_PEEPHOLE_FORWARDING, scan->raw);
                        *target_op = curr->src_op;

                        if (str_case_eq(scan->mnemonic, "JMP")) {
                            snprintf(scan->raw, sizeof(scan->raw), "    JMP %s", target_op->raw);
                        } else if (scan->has_dst && scan->has_src) {
                            snprintf(scan->raw, sizeof(scan->raw), "    %s %s, %s",
                                     scan->mnemonic, scan->dst_op.raw, scan->src_op.raw);
                        } else if (scan->has_dst) {
                            snprintf(scan->raw, sizeof(scan->raw), "    %s %s",
                                     scan->mnemonic, scan->dst_op.raw);
                        } else {
                            snprintf(scan->raw, sizeof(scan->raw), "    %s %s",
                                     scan->mnemonic, scan->src_op.raw);
                        }
                        optimizations++;
                    }

                    // Evaluate control flow boundaries AFTER attempting optimization
                    if (is_control_flow_boundary(scan)) break;

                    scan = scan->next;
                }
            }
        }
        curr = curr->next;
    }
    return optimizations;
}
