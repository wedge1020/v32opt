#include "v32opt.h"

// ============================================================================
// promote.c -- stack-slot-to-register promotion passes
// ============================================================================
//
// Three independent optimization passes, all opt-in and off by default at
// every -O level (-fpromote-leaf, -fpromote-regs, -fpromote-loops):
//
//   pass_promote_stack_slots    (-fpromote-leaf)  -- whole-function
//     promotion for CALL-free (leaf) functions only. A promoted register's
//     value never has to survive a CALL, because there isn't one.
//   pass_promote_regs           (-fpromote-regs)  -- the same scalar-
//     replacement logic generalized to ordinary non-leaf functions, applied
//     independently within each CALL-free "segment" of the function (the
//     code before the first CALL, between each pair of CALLs, and after the
//     last one). Reduces to exactly what promote-leaf does on a function
//     that happens to have no CALLs at all. Only promotes within a segment
//     that is single-entry/single-exit -- no branch inside it may target
//     outside it, and nothing outside it may branch into its middle (see
//     the segment_safe guard in pass_promote_regs for why: anything else
//     lets code run past the load or before the flush this pass inserts).
//   pass_promote_loop_registers (-fpromote-loops) -- loop-invariant
//     promotion: hoists a repeatedly read/written stack slot into a
//     register for the duration of a single CALL-free loop, via a true
//     preheader load (runs once, before the loop header label) and stores
//     at every exit edge.
//
// All three follow the same shape: find a [BP-off] stack slot referenced
// enough times in a safe, CALL-free region to be worth a register, rewrite
// every reference in that region to the register FIRST, then splice in
// the load(s)/store(s) needed to keep the stack slot's own backing memory
// correct at the region's boundaries (function entry/return, segment/CALL
// boundaries, or loop preheader/exit, respectively) -- always AFTER the
// rewrite has finished with the original node range, never before (see the
// "BUG FIX" comments at each insertion site for why that order matters).
// See each function's own header comment for its specific safety guards.
//
// All three report through `-d` like every other pass in this project:
// insert_debug_comment logs an existing instruction's original text right
// before it's rewritten in place, and every newly-spliced load/store node
// gets its own descriptive debug comment immediately after insertion.
// ============================================================================

// -------------------------------------------------------------------
// Helper: Rewrite an operand to a general-purpose register
// -------------------------------------------------------------------
void promote_operand_to_reg(Operand *op, const char *reg_name) {
    op->mode = MODE_REG;
    op->offset = 0;
    op->immediate = 0;
    op->is_float = false;
    safe_str_copy(op->reg, reg_name, sizeof(op->reg));
    safe_str_copy(op->raw, reg_name, sizeof(op->raw));
}

// -------------------------------------------------------------------
// Helper: Extract the branch-target label text from a JMP/JT/JF node.
//
// JMP has a single operand (the target), which the parser's "single
// operand" convention stores in dst_op. JT/JF have TWO operands --
// the condition register first, the target label second -- e.g.
// "JT R0, __loop_end" parses to dst_op = "R0" (the register),
// src_op = "__loop_end" (the actual target). Reading dst_op for a
// JT/JF (as every loop-back-edge/branch-target check in this file
// used to) returns the CONDITION REGISTER NAME, which never matches
// any real label -- so those checks silently failed for every
// JT/JF-based branch: back-edges never matched (loops using a trailing
// "JT/JF start_lbl" back-edge were never detected at all), and the
// loop-body safety scan misread every internal JT/JF's condition
// register as an unrecognized jump target, marking virtually any loop
// containing a conditional (i.e. almost all real loops) unsafe.
// Confirmed by direct reproduction: -fpromote-loops applied 0
// optimizations on a 300K-line real program before this fix.
// -------------------------------------------------------------------
static char *branch_target_raw(AsmNode *n) {
    if (!n) return NULL;
    if (str_case_eq(n->mnemonic, "JT") || str_case_eq(n->mnemonic, "JF")) {
        return n->src_op.raw;
    }
    return n->dst_op.raw; // JMP: single operand, stored in dst_op
}

// -------------------------------------------------------------------
// Helper: Check if a label is defined inside a loop
// -------------------------------------------------------------------
bool is_inside_loop(const char *target, AsmNode *loop_start, AsmNode *loop_end) {
    for (AsmNode *n = loop_start->next; n && n != loop_end; n = n->next) {
        if (n->type == OP_LABEL) {
            char lbl[128] = {0};
            safe_str_copy(lbl, n->raw, sizeof(lbl));
            char *c = strchr(lbl, ':');
            if (c) *c = '\0';
            if (str_case_eq(trim(lbl), target)) return true;
        }
    }
    return false;
}

