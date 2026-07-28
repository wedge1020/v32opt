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
#include "v32opt.h"

int peephole_immediate_prop(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr != NULL)
    {
        AsmNode *next = curr->next;

        // ----------------------------------------------------------
        // PATTERN 1: MOV with Immediate Value Propagation
        // MOV R1, 42; ... OP R2, R1 → OP R2, 42
        // ----------------------------------------------------------
        if (curr->type == OP_MOV && curr->dst_op.mode == MODE_REG &&
            curr->src_op.mode == MODE_IMMEDIATE)
        {
            char *def_reg = curr->dst_op.reg;
            long immediate_val = curr->src_op.immediate;
            char imm_str[64];
            snprintf(imm_str, sizeof(imm_str), "%ld", immediate_val);

            // Skip special registers
            if (str_case_eq(def_reg, "SP") || str_case_eq(def_reg, "BP"))
            {
                curr = next;
                continue;
            }

            // Scan forward for uses of def_reg
            AsmNode *scan = curr->next;
            while (scan && !is_control_flow_boundary(scan))
            {
                // Skip OP_OTHER nodes
                if (scan->type == OP_OTHER)
                {
                    scan = scan->next;
                    continue;
                }

                // Stop if def_reg is modified
                if (modifies_register(scan, def_reg))
                {
                    break;
                }

                // Check if this instruction uses def_reg as a source operand
                bool uses_def_reg = false;
                Operand *target_op = NULL;

                // Two-operand instructions: check src_op
                if (scan->has_src && scan->src_op.mode == MODE_REG &&
                    str_case_eq(scan->src_op.reg, def_reg))
                {
                    uses_def_reg = true;
                    target_op = &scan->src_op;
                }
                // For single-operand instructions that use dst_op as source
                else if (scan->type != OP_MOV && scan->has_dst &&
                         scan->dst_op.mode == MODE_REG &&
                         str_case_eq(scan->dst_op.reg, def_reg))
                {
                    // This is tricky - dst_op is usually the destination
                    // But for some instructions like NOT, it might be the only operand
                    // We'll skip these cases to be safe
                    scan = scan->next;
                    continue;
                }

                if (uses_def_reg && target_op)
                {
                    // Guard: Don't propagate into JT/JF with numeric immediates
                    if ((str_case_eq(scan->mnemonic, "JT") || str_case_eq(scan->mnemonic, "JF")) &&
                        isdigit((unsigned char)imm_str[0]))
                    {
                        break;
                    }

                    // Guard: Don't propagate into instructions that don't accept immediates
                    if (str_case_eq(scan->mnemonic, "POW") || str_case_eq(scan->mnemonic, "ATAN2"))
                    {
                        break;
                    }

                    // Guard: Don't propagate into stores with non-register destinations
                    if (scan->type == OP_MOV && scan->dst_op.mode != MODE_REG)
                    {
                        break;
                    }

                    // Perform the propagation
                    insert_debug_comment(scan->prev, OPT_PEEPHOLE_IMMEDIATE_PROP, scan->raw);

                    // Update the operand
                    target_op->mode = MODE_IMMEDIATE;
                    target_op->immediate = immediate_val;
                    target_op->is_float = false;
                    snprintf(target_op->raw, sizeof(target_op->raw), "%ld", immediate_val);

                    // Update the instruction raw text
                    if (scan->has_dst && scan->has_src)
                    {
                        snprintf(scan->raw, sizeof(scan->raw), "    %s %s, %s",
                                 scan->mnemonic, scan->dst_op.raw, target_op->raw);
                    }
                    else if (scan->has_dst)
                    {
                        snprintf(scan->raw, sizeof(scan->raw), "    %s %s",
                                 scan->mnemonic, scan->dst_op.raw);
                    }

                    optimizations++;
                    // Continue scanning for more uses
                }

                // Stop at control flow boundaries
                if (is_control_flow_boundary(scan))
                {
                    break;
                }

                scan = scan->next;
            }
        }

        curr = next;
    }

    return optimizations;
}
