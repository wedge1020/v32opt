// ===================================================================
// CSE: Common Subexpression Elimination (Vircon32-Specific)
// ===================================================================
#include "v32opt.h"

// --- Local boundary check: uses OpType enum (not just mnemonic) ---
static bool is_cf_boundary(AsmNode *node) {
    if (!node) return true;
    if (node->type == OP_LABEL) return true;
    if (node->type == OP_JMP || node->type == OP_JT || node->type == OP_JF ||
        node->type == OP_CALL || node->type == OP_RET || node->type == OP_HLT) return true;
    if (str_case_eq(node->mnemonic, "JMP") || str_case_eq(node->mnemonic, "JT") ||
        str_case_eq(node->mnemonic, "JF") || str_case_eq(node->mnemonic, "CALL") ||
        str_case_eq(node->mnemonic, "RET") || str_case_eq(node->mnemonic, "HLT")) return true;
    return false;
}

// --- Strict operation match: type AND mnemonic ---
static bool ops_match(AsmNode *a, AsmNode *b) {
    return a && b && a->type == b->type && str_case_eq(a->mnemonic, b->mnemonic);
}

// --- Computable expression check ---
static bool is_computable_expression(AsmNode *node) {
    if (!node || node->type == OP_OTHER || node->type == OP_LABEL) return false;
    if (node->type == OP_JMP || node->type == OP_JT || node->type == OP_JF ||
        node->type == OP_CALL || node->type == OP_RET || node->type == OP_HLT) return false;
    if (node->type == OP_MOV || node->type == OP_PUSH || node->type == OP_POP) return false;

    // === LUA MODE FIX: Skip OR with BOXED_* immediates ===
    if (is_lua_mode() && node->type == OP_OR && is_boxed_type_operand(&node->src_op)) {
        return false;
    }

    return true;
}

int opt_cse(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr != NULL) {
        AsmNode *next = curr->next;

        if (curr->type == OP_MOV && curr->dst_op.mode == MODE_REG) {
            char *rx = curr->dst_op.reg;
            if (str_case_eq(rx, "SP") || str_case_eq(rx, "BP")) { curr = next; continue; }

            AsmNode *op_rx = skip_other_nodes(curr->next);

            if (op_rx && is_computable_expression(op_rx) &&
                op_rx->dst_op.mode == MODE_REG &&
                str_case_eq(op_rx->dst_op.reg, rx)) {

                if (is_cf_boundary(op_rx)) { curr = next; continue; }

                AsmNode *scan = op_rx->next;
                while (scan != NULL) {
                    if (is_cf_boundary(scan)) break;
                    if (scan->type == OP_OTHER) { scan = scan->next; continue; }
                    if (modifies_register(scan, rx)) break;

                    if (scan->type == OP_MOV && scan->dst_op.mode == MODE_REG) {
                        char *ry = scan->dst_op.reg;
                        if (str_case_eq(ry, "SP") || str_case_eq(ry, "BP")) { scan = scan->next; continue; }
                        if (str_case_eq(rx, ry)) { scan = scan->next; continue; }

                        if (operands_equal(&scan->src_op, &curr->src_op)) {
                            AsmNode *op_ry = skip_other_nodes(scan->next);
                            if (!op_ry || is_cf_boundary(op_ry)) { scan = scan->next; continue; }

                            if (is_computable_expression(op_ry) &&
                                op_ry->dst_op.mode == MODE_REG &&
                                str_case_eq(op_ry->dst_op.reg, ry) &&
                                ops_match(op_ry, op_rx) &&
                                operands_equal(&op_ry->src_op, &op_rx->src_op)) {

                                // === LUA MODE FIX: Never optimize away boxed type tagging ===
                                if (is_lua_mode() && is_boxed_tagging(op_ry)) {
                                    scan = op_ry->next;
                                    continue;
                                }

                                insert_debug_comment(op_ry->prev, OPT_CSE, op_ry->raw);
                                op_ry->type = OP_MOV;
                                strcpy(op_ry->mnemonic, "MOV");
                                op_ry->src_op = op_rx->dst_op;
                                op_ry->src_op.mode = MODE_REG;
                                snprintf(op_ry->raw, sizeof(op_ry->raw), "    MOV %s, %s",
                                         op_ry->dst_op.raw, op_ry->src_op.raw);
                                optimizations++;
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
