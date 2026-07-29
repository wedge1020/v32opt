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

        // Use is_numeric_immediate to protect symbolic defines from being propagated as '0'
        if (curr->type == OP_MOV && curr->dst_op.mode == MODE_REG &&
            is_numeric_immediate(&curr->src_op))
        {
            char *def_reg = curr->dst_op.reg;

            // Skip special registers
            if (str_case_eq(def_reg, "SP") || str_case_eq(def_reg, "BP"))
            {
                curr = next;
                continue;
            }

            // Scan forward for uses of def_reg
            AsmNode *scan = curr->next;
            while (scan)
            {
                // Skip OP_OTHER nodes (comments, blank lines) safely
                if (scan->type == OP_OTHER && (scan->raw[0] == '\0' || scan->raw[0] == ';'))
                {
                    scan = scan->next;
                    continue;
                }

                // Stop if def_reg is modified
                if (modifies_register(scan, def_reg))
                {
                    break;
                }

                bool uses_def_reg = false;
                Operand *target_op = NULL;

                // Check source operand directly by mode/register (bypassing unreliable has_src flags)
                if (scan->src_op.mode == MODE_REG && str_case_eq(scan->src_op.reg, def_reg))
                {
                    uses_def_reg = true;
                    target_op = &scan->src_op;
                }
                // Check destination operand for single-operand instructions that read it
                else if ((str_case_eq(scan->mnemonic, "JMP") ||
                          str_case_eq(scan->mnemonic, "PUSH") ||
                          str_case_eq(scan->mnemonic, "POP")) &&
                         scan->dst_op.mode == MODE_REG &&
                         str_case_eq(scan->dst_op.reg, def_reg))
                {
                    uses_def_reg = true;
                    target_op = &scan->dst_op;
                }

                if (uses_def_reg && target_op)
                {
                    bool skip_substitution = false;

                    // Guard: Don't propagate into JT/JF with numeric immediates
                    if ((str_case_eq(scan->mnemonic, "JT") || str_case_eq(scan->mnemonic, "JF")) &&
                        is_numeric_immediate(&curr->src_op))
                    {
                        skip_substitution = true;
                    }

                    // Guard: Don't propagate into instructions that don't accept immediates
                    if (str_case_eq(scan->mnemonic, "POW") || str_case_eq(scan->mnemonic, "ATAN2"))
                    {
                        skip_substitution = true;
                    }

                    // Guard: Don't propagate into stores with non-register destinations
                    if (scan->type == OP_MOV && scan->dst_op.mode != MODE_REG)
                    {
                        skip_substitution = true;
                    }

                    // Perform the propagation if no guards were triggered
                    if (!skip_substitution)
                    {
                        insert_debug_comment(scan->prev, OPT_PEEPHOLE_IMMEDIATE_PROP, scan->raw);

                        // Direct operand struct copy ensures floats, modes, and raw strings are safely cloned
                        *target_op = curr->src_op;

                        // Update the instruction raw text dynamically based on available operands
                        if (str_case_eq(scan->mnemonic, "JMP"))
                        {
                            snprintf(scan->raw, sizeof(scan->raw), "    JMP %s", target_op->raw);
                        }
                        else if (scan->dst_op.mode != MODE_NONE && scan->src_op.mode != MODE_NONE)
                        {
                            snprintf(scan->raw, sizeof(scan->raw), "    %s %s, %s",
                                     scan->mnemonic, scan->dst_op.raw, scan->src_op.raw);
                        }
                        else if (scan->dst_op.mode != MODE_NONE)
                        {
                            snprintf(scan->raw, sizeof(scan->raw), "    %s %s",
                                     scan->mnemonic, scan->dst_op.raw);
                        }
                        else if (scan->src_op.mode != MODE_NONE)
                        {
                            snprintf(scan->raw, sizeof(scan->raw), "    %s %s",
                                     scan->mnemonic, scan->src_op.raw);
                        }

                        optimizations++;
                    }
                }

                // Stop at control flow boundaries AFTER analyzing them
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
/*
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

        // Use is_numeric_immediate to protect symbolic defines from being propagated as '0'
        if (curr->type == OP_MOV && curr->dst_op.mode == MODE_REG &&
            is_numeric_immediate(&curr->src_op))
        {
            char *def_reg = curr->dst_op.reg;

            // Skip special registers
            if (str_case_eq(def_reg, "SP") || str_case_eq(def_reg, "BP"))
            {
                curr = next;
                continue;
            }

            // Scan forward for uses of def_reg
            AsmNode *scan = curr->next;
            while (scan)
            {
                // Skip OP_OTHER nodes (comments, blank lines) safely
                if (scan->type == OP_OTHER && (scan->raw[0] == '\0' || scan->raw[0] == ';'))
                {
                    scan = scan->next;
                    continue;
                }

                // Stop if def_reg is modified
                if (modifies_register(scan, def_reg))
                {
                    break;
                }

                // Check if this instruction uses def_reg
                bool uses_def_reg = false;
                Operand *target_op = NULL;

                // Two-operand instructions or general source uses
                if (scan->has_src && scan->src_op.mode == MODE_REG &&
                    str_case_eq(scan->src_op.reg, def_reg))
                {
                    uses_def_reg = true;
                    target_op = &scan->src_op;
                }
                // Handle single-operand instructions where the operand is in dst_op
                else if ((str_case_eq(scan->mnemonic, "JMP") ||
                          str_case_eq(scan->mnemonic, "PUSH") ||
                          str_case_eq(scan->mnemonic, "POP")) &&
                         scan->has_dst && scan->dst_op.mode == MODE_REG &&
                         str_case_eq(scan->dst_op.reg, def_reg))
                {
                    uses_def_reg = true;
                    target_op = &scan->dst_op;
                }

                if (uses_def_reg && target_op)
                {
                    bool skip_substitution = false;

                    // Guard: Don't propagate into JT/JF with numeric immediates
                    if ((str_case_eq(scan->mnemonic, "JT") || str_case_eq(scan->mnemonic, "JF")) &&
                        is_numeric_immediate(&curr->src_op))
                    {
                        skip_substitution = true;
                    }

                    // Guard: Don't propagate into instructions that don't accept immediates
                    if (str_case_eq(scan->mnemonic, "POW") || str_case_eq(scan->mnemonic, "ATAN2"))
                    {
                        skip_substitution = true;
                    }

                    // Guard: Don't propagate into stores with non-register destinations
                    if (scan->type == OP_MOV && scan->dst_op.mode != MODE_REG)
                    {
                        skip_substitution = true;
                    }

                    // Perform the propagation if no guards were triggered
                    if (!skip_substitution)
                    {
                        insert_debug_comment(scan->prev, OPT_PEEPHOLE_IMMEDIATE_PROP, scan->raw);

                        // Direct operand struct copy ensures floats, modes, and raw strings are safely cloned
                        *target_op = curr->src_op;

                        // Update the instruction raw text dynamically based on available operands
                        if (str_case_eq(scan->mnemonic, "JMP"))
                        {
                            snprintf(scan->raw, sizeof(scan->raw), "    JMP %s", target_op->raw);
                        }
                        else if (scan->has_dst && scan->has_src)
                        {
                            snprintf(scan->raw, sizeof(scan->raw), "    %s %s, %s",
                                     scan->mnemonic, scan->dst_op.raw, scan->src_op.raw);
                        }
                        else if (scan->has_dst)
                        {
                            snprintf(scan->raw, sizeof(scan->raw), "    %s %s",
                                     scan->mnemonic, scan->dst_op.raw);
                        }
                        else if (scan->has_src)
                        {
                            snprintf(scan->raw, sizeof(scan->raw), "    %s %s",
                                     scan->mnemonic, scan->src_op.raw);
                        }

                        optimizations++;
                    }
                }

                // Stop at control flow boundaries AFTER analyzing them
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
*/
