// ===================================================================
// CSE: Common Subexpression Elimination (Vircon32-Specific)
// Eliminates redundant computations by reusing previous results within basic blocks:
//   - On Vircon32: 2-operand format (IADD R1, R2 means R1 = R1 + R2)
//   - ALL instructions are 1 cycle, so we optimize for SPACE (word count)
//   - Pattern: MOV Rx, A; OP Rx, B; ... MOV Ry, A; OP Ry, B -> MOV Ry, Rx
//
// Vircon32 2-Operand Format:
//   - IADD R1, R2  ->  R1 = R1 + R2
//   - IMUL R1, 42  ->  R1 = R1 * 42
//
// CSE Pattern for Vircon32:
//   MOV R1, R5        ; R1 = R5
//   IADD R1, R2      ; R1 = R5 + R2
//   ... (R1 not modified)
//   MOV R3, R5      ; R3 = R5
//   IADD R3, R2      ; R3 = R5 + R2 -> Replace with: MOV R3, R1
//
// This saves: 1 instruction (2 words if immediate) -> 1 instruction (1 word)
// ===================================================================
/*
#include "v32opt.h"

// Helper: Check if an instruction is a computable expression (not MOV, JMP, etc.)
static bool is_computable_expression(AsmNode *node)
{
    if (!node || node->type == OP_OTHER || node->type == OP_LABEL)
        return false;

    // Control flow
    if (node->type == OP_JMP || node->type == OP_JT || node->type == OP_JF ||
        node->type == OP_CALL || node->type == OP_RET || node->type == OP_HLT)
        return false;

    // Data movement
    if (node->type == OP_MOV || node->type == OP_PUSH || node->type == OP_POP)
        return false;

    return true;
}

// Helper: Check if two operands are equal
static bool operands_equal(Operand *a, Operand *b)
{
    if (a->mode != b->mode)
        return false;

    switch (a->mode) {
        case MODE_REG:
            return str_case_eq(a->reg, b->reg);
        case MODE_IMMEDIATE:
            return a->immediate == b->immediate && a->is_float == b->is_float;
        case MODE_INDIRECT:
            return str_case_eq(a->reg, b->reg) && a->offset == b->offset;
        default:
            return false;
    }
}

// Helper: Build a key for an expression's inputs (operation + source operand)
static void build_expr_key(char *buf, size_t buf_size, const char *mnemonic, Operand *src_op)
{
    if (src_op->mode == MODE_REG) {
        snprintf(buf, buf_size, "%s,%s", mnemonic, src_op->reg);
    } else if (src_op->mode == MODE_IMMEDIATE) {
        snprintf(buf, buf_size, "%s,%ld", mnemonic, src_op->immediate);
    } else {
        snprintf(buf, buf_size, "%s,[%s+%d]", mnemonic, src_op->reg, src_op->offset);
    }
}
*/

// ===================================================================
// CSE: Common Subexpression Elimination (Vircon32-Specific)
// Eliminates redundant computations by reusing previous results within basic blocks:
//   - Pattern: MOV Rx, A; OP Rx, B; ... MOV Ry, A; OP Ry, B -> replace OP Ry,B with MOV Ry, Rx
//   - On Vircon32: 2-operand format (IADD R1, R2 means R1 = R1 + R2)
//   - ALL instructions are 1 cycle, so we optimize for SPACE (word count)
//   - Only eliminates when: (1) same operation, (2) same source operands,
//     (3) destination registers have same initial value (via MOV from same source)
//
// Vircon32 2-Operand Format:
//   - IADD R1, R2  ->  R1 = R1 + R2
//   - IMUL R1, 42  ->  R1 = R1 * 42
//
// Examples:
//   MOV R1, R5    ; R1 = R5
//   IADD R1, R2   ; R1 = R5 + R2
//   MOV R3, R5    ; R3 = R5
//   IADD R3, R2   ; R3 = R5 + R2 -> Replace with: MOV R3, R1
// ===================================================================
#include "v32opt.h"

