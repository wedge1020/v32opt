#include "v32opt.h"

// ===================================================================
// HELPER: Check if a single-operand instruction uses a specific register
// regardless of whether the parser stored it in dst_op or src_op.
// ===================================================================
bool is_reg_op(AsmNode *node, const char *reg_name) {
    if (!node || !reg_name) return false;
    if (node->has_dst && str_case_eq(node->dst_op.reg, reg_name)) return true;
    if (node->has_src && str_case_eq(node->src_op.reg, reg_name)) return true;
    return false;
}

// ===================================================================
// HELPER: Check if an instruction references the BP register
// ===================================================================
bool references_bp(AsmNode *node) {
    if (!node) return false;

    // Only flag DIRECT BP usage (register mode)
    if (node->has_dst && node->dst_op.mode == MODE_REG) {
        if (str_case_eq(node->dst_op.reg, "BP")) return true;
    }
    if (node->has_src && node->src_op.mode == MODE_REG) {
        if (str_case_eq(node->src_op.reg, "BP")) return true;
    }
    // DO NOT flag [BP+N] - that's local variable access, not BP register usage
    return false;
}

// ===================================================================
// HELPER: Check if an instruction uses BP DIRECTLY (not [BP-N])
// [BP-N] is local variable access - does NOT require BP register
// ===================================================================
bool references_bp_direct(AsmNode *node) {
    if (!node) return false;

    // Check destination operand for DIRECT BP usage (register mode only)
    if (node->has_dst && node->dst_op.mode == MODE_REG) {
        if (str_case_eq(node->dst_op.reg, "BP")) return true;
    }

    // Check source operand for DIRECT BP usage (register mode only)
    if (node->has_src && node->src_op.mode == MODE_REG) {
        if (str_case_eq(node->src_op.reg, "BP")) return true;
    }

    // DO NOT flag [BP+N] - that's local variable access, not BP itself
    return false;
}

// ===================================================================
// PASS: Frame Pointer Elimination (Stack Frame Elision)
// Scans entire functions. If BP is never referenced in the body,
// strips the standard prologue (push BP / mov BP, SP) and
// epilogue (mov SP, BP / pop BP).
// ===================================================================
int omit_frame_pointers(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        // 1. Detect standard Function Prologue: PUSH BP followed by MOV BP, SP
        if (curr->type == OP_PUSH && is_reg_op(curr, "BP"))
        {
            AsmNode *push_bp = curr;
            AsmNode *mov_bp_sp = push_bp->next;

            // Skip ANY comments/blank lines between PUSH BP and MOV BP, SP
            while (mov_bp_sp && mov_bp_sp->type == OP_OTHER) mov_bp_sp = mov_bp_sp->next;

            if (mov_bp_sp && mov_bp_sp->type == OP_MOV &&
                mov_bp_sp->has_dst && str_case_eq(mov_bp_sp->dst_op.reg, "BP") &&
                mov_bp_sp->has_src && str_case_eq(mov_bp_sp->src_op.reg, "SP"))
            {
                bool bp_used_in_body = false;
                AsmNode *epilogue_mov = NULL;
                AsmNode *epilogue_pop = NULL;

                AsmNode *scan = mov_bp_sp->next;
                while (scan)
                {
                    // Skip comments/blanks
                    if (scan->type == OP_OTHER) {
                        scan = scan->next;
                        continue;
                    }

                    // FIX: Only abort on non-return labels (return labels are part of function)
                    if (scan->type == OP_LABEL) {
                        char lbl[128];
                        safe_str_copy(lbl, scan->raw, sizeof(lbl));
                        char *colon = strchr(lbl, ':');
                        if (colon) *colon = '\0';
                        trim(lbl);

                        // Check if this is a return label (__function_*_return:)
                        size_t lbl_len = strlen(lbl);
                        bool is_return_label = (lbl_len >= 8 && 
                                               str_case_eq(lbl + lbl_len - 8, "_return"));

                        if (!is_return_label) {
                            // Non-return label = nested control flow, abort
                            bp_used_in_body = true;
                            break;
                        }
                        // For return labels, continue scanning - they're part of the function
                        scan = scan->next;
                        continue;
                    }

                    // Check if we hit the Epilogue: MOV SP, BP followed by POP BP
                    if (scan->type == OP_MOV &&
                        scan->has_dst && str_case_eq(scan->dst_op.reg, "SP") &&
                        scan->has_src && str_case_eq(scan->src_op.reg, "BP"))
                    {
                        AsmNode *next_node = scan->next;
                        while (next_node && next_node->type == OP_OTHER) next_node = next_node->next;

                        if (next_node && next_node->type == OP_POP &&
                            is_reg_op(next_node, "BP"))
                        {
                            epilogue_mov = scan;
                            epilogue_pop = next_node;
                            scan = next_node->next;
                            continue; // Do not flag the epilogue itself as "using BP"!
                        }
                    }

                    // Stop scanning when we reach the return instruction
                    if (str_case_eq(scan->mnemonic, "RET")) {
                        break;
                    }

                    // FIX: Only flag DIRECT BP usage (not [BP-N] which is local variable access)
                    if (scan->has_dst && scan->dst_op.mode == MODE_REG && str_case_eq(scan->dst_op.reg, "BP")) {
                        bp_used_in_body = true;
                        break;
                    }
                    if (scan->has_src && scan->src_op.mode == MODE_REG && str_case_eq(scan->src_op.reg, "BP")) {
                        bp_used_in_body = true;
                        break;
                    }

                    scan = scan->next;
                }

                // 4. The Verdict: If BP was never touched in the body, eliminate the frame!
                if (!bp_used_in_body && epilogue_mov && epilogue_pop)
                {
                    push_bp->type = OP_OTHER;
                    snprintf(push_bp->raw, sizeof(push_bp->raw), "; optimized out frame: push BP");

                    mov_bp_sp->type = OP_OTHER;
                    snprintf(mov_bp_sp->raw, sizeof(mov_bp_sp->raw), "; optimized out frame: mov BP, SP");

                    epilogue_mov->type = OP_OTHER;
                    snprintf(epilogue_mov->raw, sizeof(epilogue_mov->raw), "; optimized out frame: mov SP, BP");

                    epilogue_pop->type = OP_OTHER;
                    snprintf(epilogue_pop->raw, sizeof(epilogue_pop->raw), "; optimized out frame: pop BP");

                    optimizations += 4;
                    curr = epilogue_pop; // Fast-forward our outer loop
                }
            }
        }
        curr = curr->next;
    }

    return optimizations;
}