// -------------------------------------------------------------------
// Pass: Stack Slot to Register Promotion (Scalar Replacement)
// -------------------------------------------------------------------
int pass_promote_stack_slots(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr) {
        // 1. Identify Function Boundaries
        if (curr->type == OP_LABEL) {
            char line_copy[8192];
            safe_str_copy(line_copy, curr->raw, sizeof(line_copy));
            char *lbl = trim(line_copy);

            size_t lbl_len = strlen(lbl);
            bool is_return_label = (lbl_len >= 8 && str_case_eq(lbl + lbl_len - 8, "_return:"));
            bool is_global_scope_init = str_case_eq(lbl, "__global_scope_initialization:");
            if ((strncmp(lbl, "__function_", 11) != 0 && !is_global_scope_init) || is_return_label) {
                curr = curr->next;
                continue;
            }

            // Find the end of this function body
            AsmNode *scan = curr->next;
            AsmNode *end_of_func = curr;
            while (scan) {
                if (scan->type == OP_LABEL) {
                    char next_copy[8192];
                    safe_str_copy(next_copy, scan->raw, sizeof(next_copy));
                    char *next_lbl = trim(next_copy);
                    size_t next_lbl_len = strlen(next_lbl);
                    bool next_is_return = (next_lbl_len >= 8 && str_case_eq(next_lbl + next_lbl_len - 8, "_return:"));
                    if ((strncmp(next_lbl, "__function_", 11) == 0 && !next_is_return) ||
                        strncmp(next_lbl, "__literal_", 10) == 0 ||
                        (strncmp(next_lbl, "__global_", 9) == 0 && !str_case_eq(next_lbl, "__global_scope_initialization:"))) {
                        break;
                    }
                }
                end_of_func = scan;
                scan = scan->next;
            }

            // ----------------------------------------------------------------
            // Phase A: Analyze Function Safety & Register Liveness
            // ----------------------------------------------------------------
            bool is_leaf_function = true;
            bool address_taken = false;
            bool reg_used[16] = {0};
            int slot_counts[128] = {0};

            for (AsmNode *n = curr->next; n && n != end_of_func->next; n = n->next) {
                if (str_case_eq(n->mnemonic, "CALL")) {
                    is_leaf_function = false;
                }

                // Guardrail: Check if BP is used in arithmetic or indirect mode
                if (n->type != OP_PUSH && n->type != OP_POP && !str_case_eq(n->mnemonic, "MOV")) {
                    if ((n->dst_op.mode == MODE_REG && str_case_eq(n->dst_op.reg, "BP")) ||
                        (n->src_op.mode == MODE_REG && str_case_eq(n->src_op.reg, "BP")) ||
                        (n->dst_op.mode == MODE_INDIRECT && str_case_eq(n->dst_op.reg, "BP")) ||
                        (n->src_op.mode == MODE_INDIRECT && str_case_eq(n->src_op.reg, "BP"))) {
                        address_taken = true;
                        break;
                    }
                }

                // Track ALL register usage (direct + indirect)
                if (n->dst_op.mode == MODE_REG) {
                    int idx = get_reg_index(n->dst_op.reg);
                    if (idx >= 0 && idx < 16) reg_used[idx] = true;
                }
                if (n->src_op.mode == MODE_REG) {
                    int idx = get_reg_index(n->src_op.reg);
                    if (idx >= 0 && idx < 16) reg_used[idx] = true;
                }
                if (n->dst_op.mode == MODE_INDIRECT && n->dst_op.reg[0] != '\0') {
                    int idx = get_reg_index(n->dst_op.reg);
                    if (idx >= 0 && idx < 16) reg_used[idx] = true;
                }
                if (n->src_op.mode == MODE_INDIRECT && n->src_op.reg[0] != '\0') {
                    int idx = get_reg_index(n->src_op.reg);
                    if (idx >= 0 && idx < 16) reg_used[idx] = true;
                }

                // Count references to local stack variables: [BP-offset] where offset < 0
                if (n->dst_op.mode == MODE_INDIRECT && str_case_eq(n->dst_op.reg, "BP") && n->dst_op.offset < 0) {
                    int abs_off = -n->dst_op.offset;
                    if (abs_off < 128) slot_counts[abs_off]++;
                }
                if (n->src_op.mode == MODE_INDIRECT && str_case_eq(n->src_op.reg, "BP") && n->src_op.offset < 0) {
                    int abs_off = -n->src_op.offset;
                    if (abs_off < 128) slot_counts[abs_off]++;
                }
            }

            if (address_taken) {
                curr = end_of_func->next;
                continue;
            }

            // ----------------------------------------------------------------
            // Phase B: Execute Function-Wide Promotion (Leaf Functions Only)
            // ----------------------------------------------------------------
            if (is_leaf_function) {
                int free_reg_for_off[128] = {0}; // Track which register was assigned to each offset

                for (int off = 1; off < 128; off++) {
                    if (slot_counts[off] >= 3) {
                        int free_reg = -1;
                        for (int r = 1; r <= 13; r++) {
                            if (!reg_used[r]) {
                                free_reg = r;
                                free_reg_for_off[off] = free_reg;
                                reg_used[r] = true;
                                break;
                            }
                        }

                        if (free_reg == -1) break;

                        char reg_name[16];
                        snprintf(reg_name, sizeof(reg_name), "R%d", free_reg);

                        // Find insertion point right after prologue (after `MOV BP, SP`)
                        AsmNode *prologue_end = curr->next;
                        bool found_prologue = false;
                        while (prologue_end && prologue_end != end_of_func) {
                            if (str_case_eq(prologue_end->mnemonic, "MOV") &&
                                str_case_eq(prologue_end->dst_op.reg, "BP") &&
                                str_case_eq(prologue_end->src_op.reg, "SP")) {
                                found_prologue = true;
                                // BUG FIX: "MOV BP, SP" is very often
                                // immediately followed by "ISUB SP, N",
                                // which is what actually reserves this
                                // function's local stack space. If we stop
                                // right at "MOV BP, SP", SP hasn't been
                                // decremented yet, so a [BP-off] load
                                // spliced in here reads memory that isn't
                                // part of this function's frame at all --
                                // stale/garbage data left over from
                                // whatever was last above the current SP,
                                // not the local's real initial value.
                                // Confirmed live: nostalgick.asm's
                                // __function_Game_draw_pause_overlay had
                                // its promoted loads landing between
                                // "MOV BP, SP" and "ISUB SP, 24" -- reading
                                // uninitialized stack instead of the real
                                // locals. This is invisible to the
                                // optimizer itself (it runs clean either
                                // way) and only shows up once the
                                // generated code actually executes.
                                if (prologue_end->next &&
                                    str_case_eq(prologue_end->next->mnemonic, "ISUB") &&
                                    str_case_eq(prologue_end->next->dst_op.reg, "SP")) {
                                    prologue_end = prologue_end->next;
                                }
                                break;
                            }
                            prologue_end = prologue_end->next;
                        }

                        // BUG FIX: the old "if (prologue_end)" check here was
                        // nearly always true even when the "MOV BP, SP"
                        // pattern was never found, because the while loop
                        // above only stops on a match OR on reaching
                        // end_of_func -- a valid, non-NULL node either way.
                        // If the pattern was genuinely absent, prologue_end
                        // silently became end_of_func (the function's LAST
                        // node), and the load got spliced in right after it
                        // -- after every real use inside this function, and
                        // potentially into the following function's own
                        // code. Every [BP-off] reference below still gets
                        // rewritten to the register regardless, so that
                        // register would be read uninitialized throughout.
                        // Require an actual match; skip promoting this
                        // offset (not just this insertion) if there isn't
                        // one, since there's no other verified-safe place to
                        // put the initial load.
                        if (!found_prologue) {
                            free_reg_for_off[off] = 0;
                            continue;
                        }

                        // BUG FIX: this rewrite must run BEFORE the load is
                        // inserted, not after. The old order inserted the
                        // load first (right after prologue_end, i.e.
                        // INSIDE the [curr->next, end_of_func->next) range
                        // the rewrite loop below scans), so the freshly
                        // created "MOV R_free, [BP-off]" was immediately
                        // caught by its OWN rewrite -- its src operand IS a
                        // [BP-off] match for this exact offset -- and
                        // corrupted into "MOV R_free, R_free", a no-op that
                        // never actually loads the real stack value.
                        // Confirmed by direct reproduction: a simple 3-line
                        // leaf function promoting one local produced exactly
                        // this "MOV R4, R4" instead of "MOV R4, [BP-1]".
                        // Doing the rewrite first (over the original body,
                        // before the load node exists at all) and inserting
                        // the load only afterward makes it structurally
                        // impossible for the rewrite to ever see its own
                        // insertion.
                        //
                        // Rewrite all [BP-off] references in the function body to R_free
                        for (AsmNode *n = curr->next; n && n != end_of_func->next; n = n->next) {
                            bool modified = false;
                            if (n->dst_op.mode == MODE_INDIRECT && str_case_eq(n->dst_op.reg, "BP") && n->dst_op.offset == -off) {
                                promote_operand_to_reg(&n->dst_op, reg_name);
                                modified = true;
                            }
                            if (n->src_op.mode == MODE_INDIRECT && str_case_eq(n->src_op.reg, "BP") && n->src_op.offset == -off) {
                                promote_operand_to_reg(&n->src_op, reg_name);
                                modified = true;
                            }
                            if (modified) {
                                // Log the original instruction text before it's
                                // overwritten below -- same convention every
                                // other pass in this project follows.
                                insert_debug_comment(n->prev, OPT_PROMOTE_LEAF, n->raw);
                                if (n->has_dst && n->has_src) {
                                    snprintf(n->raw, sizeof(n->raw), "    %s %s, %s", n->mnemonic, n->dst_op.raw, n->src_op.raw);
                                } else if (n->has_dst) {
                                    snprintf(n->raw, sizeof(n->raw), "    %s %s", n->mnemonic, n->dst_op.raw);
                                }
                                optimizations++;
                            }
                        }

                        // Now insert the pre-header load, safely after the
                        // rewrite above has already finished with the
                        // original (pre-insertion) node range.
                        {
                            char src_str[32];
                            snprintf(src_str, sizeof(src_str), "[BP-%d]", off);
                            char load_raw[64];
                            snprintf(load_raw, sizeof(load_raw), "    MOV %s, %s", reg_name, src_str);

                            AsmNode *load_node = create_node(load_raw, OP_MOV, "MOV", reg_name, src_str);
                            load_node->has_dst = true;
                            load_node->has_src = true;
                            load_node->dst_op = parse_operand(reg_name);
                            load_node->src_op = parse_operand(src_str);

                            load_node->next = prologue_end->next;
                            load_node->prev = prologue_end;
                            if (prologue_end->next) prologue_end->next->prev = load_node;
                            prologue_end->next = load_node;

                            // Describe the newly-spliced load itself -- there's
                            // no "original" instruction to quote here (this
                            // node didn't exist before), so the debug comment
                            // states what it's for instead.
                            char dbg_msg[96];
                            snprintf(dbg_msg, sizeof(dbg_msg),
                                     "PROMOTE-LEAF: preheader load of [BP-%d] into %s (whole-function, leaf-only)",
                                     off, reg_name);
                            insert_debug_comment(load_node, OPT_PROMOTE_LEAF, dbg_msg);
                        }
                    }
                }

                // Insert stores before RET to save promoted values back to stack
                for (int off = 1; off < 128; off++) {
                    if (slot_counts[off] >= 3 && free_reg_for_off[off] != 0) {
                        char dst_str[32];
                        snprintf(dst_str, sizeof(dst_str), "[BP-%d]", off);
                        char reg_name[16];
                        snprintf(reg_name, sizeof(reg_name), "R%d", free_reg_for_off[off]);
                        char store_raw[64];
                        snprintf(store_raw, sizeof(store_raw), "    MOV %s, %s", dst_str, reg_name);

                        for (AsmNode *n = curr->next; n && n != end_of_func->next; n = n->next) {
                            if (str_case_eq(n->mnemonic, "RET")) {
                                AsmNode *store_node = create_node(store_raw, OP_MOV, "MOV", dst_str, reg_name);
                                store_node->prev = n->prev;
                                store_node->next = n;
                                if (n->prev) n->prev->next = store_node;
                                n->prev = store_node;
                                optimizations++;

                                char dbg_msg[96];
                                snprintf(dbg_msg, sizeof(dbg_msg),
                                         "PROMOTE-LEAF: flush %s back to [BP-%d] before RET",
                                         reg_name, off);
                                insert_debug_comment(store_node, OPT_PROMOTE_LEAF, dbg_msg);
                            }
                        }
                    }
                }
            }

            curr = end_of_func->next;
            continue;
        }
        curr = curr->next;
    }

    return optimizations;
}