// Helper: Check if an instruction is a computable expression (not MOV, JMP, etc.)
static bool is_computable_expression(AsmNode *node)
{
    if (!node || node->type == OP_OTHER || node->type == OP_LABEL)
        return false;

    // Control flow
    if (node->type == OP_JMP || node->type == OP_JT || node->type == OP_JF ||
        node->type == OP_CALL || node->type == OP_RET || node->type == OP_HLT)
        return false;

    // Data movement
    if (node->type == OP_MOV || node->type == OP_PUSH || node->type == OP_POP)
        return false;

    return true;
}

int opt_cse (AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr != NULL)
    {
        AsmNode *next = curr->next;

        // ONLY process MOV instructions that start a potential CSE pattern
        if (curr->type == OP_MOV && curr->dst_op.mode == MODE_REG)
        {
            char *rx = curr->dst_op.reg;

            // Skip SP and BP
            if (str_case_eq(rx, "SP") || str_case_eq(rx, "BP"))
            {
                curr = next;
                continue;
            }

            // Look for: OP rx, b  immediately after MOV
            AsmNode *op_rx = skip_other_nodes(curr->next);

            if (op_rx && is_computable_expression(op_rx) &&
                op_rx->dst_op.mode == MODE_REG &&
                str_case_eq(op_rx->dst_op.reg, rx))
            {
                // Found: MOV rx, a; OP rx, b
                // Now scan forward for: MOV ry, a; OP ry, b

                AsmNode *scan = op_rx->next;

                while (scan && !is_control_flow_boundary(scan))
                {
                    if (scan->type == OP_OTHER)
                    {
                        scan = scan->next;
                        continue;
                    }

                    // Stop if rx is modified (the result register)
                    if (modifies_register(scan, rx))
                    {
                        break;
                    }

                    // Look for MOV ry, a (same 'a' as original MOV)
                    if (scan->type == OP_MOV && scan->dst_op.mode == MODE_REG)
                    {
                        char *ry = scan->dst_op.reg;

                        // Skip SP and BP
                        if (str_case_eq(ry, "SP") || str_case_eq(ry, "BP"))
                        {
                            scan = scan->next;
                            continue;
                        }

                        // Check if source matches original MOV source
                        // Use the existing operands_equal from helpers.c
                        if (operands_equal(&scan->src_op, &curr->src_op))
                        {
                            // Found MOV ry, a - now look for OP ry, b
                            AsmNode *op_ry = skip_other_nodes(scan->next);

                            if (op_ry && is_computable_expression(op_ry) &&
                                op_ry->dst_op.mode == MODE_REG &&
                                str_case_eq(op_ry->dst_op.reg, ry) &&
                                str_case_eq(op_ry->mnemonic, op_rx->mnemonic) &&
                                operands_equal(&op_ry->src_op, &op_rx->src_op))
                            {
                                // VERIFY: ry must not be rx (different destination)
                                if (str_case_eq(rx, ry))
                                {
                                    scan = op_ry->next;
                                    continue;
                                }

                                // Found complete pattern - replace OP ry, b with MOV ry, rx
                                insert_debug_comment(op_ry->prev, OPT_CSE, op_ry->raw);

                                op_ry->type = OP_MOV;
                                strcpy(op_ry->mnemonic, "MOV");
                                op_ry->src_op = op_rx->dst_op;
                                op_ry->src_op.mode = MODE_REG;
                                snprintf(op_ry->raw, sizeof(op_ry->raw), "    MOV %s, %s",
                                         op_ry->dst_op.raw, op_ry->src_op.raw);

                                optimizations++;
                                // Continue scanning for more matches with this rx
                                scan = op_ry->next;
                                continue;
                            }
                        }
                    }

                    // Stop at control flow boundaries
                    if (is_control_flow_boundary(scan))
                    {
                        break;
                    }

                    scan = scan->next;
                }
            }
        }

        curr = next;
    }

    return optimizations;
}
