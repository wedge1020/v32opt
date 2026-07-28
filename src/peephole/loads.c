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
// PEEPHOLE: Redundant Load Elimination
// Replaces a load from an address with a register if the value was
// recently loaded into another register:
//   - MOV r1, [r2+off]; MOV r3, [r2+off] → MOV r3, r1
// ===================================================================
int peephole_loads(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        // Must be a load: MOV dst_reg, [base_reg + offset]
        if (curr->type == OP_MOV &&
            curr->has_dst && curr->dst_op.mode == MODE_REG &&
            curr->has_src && curr->src_op.mode == MODE_INDIRECT)
        {
            const char *val_reg = curr->dst_op.reg;
            const char *base_reg = curr->src_op.reg;
            int offset = curr->src_op.offset;

            // Safety check: If curr overwrites its own base address register
            // (e.g., MOV R2, [R2]), subsequent loads from [R2] refer to the
            // new address, so curr cannot be used as a redundant load source.
            if (!str_case_eq(val_reg, base_reg))
            {
                AsmNode *scan = curr->next;
                while (scan)
                {
                    // Skip inline comments and blank lines
                    if (scan->type == OP_OTHER && (scan->raw[0] == '\0' || scan->raw[0] == ';'))
                    {
                        scan = scan->next;
                        continue;
                    }

                    // Stop on control flow boundaries, jumps, calls, or labels
                    if (is_control_flow_boundary(scan) ||
                        str_case_eq(scan->mnemonic, "CALL") ||
                        str_case_eq(scan->mnemonic, "JT")   ||
                        str_case_eq(scan->mnemonic, "JF")   ||
                        scan->type == OP_LABEL)
                    {
                        break;
                    }

                    // Stop on memory writes (stores to indirect destinations or stack/memory ops)
                    if ((scan->has_dst && scan->dst_op.mode == MODE_INDIRECT) ||
                        scan->type == OP_PUSH || scan->type == OP_MOVS || scan->type == OP_SETS)
                    {
                        break;
                    }

                    // Stop if either the base address register or the cached value register is modified
                    if (modifies_register(scan, base_reg) || modifies_register(scan, val_reg))
                    {
                        break;
                    }

                    // Check for redundant load: MOV dst_reg2, [base_reg + offset]
                    if (scan->type == OP_MOV &&
                        scan->has_dst && scan->dst_op.mode == MODE_REG &&
                        scan->has_src && scan->src_op.mode == MODE_INDIRECT &&
                        str_case_eq(scan->src_op.reg, base_reg) &&
                        scan->src_op.offset == offset)
                    {
                        // Insert debug comment using the central debugging system
                        insert_debug_comment(scan->prev, OPT_PEEPHOLE_LOADS, scan->raw);

                        // Replace memory indirect source operand with cached register operand
                        scan->src_op = curr->dst_op;
                        snprintf(scan->raw, sizeof(scan->raw), "    MOV %s, %s",
                                 scan->dst_op.raw, scan->src_op.raw);
                        optimizations++;
                    }

                    scan = scan->next;
                }
            }
        }
        curr = curr->next;
    }

    return optimizations;
}