// -------------------------------------------------------------------
// Pass: Stack Slot to Register Promotion (CALL-free segments)
//
// promote-leaf (pass_promote_stack_slots, above) only ever promotes a
// function that contains NO CALL anywhere, because a promoted register's
// value would be destroyed by any CALL (CALL clobbers R0-R13 -- see
// modifies_register). That's sound but leaves out the common case: an
// ordinary non-leaf function with one or more CALLs, where some local
// variable is still referenced >=3 times purely on ONE side of a call --
// never live across it.
//
// pass_promote_regs generalizes this: instead of requiring the WHOLE
// function to be call-free, it walks each function as a sequence of
// CALL-free "segments" -- the code before the first CALL, between each
// pair of consecutive CALLs, and after the last CALL -- and applies the
// same scalar-replacement logic INDEPENDENTLY within each segment. A
// promoted register's load and (if written) store are both confined to
// a single segment, so its value never has to survive a CALL, and
// nothing is shared or carried between segments even for the same stack
// offset -- each segment gets its own fresh load and, if the offset is
// referenced again in a later segment, that's an entirely separate,
// independent promotion with its own load/store pair.
//
// Note: for a function that happens to have NO calls at all, this
// reduces to exactly one segment covering the whole function -- the same
// promotion pass_promote_stack_slots (promote-leaf) would already do.
// Enabling both -fpromote-leaf and -fpromote-regs is safe (harmless,
// idempotent overlap on leaf functions specifically) but redundant on
// them; promote-regs's actual value-add is entirely in non-leaf
// functions.
// -------------------------------------------------------------------
int pass_promote_regs(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr) {
        // 1. Identify Function Boundaries (identical to pass_promote_stack_slots)
        if (curr->type == OP_LABEL) {
            char line_copy[8192];
            safe_str_copy(line_copy, curr->raw, sizeof(line_copy));
            char *lbl = trim(line_copy);

            size_t lbl_len = strlen(lbl);
            bool is_return_label = (lbl_len >= 8 && str_case_eq(lbl + lbl_len - 8, "_return:"));
            bool is_global_scope_init = str_case_eq(lbl, "__global_scope_initialization:");
            if ((strncmp(lbl, "__function_", 11) != 0 && !is_global_scope_init) || is_return_label) {
                curr = curr->next;
                continue;
            }

            // Find the end of this function body
            AsmNode *scan = curr->next;
            AsmNode *end_of_func = curr;
            while (scan) {
                if (scan->type == OP_LABEL) {
                    char next_copy[8192];
                    safe_str_copy(next_copy, scan->raw, sizeof(next_copy));
                    char *next_lbl = trim(next_copy);
                    size_t next_lbl_len = strlen(next_lbl);
                    bool next_is_return = (next_lbl_len >= 8 && str_case_eq(next_lbl + next_lbl_len - 8, "_return:"));
                    if ((strncmp(next_lbl, "__function_", 11) == 0 && !next_is_return) ||
                        strncmp(next_lbl, "__literal_", 10) == 0 ||
                        (strncmp(next_lbl, "__global_", 9) == 0 && !str_case_eq(next_lbl, "__global_scope_initialization:"))) {
                        break;
                    }
                }
                end_of_func = scan;
                scan = scan->next;
            }

            // GUARD: function-wide address-taken check -- identical
            // reasoning to pass_promote_stack_slots. If BP's address is
            // used anywhere (LEA, or any non-MOV/PUSH/POP instruction
            // touching BP directly or indirectly), no stack slot in this
            // function can be trusted to be reachable only via a literal
            // [BP-off] operand, so skip the whole function -- CALL
            // segmentation doesn't change this; it's unrelated to leafness.
            bool address_taken = false;
            for (AsmNode *n = curr->next; n && n != end_of_func->next; n = n->next) {
                if (n->type != OP_PUSH && n->type != OP_POP && !str_case_eq(n->mnemonic, "MOV")) {
                    if ((n->dst_op.mode == MODE_REG && str_case_eq(n->dst_op.reg, "BP")) ||
                        (n->src_op.mode == MODE_REG && str_case_eq(n->src_op.reg, "BP")) ||
                        (n->dst_op.mode == MODE_INDIRECT && str_case_eq(n->dst_op.reg, "BP")) ||
                        (n->src_op.mode == MODE_INDIRECT && str_case_eq(n->src_op.reg, "BP"))) {
                        address_taken = true;
                        break;
                    }
                }
            }
            if (address_taken) {
                curr = end_of_func->next;
                continue;
            }

            // ----------------------------------------------------------------
            // Walk the function in CALL-free segments: [seg_start, seg_limit)
            // where seg_limit is either the next CALL or end_of_func->next
            // (the function's end). Apply scalar replacement independently
            // within each segment -- nothing promoted in one segment is
            // ever assumed live in another.
            // ----------------------------------------------------------------
            AsmNode *seg_start = curr->next;
            while (seg_start && seg_start != end_of_func->next) {
                bool is_first_segment = (seg_start->prev == curr);

                AsmNode *seg_limit = seg_start;
                while (seg_limit && seg_limit != end_of_func->next &&
                       !str_case_eq(seg_limit->mnemonic, "CALL")) {
                    seg_limit = seg_limit->next;
                }
                // seg_limit is now either the CALL ending this segment, or
                // end_of_func->next (the function's end) -- an EXCLUSIVE
                // upper bound either way.

                // ----------------------------------------------------------------
                // BUG FIX / SAFETY GUARD: a segment only behaves like the
                // straight-line, single-entry/single-exit region this pass's
                // "load once at the top, flush once at the natural end (RET
                // or the ending CALL)" model assumes if NOTHING branches
                // into or out of its middle. Two ways that can be violated,
                // either of which lets code run with a promoted register's
                // value silently diverged from the real [BP-off] memory:
                //
                //   (a) a branch INSIDE the segment (JT/JF/JMP) jumps to a
                //       label OUTSIDE it -- an early exit that reaches code
                //       after the segment without ever running the flush
                //       this pass places at the segment's natural end.
                //   (b) a branch OUTSIDE the segment jumps to a label
                //       INSIDE it -- e.g. a loop back-edge landing on a
                //       loop header that happens to sit inside this
                //       CALL-free stretch -- reaching the segment's
                //       promoted code without ever running the load this
                //       pass places at the segment's start.
                //
                // Confirmed by direct reproduction: a synthetic function
                // with "JF R5, __after_call" jumping past a promoted
                // variable's flush-before-CALL left the promoted register
                // holding the right value while the stack slot it should
                // have been flushed to was never written -- code reached
                // via that branch read stale/garbage data straight from
                // memory. This is the exact failure reported against real
                // game code: a blank screen and 100% CPU immediately on
                // start, consistent with an early guard-clause branch (an
                // extremely common pattern -- parameter checks, short-
                // circuits) corrupting a promoted variable the very first
                // time it fired.
                //
                // -fpromote-loops already guards against this same class of
                // bug for loop bodies (its Rule 3 and "other entry" checks)
                // -- pass_promote_regs never had an equivalent, because
                // straight-line-only segments (no branches at all) were all
                // that got exercised by this pass's own test coverage
                // before now. Rather than track and patch every individual
                // entry/exit edge, require the segment be free of both
                // problems before promoting anything in it at all -- same
                // conservative approach already proven correct in
                // -fpromote-loops. This does cost some optimization
                // opportunities on segments with internal branching (common
                // in real code), but a segment that fails this check gets
                // skipped entirely -- promotion in every OTHER, cleaner
                // segment of the same function is unaffected.
                // ----------------------------------------------------------------
                bool segment_safe = true;

                // (a) every branch inside the segment must target a label
                // that is itself inside the segment.
                for (AsmNode *m = seg_start; segment_safe && m != seg_limit; m = m->next) {
                    if (str_case_eq(m->mnemonic, "JT") || str_case_eq(m->mnemonic, "JF") ||
                        str_case_eq(m->mnemonic, "JMP")) {
                        char target[128] = {0};
                        safe_str_copy(target, trim(branch_target_raw(m)), sizeof(target));
                        bool internal = false;
                        for (AsmNode *chk = seg_start; chk != seg_limit; chk = chk->next) {
                            if (chk->type == OP_LABEL) {
                                char lbl_copy[128] = {0};
                                safe_str_copy(lbl_copy, chk->raw, sizeof(lbl_copy));
                                char *c = strchr(lbl_copy, ':');
                                if (c) *c = '\0';
                                if (str_case_eq(trim(lbl_copy), target)) { internal = true; break; }
                            }
                        }
                        if (!internal) segment_safe = false;
                    }
                }

                // (b) no branch anywhere else in the enclosing function may
                // target a label inside the segment (bounded to the
                // function, not the whole program, for the same performance
                // reason -fpromote-loops's equivalent check is bounded --
                // label targets are function-local by this compiler's own
                // naming convention).
                if (segment_safe) {
                    bool inside_seg = false;
                    for (AsmNode *n = curr->next; n && n != end_of_func->next; n = n->next) {
                        if (n == seg_start) inside_seg = true;
                        if (!inside_seg &&
                            (str_case_eq(n->mnemonic, "JMP") || str_case_eq(n->mnemonic, "JT") ||
                             str_case_eq(n->mnemonic, "JF"))) {
                            char target[128] = {0};
                            safe_str_copy(target, trim(branch_target_raw(n)), sizeof(target));
                            for (AsmNode *chk = seg_start; chk != seg_limit; chk = chk->next) {
                                if (chk->type == OP_LABEL) {
                                    char lbl_copy[128] = {0};
                                    safe_str_copy(lbl_copy, chk->raw, sizeof(lbl_copy));
                                    char *c = strchr(lbl_copy, ':');
                                    if (c) *c = '\0';
                                    if (str_case_eq(trim(lbl_copy), target)) { segment_safe = false; break; }
                                }
                            }
                        }
                        if (n == seg_limit) inside_seg = false;
                        if (!segment_safe) break;
                    }
                }

                if (!segment_safe) {
                    seg_start = (seg_limit == end_of_func->next) ? end_of_func->next
                                                                   : (seg_limit ? seg_limit->next : NULL);
                    continue;
                }

                // ---- Analyze this segment: register usage + BP-off refs ----
                bool reg_used[16] = {0};
                int slot_counts[128] = {0};
                for (AsmNode *m = seg_start; m != seg_limit; m = m->next) {
                    if (m->dst_op.mode == MODE_REG) {
                        int idx = get_reg_index(m->dst_op.reg);
                        if (idx >= 0 && idx < 16) reg_used[idx] = true;
                    }
                    if (m->src_op.mode == MODE_REG) {
                        int idx = get_reg_index(m->src_op.reg);
                        if (idx >= 0 && idx < 16) reg_used[idx] = true;
                    }
                    if (m->dst_op.mode == MODE_INDIRECT && m->dst_op.reg[0] != '\0') {
                        int idx = get_reg_index(m->dst_op.reg);
                        if (idx >= 0 && idx < 16) reg_used[idx] = true;
                    }
                    if (m->src_op.mode == MODE_INDIRECT && m->src_op.reg[0] != '\0') {
                        int idx = get_reg_index(m->src_op.reg);
                        if (idx >= 0 && idx < 16) reg_used[idx] = true;
                    }
                    if (m->dst_op.mode == MODE_INDIRECT && str_case_eq(m->dst_op.reg, "BP") && m->dst_op.offset < 0) {
                        int abs_off = -m->dst_op.offset;
                        if (abs_off < 128) slot_counts[abs_off]++;
                    }
                    if (m->src_op.mode == MODE_INDIRECT && str_case_eq(m->src_op.reg, "BP") && m->src_op.offset < 0) {
                        int abs_off = -m->src_op.offset;
                        if (abs_off < 128) slot_counts[abs_off]++;
                    }
                }

                // For the FIRST segment (right after the function label),
                // loads must be inserted after the "MOV BP, SP" prologue,
                // exactly like pass_promote_stack_slots -- inserting before
                // it would read [BP-off] using the CALLER's BP, before this
                // function's own frame even exists. For every LATER segment
                // (one that starts right after a CALL), there's no prologue
                // to find or skip -- the insertion point is simply
                // seg_start itself.
                AsmNode *insert_after = NULL; // valid only when is_first_segment
                bool first_segment_prologue_found = false;
                if (is_first_segment) {
                    AsmNode *p = seg_start;
                    while (p && p != seg_limit) {
                        if (str_case_eq(p->mnemonic, "MOV") &&
                            str_case_eq(p->dst_op.reg, "BP") &&
                            str_case_eq(p->src_op.reg, "SP")) {
                            insert_after = p;
                            first_segment_prologue_found = true;
                            // BUG FIX: same trap as pass_promote_stack_slots
                            // -- "MOV BP, SP" is very often immediately
                            // followed by "ISUB SP, N", which is what
                            // actually reserves this function's local stack
                            // space. Stopping at "MOV BP, SP" alone means
                            // the load lands before SP is decremented, so it
                            // reads memory outside this function's frame
                            // (stale/garbage, not the local's real initial
                            // value). Skip past the ISUB too when present.
                            if (p->next &&
                                str_case_eq(p->next->mnemonic, "ISUB") &&
                                str_case_eq(p->next->dst_op.reg, "SP")) {
                                insert_after = p->next;
                            }
                            break;
                        }
                        p = p->next;
                    }
                }

                for (int off = 1; off < 128; off++) {
                    if (slot_counts[off] < 3) continue;

                    // BUG-CLASS GUARD (see pass_promote_stack_slots's own
                    // fix for the same trap): if this is the first segment
                    // and no "MOV BP, SP" was found, don't guess an
                    // insertion point -- skip promoting this offset rather
                    // than risk reading through the wrong BP.
                    if (is_first_segment && !first_segment_prologue_found) continue;

                    int free_reg = -1;
                    for (int r = 1; r <= 13; r++) {
                        if (!reg_used[r]) { free_reg = r; reg_used[r] = true; break; }
                    }
                    if (free_reg == -1) break;

                    char reg_name[16];
                    snprintf(reg_name, sizeof(reg_name), "R%d", free_reg);
                    char src_str[32];
                    snprintf(src_str, sizeof(src_str), "[BP-%d]", off);
                    char load_raw[64];
                    snprintf(load_raw, sizeof(load_raw), "    MOV %s, %s", reg_name, src_str);

                    // BUG FIX: the rewrite below must happen BEFORE the
                    // load node is inserted, not after -- see
                    // pass_promote_stack_slots's identical fix and comment
                    // for the reproduction. Inserting first (specifically
                    // via the "insert_after" placement, which lands INSIDE
                    // the [seg_start, seg_limit) range this rewrite scans)
                    // let the rewrite catch and corrupt its own freshly
                    // created "MOV R_free, [BP-off]" into "MOV R_free,
                    // R_free". Rewriting the original segment first, before
                    // the load node exists at all, makes that impossible.
                    bool slot_written = false;
                    for (AsmNode *m = seg_start; m != seg_limit; m = m->next) {
                        bool modified = false;
                        if (m->dst_op.mode == MODE_INDIRECT && str_case_eq(m->dst_op.reg, "BP") && m->dst_op.offset == -off) {
                            promote_operand_to_reg(&m->dst_op, reg_name);
                            modified = true;
                            slot_written = true;
                        }
                        if (m->src_op.mode == MODE_INDIRECT && str_case_eq(m->src_op.reg, "BP") && m->src_op.offset == -off) {
                            promote_operand_to_reg(&m->src_op, reg_name);
                            modified = true;
                        }
                        if (modified) {
                            // Log the original instruction text before it's
                            // overwritten below -- same convention every
                            // other pass in this project follows.
                            insert_debug_comment(m->prev, OPT_PROMOTE_REGS, m->raw);
                            if (m->has_dst && m->has_src) {
                                snprintf(m->raw, sizeof(m->raw), "    %s %s, %s", m->mnemonic, m->dst_op.raw, m->src_op.raw);
                            } else if (m->has_dst) {
                                snprintf(m->raw, sizeof(m->raw), "    %s %s", m->mnemonic, m->dst_op.raw);
                            }
                            optimizations++;
                        }
                    }

                    AsmNode *load_node = create_node(load_raw, OP_MOV, "MOV", reg_name, src_str);
                    load_node->has_dst = true;
                    load_node->has_src = true;
                    load_node->dst_op = parse_operand(reg_name);
                    load_node->src_op = parse_operand(src_str);

                    if (insert_after) {
                        // First segment, prologue found: splice right after
                        // it, same placement pass_promote_stack_slots uses.
                        load_node->next = insert_after->next;
                        load_node->prev = insert_after;
                        if (insert_after->next) insert_after->next->prev = load_node;
                        insert_after->next = load_node;
                    } else {
                        // A later segment (starts right after a CALL): splice
                        // right before seg_start -- already OUTSIDE the
                        // [seg_start, seg_limit) range the rewrite above
                        // scanned, so this placement was never at risk of
                        // the same self-catch bug. seg_start->prev is kept
                        // in sync by each insertion, so promoting multiple
                        // offsets in the same segment chains correctly.
                        load_node->prev = seg_start->prev;
                        load_node->next = seg_start;
                        if (seg_start->prev) seg_start->prev->next = load_node;
                        seg_start->prev = load_node;
                    }

                    // Describe the newly-spliced load -- no "original"
                    // instruction to quote (this node is brand new), so the
                    // debug comment states what it's for and which segment
                    // it belongs to instead.
                    {
                        char dbg_msg[128];
                        snprintf(dbg_msg, sizeof(dbg_msg),
                                 "PROMOTE-REGS: %s-segment load of [BP-%d] into %s (CALL-free segment, independent of any other segment)",
                                 is_first_segment ? "first" : "later", off, reg_name);
                        insert_debug_comment(load_node, OPT_PROMOTE_REGS, dbg_msg);
                    }

                    // Flush back to the stack slot at EVERY exit from this
                    // segment: any RET inside it, and the CALL that ends
                    // the segment (if any) -- that flush is what keeps a
                    // LATER segment's independent, fresh reload of the same
                    // offset correct.
                    if (slot_written) {
                        char dst_str[32];
                        snprintf(dst_str, sizeof(dst_str), "[BP-%d]", off);
                        char store_raw[64];
                        snprintf(store_raw, sizeof(store_raw), "    MOV %s, %s", dst_str, reg_name);

                        for (AsmNode *m = seg_start; m != seg_limit; m = m->next) {
                            if (str_case_eq(m->mnemonic, "RET")) {
                                AsmNode *store_node = create_node(store_raw, OP_MOV, "MOV", dst_str, reg_name);
                                store_node->prev = m->prev;
                                store_node->next = m;
                                if (m->prev) m->prev->next = store_node;
                                m->prev = store_node;
                                optimizations++;

                                char dbg_msg[96];
                                snprintf(dbg_msg, sizeof(dbg_msg),
                                         "PROMOTE-REGS: flush %s back to [BP-%d] before RET",
                                         reg_name, off);
                                insert_debug_comment(store_node, OPT_PROMOTE_REGS, dbg_msg);
                            }
                        }
                        if (seg_limit && str_case_eq(seg_limit->mnemonic, "CALL")) {
                            AsmNode *store_node = create_node(store_raw, OP_MOV, "MOV", dst_str, reg_name);
                            store_node->prev = seg_limit->prev;
                            store_node->next = seg_limit;
                            if (seg_limit->prev) seg_limit->prev->next = store_node;
                            seg_limit->prev = store_node;
                            optimizations++;

                            char dbg_msg[128];
                            snprintf(dbg_msg, sizeof(dbg_msg),
                                     "PROMOTE-REGS: flush %s back to [BP-%d] before CALL ends this segment (next segment reloads independently)",
                                     reg_name, off);
                            insert_debug_comment(store_node, OPT_PROMOTE_REGS, dbg_msg);
                        }
                    }
                }

                // BUG FIX: advancing to seg_limit->next unconditionally
                // overran the function boundary whenever a segment ended at
                // end_of_func->next (i.e. no further CALL -- the function's
                // last segment) rather than at an actual CALL node. In that
                // case seg_limit->next is ONE NODE PAST the function's own
                // end, and since the outer "while (seg_start != end_of_func
                // ->next)" condition compares by identity, a seg_start that
                // has skipped past that exact node never satisfies it again
                // -- the loop kept walking forward through the REST OF THE
                // PROGRAM as if it were still more segments of THIS
                // function, reusing this function's now-stale curr/
                // end_of_func. Confirmed by reproduction: -O3 combined with
                // -fpromote-regs did not finish in over a minute on a
                // 300K-line real program (isolated -fpromote-regs alone
                // converged in under a second -- the -O3 combination just
                // gave the bug more chances to fire across repeated
                // do-while passes). Setting seg_start to end_of_func->next
                // directly whenever this was the last segment makes the
                // outer loop's own termination check see it immediately,
                // instead of stepping past it.
                seg_start = (seg_limit == end_of_func->next) ? end_of_func->next
                                                               : (seg_limit ? seg_limit->next : NULL);
            }

            curr = end_of_func->next;
            continue;
        }
        curr = curr->next;
    }

    return optimizations;
}

