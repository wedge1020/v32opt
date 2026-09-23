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
// HELPER: Check if an instruction references the BP register DIRECTLY
// (register-mode operand), as opposed to [BP+N]/[BP-N] indirect access.
//
// NOTE: indirect [BP+N]/[BP-N] operands DO depend on BP pointing at this
// function's frame -- they just aren't flagged HERE because the frame
// elimination pass checks them separately (see omit_frame_pointers).
// This helper answers only "is BP itself an operand?".
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
                bool frame_used_in_body = false;
                AsmNode *epilogue_mov = NULL;
                AsmNode *epilogue_pop = NULL;

                // First pass: Find the epilogue
                AsmNode *scan = mov_bp_sp->next;
                while (scan)
                {
                    if (scan->type == OP_OTHER) {
                        scan = scan->next;
                        continue;
                    }

                    if (scan->type == OP_LABEL) {
                        char lbl[128];
                        safe_str_copy(lbl, scan->raw, sizeof(lbl));
                        char *colon = strchr(lbl, ':');
                        if (colon) *colon = '\0';
                        trim(lbl);

                        size_t lbl_len = strlen(lbl);
                        bool is_return_label = (lbl_len >= 7 && str_case_eq(lbl + lbl_len - 7, "_return"));

                        if (!is_return_label) {
                            frame_used_in_body = true;
                            break;
                        }
                        scan = scan->next;
                        continue;
                    }

                    // Check if we hit the Epilogue
                    if (scan->type == OP_MOV &&
                        scan->has_dst && str_case_eq(scan->dst_op.reg, "SP") &&
                        scan->has_src && str_case_eq(scan->src_op.reg, "BP")) {
                        AsmNode *next_node = scan->next;
                        while (next_node && next_node->type == OP_OTHER) next_node = next_node->next;

                        if (next_node && next_node->type == OP_POP && is_reg_op(next_node, "BP")) {
                            epilogue_mov = scan;
                            epilogue_pop = next_node;
                            break;
                        }
                    }

                    if (str_case_eq(scan->mnemonic, "RET")) {
                        break;
                    }

                    scan = scan->next;
                }

                // Second pass: Check for BP/SP usage in BODY ONLY (between prologue and epilogue)
                if (epilogue_mov && epilogue_pop) {
                    scan = mov_bp_sp->next;
                    while (scan && scan != epilogue_mov)
                    {
                        if (scan->type == OP_OTHER) {
                            scan = scan->next;
                            continue;
                        }

                        if (scan->type == OP_LABEL) {
                            char lbl[128];
                            safe_str_copy(lbl, scan->raw, sizeof(lbl));
                            char *colon = strchr(lbl, ':');
                            if (colon) *colon = '\0';
                            trim(lbl);

                            size_t lbl_len = strlen(lbl);
                            bool is_return_label = (lbl_len >= 7 && str_case_eq(lbl + lbl_len - 7, "_return"));

                            if (!is_return_label) {
                                frame_used_in_body = true;
                                break;
                            }
                            scan = scan->next;
                            continue;
                        }

                        // Check for BP usage: ANY BP-based reference --
                        // direct register use, or an indirect operand with
                        // ANY offset.
                        //
                        // BUG FIX: this used to flag only [BP+N] with
                        // offset >= 0, on the theory that "[BP-N] is local
                        // variable access, not BP register usage". That is
                        // backwards: [BP-N] is BP-RELATIVE addressing -- it
                        // needs BP to point at this function's frame just
                        // as much as [BP+N] does. A function whose locals
                        // live at [BP-N] (and that allocates them without
                        // touching SP, e.g. red-zone style) had its frame
                        // stripped, silently redirecting every local access
                        // into the CALLER's frame. All indirect BP
                        // references now block elimination.
                        bool bp_used = references_bp_direct(scan) ||
                                      (scan->has_dst && scan->dst_op.mode == MODE_INDIRECT &&
                                       str_case_eq(scan->dst_op.reg, "BP")) ||
                                      (scan->has_src && scan->src_op.mode == MODE_INDIRECT &&
                                       str_case_eq(scan->src_op.reg, "BP"));

                        // Check for SP usage. BUG FIX (two holes):
                        // 1. This used to be just modifies_register(scan,
                        //    "SP"), which MISSED "MOV R1, SP" (reading SP
                        //    is not modifying it) -- yet without the
                        //    prologue SP sits 2 words higher, so a body
                        //    that reads SP observes a different value.
                        //    Any SP reference (read or write, direct or
                        //    indirect) now blocks elimination.
                        // 2. CALL is deliberately EXEMPT here (unlike the
                        //    memory-traffic passes): the hardware
                        //    push/pop of the return address is net-zero
                        //    across the call, and both supported
                        //    compilers keep SP net-zero across calls
                        //    (v32lua cleans arguments in the caller, the
                        //    v32 C compiler stages them with stores
                        //    instead of pushes), so a body with no other
                        //    SP/BP reference is unaffected by the call's
                        //    temporary SP movement. An unbalanced callee
                        //    would corrupt its own RET with or without
                        //    this epilogue.
                        bool sp_used =
                            scan->type == OP_PUSH || scan->type == OP_POP ||
                            (scan->has_dst && scan->dst_op.mode == MODE_REG &&
                             str_case_eq(scan->dst_op.reg, "SP")) ||
                            (scan->has_src && scan->src_op.mode == MODE_REG &&
                             str_case_eq(scan->src_op.reg, "SP")) ||
                            (scan->has_dst && scan->dst_op.mode == MODE_INDIRECT &&
                             str_case_eq(scan->dst_op.reg, "SP")) ||
                            (scan->has_src && scan->src_op.mode == MODE_INDIRECT &&
                             str_case_eq(scan->src_op.reg, "SP"));

                        if (bp_used || sp_used) {
                            frame_used_in_body = true;
                            break;
                        }

                        if (str_case_eq(scan->mnemonic, "RET")) {
                            break;
                        }

                        scan = scan->next;
                    }
                } else {
                    // No epilogue found, can't eliminate
                    frame_used_in_body = true;
                }

                // 4. The Verdict
                if (!frame_used_in_body && epilogue_mov && epilogue_pop)
                {
                    AsmNode *nodes[] = {push_bp, mov_bp_sp, epilogue_mov, epilogue_pop};
                    if (remove_with_debug(&curr, nodes, 4, OPT_OMIT_FRAME_POINTERS)) optimizations += 4;
                    continue;
                }
            }
        }
        curr = curr->next;
    }

    return optimizations;
}
