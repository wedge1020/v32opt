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

/*
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
*/

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

int cse(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr != NULL)
    {
        AsmNode *next = curr->next;

        // Pattern: MOV Rx, A; OP Rx, B
        // This computes OP(A, B) and stores in Rx
        // Later: MOV Ry, A; OP Ry, B can be replaced with MOV Ry, Rx
        if (curr->type == OP_MOV && curr->dst_op.mode == MODE_REG)
        {
            char *dest_reg = curr->dst_op.reg;

            // Skip SP and BP
            if (str_case_eq(dest_reg, "SP") || str_case_eq(dest_reg, "BP"))
            {
                curr = next;
                continue;
            }

            // Look at next non-OP_OTHER instruction
            AsmNode *op_instr = skip_other_nodes(curr->next);

            if (op_instr && is_computable_expression(op_instr) &&
                op_instr->dst_op.mode == MODE_REG &&
                str_case_eq(op_instr->dst_op.reg, dest_reg))
            {
                // Found pattern: MOV Rx, A; OP Rx, B
                // This computes OP(A, B) -> Rx

                // Now scan forward for: MOV Ry, A; OP Ry, B
                AsmNode *scan = op_instr->next;

                while (scan && !is_control_flow_boundary(scan))
                {
                    if (scan->type == OP_OTHER)
                    {
                        scan = scan->next;
                        continue;
                    }

                    // Stop if dest_reg (Rx) is modified
                    if (modifies_register(scan, dest_reg))
                    {
                        break;
                    }

                    // Look for: MOV Ry, same_A; OP Ry, same_B
                    if (scan->type == OP_MOV && scan->dst_op.mode == MODE_REG)
                    {
                        char *new_dest = scan->dst_op.reg;

                        // Skip SP and BP
                        if (str_case_eq(new_dest, "SP") || str_case_eq(new_dest, "BP"))
                        {
                            scan = scan->next;
                            continue;
                        }

                        // Check if MOV source matches the original MOV source
                        if (operands_equal(&scan->src_op, &curr->src_op))
                        {
                            // Now look for the operation on new_dest
                            AsmNode *next_op = skip_other_nodes(scan->next);

                            if (next_op && is_computable_expression(next_op) &&
                                next_op->dst_op.mode == MODE_REG &&
                                str_case_eq(next_op->dst_op.reg, new_dest) &&
                                str_case_eq(next_op->mnemonic, op_instr->mnemonic) &&
                                operands_equal(&next_op->src_op, &op_instr->src_op))
                            {
                                // Found match: MOV Ry, A; OP Ry, B
                                // Replace OP Ry, B with MOV Ry, Rx
                                insert_debug_comment(next_op->prev, OPT_CSE, next_op->raw);

                                next_op->type = OP_MOV;
                                strcpy(next_op->mnemonic, "MOV");
                                next_op->src_op = op_instr->dst_op;
                                next_op->src_op.mode = MODE_REG;
                                snprintf(next_op->raw, sizeof(next_op->raw), "    MOV %s, %s",
                                         next_op->dst_op.raw, next_op->src_op.raw);

                                optimizations++;
                                // Continue scanning for more matches
                                scan = next_op->next;
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