// -------------------------------------------------------------------
// Pass: Loop-Invariant Register Promotion (CALL-Free Loops)
// -------------------------------------------------------------------
int pass_promote_loop_registers(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr) {
        // 1. Identify Loop Headers (e.g., __for_1_start:, __while_1_start:)
        if (curr->type == OP_LABEL) {
            char start_lbl[128] = {0};
            safe_str_copy(start_lbl, curr->raw, sizeof(start_lbl));
            char *colon = strchr(start_lbl, ':');
            if (colon) *colon = '\0';
            trim(start_lbl);

            size_t len = strlen(start_lbl);

            // BUG FIX: match against the label with one trailing "_<digits>"
            // disambiguation index stripped, not the raw label. This
            // heuristic was written against crisp-lib's (C-mode) own
            // labeling convention, where a distinguishing numeric ID
            // appears BEFORE the role suffix -- e.g. "__do_101780_start:"
            // -- so matching the literal last 6/5/7 characters works fine
            // there. The Lua-mode compiler instead appends its
            // disambiguating index AFTER the role suffix, to tell multiple
            // loops in the same function apart -- e.g.
            // "__Background_draw_clouds_for_gen_start_0:",
            // "__Game_draw_pause_overlay_for_start_3:". Against nostalgick
            // .asm specifically, every single real loop header (38 of them)
            // uses this trailing-index form and NONE matched the old
            // literal-suffix check -- this pass was silently a complete
            // no-op under Lua mode, not "no qualifying loops found". The
            // strip only ever removes one "_<digits>" group and only when
            // it's preceded by an underscore, so it does nothing to labels
            // that don't have one (crisp-lib's own labels are unaffected --
            // their digits are already mid-string, not trailing).
            char norm_lbl[128];
            safe_str_copy(norm_lbl, start_lbl, sizeof(norm_lbl));
            size_t norm_len = len;
            {
                size_t i = norm_len;
                while (i > 0 && isdigit((unsigned char)norm_lbl[i - 1])) i--;
                if (i > 0 && i < norm_len && norm_lbl[i - 1] == '_') {
                    norm_lbl[i - 1] = '\0';
                    norm_len = i - 1;
                }
            }

            // Match common compiler patterns
            bool is_loop_label = (norm_len >= 6 && str_case_eq(norm_lbl + norm_len - 6, "_start")) ||
                                 (norm_len >= 5 && str_case_eq(norm_lbl + norm_len - 5, "_for_")) ||
                                 (norm_len >= 7 && str_case_eq(norm_lbl + norm_len - 7, "_while_"));
            if (!is_loop_label) {
                curr = curr->next;
                continue;
            }

            // 2. Find the Loop Back-Edge (Unconditional JMP back to start_lbl)
            AsmNode *end_jmp = NULL;
            AsmNode *scan = curr->next;
            while (scan) {
                if (scan->type == OP_LABEL && strncmp(trim(scan->raw), "__function_", 11) == 0) {
                    break;
                }
                if ((str_case_eq(scan->mnemonic, "JMP") ||
                    str_case_eq(scan->mnemonic, "JT") ||
                    str_case_eq(scan->mnemonic, "JF")) &&
                    str_case_eq(trim(branch_target_raw(scan)), start_lbl)) {
                    end_jmp = scan;
                    break;
                }
                scan = scan->next;
            }

            if (!end_jmp) {
                curr = curr->next;
                continue;
            }

            // ----------------------------------------------------------------
            // GUARD: a preheader load (inserted just before start_lbl, so it
            // runs once on fall-through entry and is never touched by the
            // back-edge, which jumps straight to the label) is only sound if
            // fall-through is the ONLY way execution first reaches start_lbl.
            // A jump from WITHIN this loop's own body back to start_lbl is
            // fine (e.g. a "continue" -- it's just an alternate back-edge:
            // by the time it runs, the preheader has already executed on
            // first entry, exactly like the recognized back-edge). What's
            // NOT fine is a jump from OUTSIDE the loop -- a stray jump from
            // elsewhere in the function, or another loop reusing this label
            // -- landing on start_lbl directly, which would skip the
            // preheader and read the promoted register uninitialized.
            //
            // Bounded to the enclosing function rather than the whole
            // program: label targets are function-local by this compiler's
            // own naming convention (nothing legitimately jumps into a
            // "__forN_start"/"__whileN_start" label from a different
            // function), and re-scanning the full program for every loop
            // found was measured to take minutes on a 300K-line real
            // program -- proportional to function size instead keeps this
            // guard cheap.
            // ----------------------------------------------------------------
            AsmNode *func_start = curr;
            while (func_start->prev && !(func_start->prev->type == OP_LABEL &&
                   strncmp(trim(func_start->prev->raw), "__function_", 11) == 0)) {
                func_start = func_start->prev;
            }
            AsmNode *func_end = end_jmp;
            while (func_end->next && !(func_end->next->type == OP_LABEL &&
                   strncmp(trim(func_end->next->raw), "__function_", 11) == 0)) {
                func_end = func_end->next;
            }

            bool other_entry = false;
            bool inside_loop_range = false;
            for (AsmNode *n = func_start; n; n = n->next) {
                if (n == curr) inside_loop_range = true;
                if (!inside_loop_range &&
                    (str_case_eq(n->mnemonic, "JMP") || str_case_eq(n->mnemonic, "JT") ||
                     str_case_eq(n->mnemonic, "JF")) &&
                    str_case_eq(trim(branch_target_raw(n)), start_lbl)) {
                    other_entry = true;
                    break;
                }
                if (n == end_jmp) inside_loop_range = false;
                if (n == func_end) break;
            }
            if (other_entry) {
                curr = end_jmp->next;
                continue;
            }

            // 3. Find the designated Loop Exit Label
            AsmNode *exit_label_node = end_jmp->next;
            while (exit_label_node && exit_label_node->type == OP_OTHER &&
                  (exit_label_node->raw[0] == '\0' || exit_label_node->raw[0] == ';')) {
                exit_label_node = exit_label_node->next;
            }

            if (!exit_label_node || exit_label_node->type != OP_LABEL) {
                curr = end_jmp->next;
                continue;
            }

            char exit_lbl[128] = {0};
            safe_str_copy(exit_lbl, exit_label_node->raw, sizeof(exit_lbl));
            colon = strchr(exit_lbl, ':');
            if (colon) *colon = '\0';
            trim(exit_lbl);

            // ----------------------------------------------------------------
            // Phase A: Safety Scan across the Loop Body
            // ----------------------------------------------------------------
            bool loop_safe = true;
            bool reg_used[16] = {0};
            int slot_read[128] = {0};
            int slot_written[128] = {0};

            for (AsmNode *n = curr->next; n && n != end_jmp; n = n->next) {
                // Rule 1: No CALLs, RETs, or HLTs inside the loop
                if (str_case_eq(n->mnemonic, "CALL") || str_case_eq(n->mnemonic, "RET") || str_case_eq(n->mnemonic, "HLT")) {
                    loop_safe = false;
                    break;
                }

                // Rule 2: No BP Address-Taking (direct or indirect)
                if (n->type != OP_PUSH && n->type != OP_POP && !str_case_eq(n->mnemonic, "MOV")) {
                    if ((n->dst_op.mode == MODE_REG && str_case_eq(n->dst_op.reg, "BP")) ||
                        (n->src_op.mode == MODE_REG && str_case_eq(n->src_op.reg, "BP")) ||
                        (n->dst_op.mode == MODE_INDIRECT && str_case_eq(n->dst_op.reg, "BP")) ||
                        (n->src_op.mode == MODE_INDIRECT && str_case_eq(n->src_op.reg, "BP"))) {
                        loop_safe = false;
                        break;
                    }
                }

                // Rule 3: Verify all branches jump inside loop, to start, or to exit
                if (str_case_eq(n->mnemonic, "JT") || str_case_eq(n->mnemonic, "JF") || str_case_eq(n->mnemonic, "JMP")) {
                    char target[128] = {0};
                    safe_str_copy(target, trim(branch_target_raw(n)), sizeof(target));
                    if (!str_case_eq(target, start_lbl) && !str_case_eq(target, exit_lbl)) {
                        bool internal_lbl = false;
                        for (AsmNode *chk = curr->next; chk && chk != end_jmp; chk = chk->next) {
                            if (chk->type == OP_LABEL) {
                                char int_copy[128] = {0};
                                safe_str_copy(int_copy, chk->raw, sizeof(int_copy));
                                char *c = strchr(int_copy, ':');
                                if (c) *c = '\0';
                                if (str_case_eq(trim(int_copy), target)) { internal_lbl = true; break; }
                            }
                        }
                        if (!internal_lbl) { loop_safe = false; break; }
                    }
                }

                // Track ALL register usage (direct + indirect)
                if (n->dst_op.mode == MODE_REG) {
                    int idx = get_reg_index(n->dst_op.reg);
                    if (idx >= 0 && idx < 16) reg_used[idx] = true;
                }
                if (n->src_op.mode == MODE_REG) {
                    int idx = get_reg_index(n->src_op.reg);
                    if (idx >= 0 && idx < 16) reg_used[idx] = true;
                }
                if (n->dst_op.mode == MODE_INDIRECT && n->dst_op.reg[0] != '\0') {
                    int idx = get_reg_index(n->dst_op.reg);
                    if (idx >= 0 && idx < 16) reg_used[idx] = true;
                }
                if (n->src_op.mode == MODE_INDIRECT && n->src_op.reg[0] != '\0') {
                    int idx = get_reg_index(n->src_op.reg);
                    if (idx >= 0 && idx < 16) reg_used[idx] = true;
                }

                // Track stack slot reads and writes
                if (n->dst_op.mode == MODE_INDIRECT && str_case_eq(n->dst_op.reg, "BP") && n->dst_op.offset < 0) {
                    int abs_off = -n->dst_op.offset;
                    if (abs_off < 128) slot_written[abs_off]++;
                }
                if (n->src_op.mode == MODE_INDIRECT && str_case_eq(n->src_op.reg, "BP") && n->src_op.offset < 0) {
                    int abs_off = -n->src_op.offset;
                    if (abs_off < 128) slot_read[abs_off]++;
                }
            }

            if (!loop_safe) {
                curr = end_jmp->next;
                continue;
            }

            // ----------------------------------------------------------------
            // Phase B: Execute Loop Promotion
            // ----------------------------------------------------------------
            AsmNode *store_insert_pt = exit_label_node;
            int free_reg_for_off[128] = {0}; // Track register assignment per offset

            for (int off = 1; off < 128; off++) {
                if (slot_read[off] + slot_written[off] >= 2) {
                    int free_reg = -1;
                    for (int r = 1; r <= 13; r++) {
                        if (!reg_used[r]) {
                            free_reg = r;
                            free_reg_for_off[off] = free_reg;
                            reg_used[r] = true;
                            break;
                        }
                    }

                    if (free_reg == -1) break;

                    char reg_name[16];
                    snprintf(reg_name, sizeof(reg_name), "R%d", free_reg);

                    // 1. Insert Pre-Header Load BEFORE the loop start label.
                    //
                    // BUG FIX: inserting the load AFTER the label (the
                    // previous behavior) put it INSIDE the loop body --
                    // the back-edge jumps straight to the label, so anything
                    // placed right after it re-runs on every iteration. That
                    // reloaded the ORIGINAL stack value every single pass
                    // through the loop, silently discarding whatever the
                    // loop body had just computed into the register.
                    // Confirmed by direct reproduction: a synthetic counting
                    // loop's increment was lost on every iteration under the
                    // old placement (it "worked" only by coincidence, in
                    // ways that could not be relied on -- see below).
                    // Inserting before the label instead makes this a true
                    // preheader: it runs exactly once, on the fall-through
                    // entry into the loop (the GUARD added above confirms
                    // fall-through is the only way in), and the back-edge
                    // never touches it again.
                    //
                    // This placement also fixes a second, independent bug
                    // the old one caused as a side effect: the rewrite loop
                    // below (which retargets every [BP-off] reference inside
                    // the loop body to this register) scans starting at
                    // curr->next -- exactly where the old code inserted the
                    // load itself. So the freshly-inserted
                    // "MOV R_free, [BP-off]" was immediately caught by its
                    // OWN rewrite and corrupted into "MOV R_free, R_free" --
                    // a no-op that never actually loaded anything, leaving
                    // the register's starting value to whatever it happened
                    // to already hold. Inserting before curr keeps the load
                    // outside the [curr->next, end_jmp) range the rewrite
                    // loop scans, so it can no longer see or corrupt itself.
                    char src_str[32];
                    snprintf(src_str, sizeof(src_str), "[BP-%d]", off);
                    char load_raw[64];
                    snprintf(load_raw, sizeof(load_raw), "    MOV %s, %s", reg_name, src_str);

                    AsmNode *load_node = create_node(load_raw, OP_MOV, "MOV", reg_name, src_str);
                    load_node->has_dst = true;
                    load_node->has_src = true;
                    load_node->dst_op = parse_operand(reg_name);
                    load_node->src_op = parse_operand(src_str);

                    // Insert BEFORE the loop label (true preheader).
                    load_node->prev = curr->prev;
                    load_node->next = curr;
                    if (curr->prev) curr->prev->next = load_node;
                    curr->prev = load_node;

                    // Describe the newly-spliced preheader load -- no
                    // "original" instruction to quote (this node is brand
                    // new), so the debug comment states what it's for.
                    {
                        char dbg_msg[96];
                        snprintf(dbg_msg, sizeof(dbg_msg),
                                 "PROMOTE-LOOPS: preheader load of [BP-%d] into %s (runs once, before %s)",
                                 off, reg_name, start_lbl);
                        insert_debug_comment(load_node, OPT_PROMOTE_LOOPS, dbg_msg);
                    }

                    // 2. Insert Exit Store AFTER loop exit label (if slot was written)
                    if (slot_written[off] > 0) {
                        char dst_str[32];
                        snprintf(dst_str, sizeof(dst_str), "[BP-%d]", off);
                        char store_raw[64];
                        snprintf(store_raw, sizeof(store_raw), "    MOV %s, %s", dst_str, reg_name);

                        AsmNode *store_node = create_node(store_raw, OP_MOV, "MOV", dst_str, reg_name);
                        store_node->has_dst = true;
                        store_node->has_src = true;
                        store_node->dst_op = parse_operand(dst_str);
                        store_node->src_op = parse_operand(reg_name);

                        store_node->next = store_insert_pt->next;
                        store_node->prev = store_insert_pt;
                        if (store_insert_pt->next) store_insert_pt->next->prev = store_node;
                        store_insert_pt->next = store_node;
                        store_insert_pt = store_node;

                        char dbg_msg[96];
                        snprintf(dbg_msg, sizeof(dbg_msg),
                                 "PROMOTE-LOOPS: flush %s back to [BP-%d] at main loop exit (%s)",
                                 reg_name, off, exit_lbl);
                        insert_debug_comment(store_node, OPT_PROMOTE_LOOPS, dbg_msg);
                    }

                    // 3. Rewrite all [BP-off] references inside loop body to R_free
                    for (AsmNode *n = curr->next; n && n != end_jmp; n = n->next) {
                        bool modified = false;
                        if (n->dst_op.mode == MODE_INDIRECT && str_case_eq(n->dst_op.reg, "BP") && n->dst_op.offset == -off) {
                            promote_operand_to_reg(&n->dst_op, reg_name);
                            modified = true;
                        }
                        if (n->src_op.mode == MODE_INDIRECT && str_case_eq(n->src_op.reg, "BP") && n->src_op.offset == -off) {
                            promote_operand_to_reg(&n->src_op, reg_name);
                            modified = true;
                        }
                        if (modified) {
                            // Log the original instruction text before it's
                            // overwritten below -- same convention every
                            // other pass in this project follows.
                            insert_debug_comment(n->prev, OPT_PROMOTE_LOOPS, n->raw);
                            if (n->has_dst && n->has_src) {
                                snprintf(n->raw, sizeof(n->raw), "    %s %s, %s", n->mnemonic, n->dst_op.raw, n->src_op.raw);
                            } else if (n->has_dst) {
                                snprintf(n->raw, sizeof(n->raw), "    %s %s", n->mnemonic, n->dst_op.raw);
                            }
                            optimizations++;
                        }
                    }
                }
            }

            // Insert stores at ALL loop exits (not just the main one)
            for (int off = 1; off < 128; off++) {
                if (slot_read[off] + slot_written[off] >= 2 && free_reg_for_off[off] != 0 && slot_written[off] > 0) {
                    char dst_str[32];
                    snprintf(dst_str, sizeof(dst_str), "[BP-%d]", off);
                    char reg_name[16];
                    snprintf(reg_name, sizeof(reg_name), "R%d", free_reg_for_off[off]);
                    char store_raw[64];
                    snprintf(store_raw, sizeof(store_raw), "    MOV %s, %s", dst_str, reg_name);

                    // Find all jumps that exit the loop
                    for (AsmNode *n = curr->next; n && n != end_jmp; n = n->next) {
                        if ((str_case_eq(n->mnemonic, "JT") || str_case_eq(n->mnemonic, "JF") ||
                             str_case_eq(n->mnemonic, "JMP"))) {
                            char target[128] = {0};
                            safe_str_copy(target, trim(branch_target_raw(n)), sizeof(target));
                            if (!is_inside_loop(target, curr, end_jmp) &&
                                !str_case_eq(target, start_lbl) &&
                                !str_case_eq(target, exit_lbl)) {
                                // This is an exit - find the target label and insert store before it
                                AsmNode *exit_target = n->next;
                                while (exit_target && exit_target->type != OP_LABEL) {
                                    exit_target = exit_target->next;
                                }
                                if (exit_target) {
                                    AsmNode *store_node = create_node(store_raw, OP_MOV, "MOV", dst_str, reg_name);
                                    store_node->prev = exit_target->prev;
                                    store_node->next = exit_target;
                                    if (exit_target->prev) exit_target->prev->next = store_node;
                                    exit_target->prev = store_node;
                                    optimizations++;

                                    char dbg_msg[128];
                                    snprintf(dbg_msg, sizeof(dbg_msg),
                                             "PROMOTE-LOOPS: flush %s back to [BP-%d] at secondary loop exit (%s)",
                                             reg_name, off, target);
                                    insert_debug_comment(store_node, OPT_PROMOTE_LOOPS, dbg_msg);
                                }
                            }
                        }
                    }
                }
            }

            curr = end_jmp->next;
            continue;
        }
        curr = curr->next;
    }

    return optimizations;
}
