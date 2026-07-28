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
// PEEPHOLE: Dead Store Elimination (DSE) - Upgraded with Liveness!
// Eliminates register writes overwritten OR dead at function exit.
// ===================================================================
int peephole_dead_stores(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        // We only care about pure MOV instructions that define a register
        if (curr->type == OP_MOV && curr->has_dst && curr->dst_op.mode == MODE_REG)
        {
            char *def_reg = curr->dst_op.reg;

            // Never optimize away stack frame manipulations
            if (!str_case_eq(def_reg, "SP") && !str_case_eq(def_reg, "BP"))
            {
                AsmNode *scan = curr->next;
                while (scan)
                {
                    if (scan->type == OP_OTHER && (scan->raw[0] == '\0' || scan->raw[0] == ';')) {
                        scan = scan->next;
                        continue;
                    }

                    // ----------------------------------------------------
                    // CRITICAL CHANGE 1:
                    // Do NOT break on OP_LABEL! It is mathematically safe to scan
                    // across labels for dead stores as long as we check reads.
                    // Only break on branching jumps (JMP) or function calls.
                    // ----------------------------------------------------
                    if (str_case_eq(scan->mnemonic, "JMP")  ||
                        str_case_eq(scan->mnemonic, "CALL") ||
                        str_case_eq(scan->mnemonic, "JT")   ||
                        str_case_eq(scan->mnemonic, "JF")   ||
                        str_case_eq(scan->mnemonic, "HLT"))
                    {
                        break;
                    }

                    // If any instruction READS our register, the store is live! Abort scan.
                    if (is_register_read(scan, def_reg)) {
                        break;
                    }

                    // ----------------------------------------------------
                    // CRITICAL CHANGE 2: Terminal Dead Store Check
                    // If we reach a RET instruction, check if def_reg is live-out!
                    // If it is NOT R0, SP, or BP, nobody will ever read it.
                    // ----------------------------------------------------
                    if (str_case_eq(scan->mnemonic, "RET"))
                    {
                        if (!is_live_out_register(def_reg)) {
                            curr->type = OP_OTHER;
                            snprintf(curr->raw, sizeof(curr->raw), "; optimized out terminal dead store: MOV %s", def_reg);
                            optimizations++;
                        }
                        break; // Stop scanning after RET
                    }

                    // Standard DSE: Another MOV overwrites our register before it was read
                    if (scan->type == OP_MOV && scan->has_dst &&
                        scan->dst_op.mode == MODE_REG && str_case_eq(scan->dst_op.reg, def_reg))
                    {
                        curr->type = OP_OTHER;
                        snprintf(curr->raw, sizeof(curr->raw), "; optimized out dead store: MOV %s", def_reg);
                        optimizations++;
                        break;
                    }

                    scan = scan->next;
                }
            }
        }
        curr = curr->next;
    }

    return optimizations;
}
