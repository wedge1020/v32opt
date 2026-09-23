#include "v32opt.h"

// ===================================================================
// CSE: Common Subexpression Elimination (Vircon32-Specific)
// ===================================================================

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

    // === LUA MODE FIX: Skip boxed-tagging ops (OR/AND/IADD with BOXED_*
    // immediates -- see is_boxed_tagging()'s comment in tools.c). Previously
    // this only checked OP_OR directly, which let "AND Rd, BOXED_PAYLOAD"
    // (unboxing) and "IADD Rd, BOXED_BOOLEAN" (boolean boxing) be treated as
    // ordinary computable expressions and considered for CSE. ===
    if (is_lua_mode() && is_boxed_tagging(node)) {
        return false;
    }

    return true;
}

// ---------------------------------------------------------------
// BUG FIX helper: might 'node' change the VALUE that operand 'op'
// currently holds? The original CSE only tracked the DESTINATION
// register between the two matching expressions; the sources were
// compared purely textually, so a source register that was modified
// in between (or memory a source pointed at that was re-written)
// still "matched", and the second computation was replaced with a
// move of the now-stale first result.
//
// Reproduction:
//     MOV R1, R4
//     IADD R1, R6     ; R1 = old R4 + R6
//     MOV R4, 99      ; <-- source A changed; not detected before
//     MOV R3, R4
//     IADD R3, R6     ; was replaced with "MOV R3, R1" (old R4+R6)
//
// Rules:
//   - register operand: clobbered iff the node modifies that register
//   - indirect operand: clobbered by ANY memory write (any indirect
//     destination, MOVS/SETS dynamic writes, PUSH/POP/CALL stack
//     traffic) -- we cannot prove non-aliasing through other bases
//   - immediate/symbolic operand: never clobbered
// ---------------------------------------------------------------
static bool operand_value_clobbered(AsmNode *node, const Operand *op) {
    if (!node || !op) return false;

    if (op->mode == MODE_REG) {
        return modifies_register(node, op->reg);
    }
    if (op->mode == MODE_INDIRECT) {
        if (node->has_dst && node->dst_op.mode == MODE_INDIRECT) return true;
        if (node->type == OP_MOVS || node->type == OP_SETS) return true;
        if (node->type == OP_PUSH || node->type == OP_POP || node->type == OP_CALL) return true;
    }
    return false;
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

                    // BUG FIX: the sources of the tracked expression must
                    // survive too. Rx's value at the replacement point is
                    // "A op B" as computed by op_rx; the second occurrence
                    // only computes the same thing if A's and B's VALUES
                    // are unchanged between op_rx and op_ry. (Between the
                    // candidate MOV and op_ry there can be nothing but
                    // comments -- op_ry is the next non-OTHER node after
                    // the candidate -- so checking every node up to and
                    // including the candidate covers the whole window.)
                    if (operand_value_clobbered(scan, &curr->src_op)) break;  // A
                    if (operand_value_clobbered(scan, &op_rx->src_op)) break; // B

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

                                if (!trigger_allowed()) { scan = op_ry->next; continue; }

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
