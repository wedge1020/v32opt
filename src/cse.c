// ===================================================================
// CSE: Common Subexpression Elimination (Vircon32-Specific)
// Eliminates redundant computations by reusing previous results within basic blocks:
//   - Pattern: MOV Rx, A; OP Rx, B; ... MOV Ry, A; OP Ry, B -> replace OP Ry,B with MOV Ry, Rx
//   - On Vircon32: 2-operand format (IADD R1, R2 means R1 = R1 + R2)
//   - ALL instructions are 1 cycle, so we optimize for SPACE (word count)
//   - Only eliminates when: (1) same operation, (2) same source operands,
//     (3) destination registers have same initial value (via MOV from same source)
// ===================================================================
// ===================================================================
// CSE: Common Subexpression Elimination (Vircon32-Specific)
// Fixed version: Strict operation matching + comprehensive control flow detection
// ===================================================================
#include "v32opt.h"

// ----- Local control flow boundary check (comprehensive) -----
static bool is_cf_boundary(AsmNode *node) {
    if (!node) return true;
    if (node->type == OP_LABEL) return true;
    if (node->type == OP_JMP || node->type == OP_CALL || node->type == OP_RET ||
        node->type == OP_HLT || node->type == OP_JT || node->type == OP_JF) return true;
    return false;
}

// ----- Strict operation match: type AND mnemonic -----
static bool ops_match(AsmNode *a, AsmNode *b) {
    return a && b && a->type == b->type && str_case_eq(a->mnemonic, b->mnemonic);
}

// ----- Check if instruction is a computable ALU operation -----
static bool is_computable_expression(AsmNode *node) {
    if (!node || node->type == OP_OTHER || node->type == OP_LABEL) return false;
    if (node->type == OP_JMP || node->type == OP_JT || node->type == OP_JF ||
        node->type == OP_CALL || node->type == OP_RET || node->type == OP_HLT) return false;
    if (node->type == OP_MOV || node->type == OP_PUSH || node->type == OP_POP) return false;
    return true;
}

int opt_cse(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr != NULL) {
        AsmNode *next = curr->next;

        // Only process MOV Rx, A instructions
        if (curr->type == OP_MOV && curr->dst_op.mode == MODE_REG) {
            char *rx = curr->dst_op.reg;

            // Skip SP and BP
            if (str_case_eq(rx, "SP") || str_case_eq(rx, "BP")) {
                curr = next;
                continue;
            }

            // Find immediate OP Rx, B
            AsmNode *op_rx = skip_other_nodes(curr->next);

            if (op_rx && is_computable_expression(op_rx) &&
                op_rx->dst_op.mode == MODE_REG &&
                str_case_eq(op_rx->dst_op.reg, rx)) {

                // Stop if OP Rx,B is at control flow boundary
                if (is_cf_boundary(op_rx)) {
                    curr = next;
                    continue;
                }

                // Scan forward for MOV Ry, A; OP Ry, B
                AsmNode *scan = op_rx->next;
                while (scan != NULL) {
                    // Stop at ANY control flow boundary
                    if (is_cf_boundary(scan)) break;

                    if (scan->type == OP_OTHER) {
                        scan = scan->next;
                        continue;
                    }

                    // Stop if Rx is modified
                    if (modifies_register(scan, rx)) break;

                    // Found candidate MOV Ry, A
                    if (scan->type == OP_MOV && scan->dst_op.mode == MODE_REG) {
                        char *ry = scan->dst_op.reg;

                        // Skip SP and BP
                        if (str_case_eq(ry, "SP") || str_case_eq(ry, "BP")) {
                            scan = scan->next;
                            continue;
                        }

                        // Ry != Rx (prevent self-match)
                        if (str_case_eq(rx, ry)) {
                            scan = scan->next;
                            continue;
                        }

                        // Source must match original MOV
                        if (operands_equal(&scan->src_op, &curr->src_op)) {
                            AsmNode *op_ry = skip_other_nodes(scan->next);

                            // Must be in same basic block
                            if (!op_ry || is_cf_boundary(op_ry)) {
                                scan = scan->next;
                                continue;
                            }

                            // CRITICAL: Exact operation match (type + mnemonic)
                            if (is_computable_expression(op_ry) &&
                                op_ry->dst_op.mode == MODE_REG &&
                                str_case_eq(op_ry->dst_op.reg, ry) &&
                                ops_match(op_ry, op_rx) &&  // <-- FIX #2: Strict match
                                operands_equal(&op_ry->src_op, &op_rx->src_op)) {

                                insert_debug_comment(op_ry->prev, OPT_CSE, op_ry->raw);

                                // Replace OP Ry,B with MOV Ry, Rx
                                op_ry->type = OP_MOV;
                                strcpy(op_ry->mnemonic, "MOV");
                                op_ry->src_op = op_rx->dst_op;
                                op_ry->src_op.mode = MODE_REG;
                                snprintf(op_ry->raw, sizeof(op_ry->raw), "    MOV %s, %s",
                                         op_ry->dst_op.raw, op_ry->src_op.raw);

                                optimizations++;
                                // Continue scanning for more matches with this Rx
                                scan = op_ry->next;
                                continue;
                            }
                        }
                    }
                    scan = scan->next;
                }
            }
        }
        curr = next;
    }
    return optimizations;
}
