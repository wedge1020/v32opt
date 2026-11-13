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
// FRAME CLEANUP (part of omit-frame-pointers): what the other passes leave
// of a function's frame once its locals live in registers, or once full
// unrolling has made a promoted loop counter disappear.
//
//   (1) Orphan register saves. "PUSH Rn" in the prologue, after the frame
//       setup, with "POP Rn" in every epilogue, where Rn (R1-R10) appears
//       nowhere else in the function: both go.
//   (2) Unused frame. "ISUB SP, N" in the prologue when no [BP-k] (k >= 1)
//       is left: it goes ("MOV SP, BP" in the epilogue needs no change).
//
// Once both are gone a small leaf function has nothing but "PUSH BP / MOV
// BP, SP ... MOV SP, BP / POP BP", which the pass below then removes.
//
// The function (promote's boundaries: "__function_X" up to the next
// function, literal or Lua global label) must be exactly
//
//     PUSH BP / MOV BP, SP / [ISUB SP, N] / [PUSH Ra / PUSH Rb ...]
//     body
//     (at every RET)  [... POP Rb / POP Ra] / MOV SP, BP / POP BP / RET
//
// with the same saves popped in reverse order at every RET, its last
// instruction a RET, JMP or HLT, and in the body:
//   - no jump out of the function, and no computed jump;
//   - no directive, no escape of BP's or SP's value (promote's
//     bp_value_escapes / sp_value_escapes: "MOV R1, SP", "LEA R1, [BP-2]",
//     ...), no other write of BP or SP than PUSH/POP and "ISUB/IADD SP,
//     <integer>";
//   - the body's own pushes balanced at every label and every jump (so the
//     number of words pushed below the saves is known at each instruction).
// Removing a save or the ISUB moves everything pushed after it one word (N
// words) up. "[SP+k]" moves along with it -- unless it reaches past those
// pushed words into the removed one(s): k must stay below the number of
// words pushed after the removed save (after the ISUB) at that point. That
// rules out a call's arguments staged with "MOV [SP+k], Rx" into the space
// the ISUB reserved. [BP+k] does not move; [BP-k] must stay inside the ISUB
// area for (1), and must not exist for (2).
// (1) also refuses a function with a CALL: under the stock compiler's
// callee-save convention a save the function no longer needs for itself
// may still be protecting its caller from a callee that does not preserve
// Rn (testing/lint: the stock compiler is not consistently callee-save).
//
// TRIGGER CAP: one slot per removed save (its PUSH and all its POPs), one
// per removed ISUB.
// ===================================================================
#define FC_MAX_SAVES 16
#define FC_MAX_EPI   16

static AsmNode *fc_next_real(AsmNode *n, AsmNode *limit, bool *directive) {
    for (; n && n != limit; n = n->next) {
        if (n->type != OP_OTHER) return n;
        if (is_directive_node(n)) { *directive = true; return NULL; }
    }
    return NULL;
}

static AsmNode *fc_prev_real(AsmNode *n) {
    for (n = n ? n->prev : NULL; n && n->type == OP_OTHER && !is_directive_node(n); n = n->prev) ;
    return n;
}

static int fc_reg(const AsmNode *n) {           // PUSH/POP Rn -> n (R1-R13), else -1
    if (!n->has_dst || n->dst_op.mode != MODE_REG) return -1;
    if (toupper((unsigned char)n->dst_op.reg[0]) != 'R') return -1;
    int r = get_reg_index(n->dst_op.reg);
    return (r >= 1 && r <= 13) ? r : -1;
}

static bool fc_is(const AsmNode *n, OpType t, const char *dst, const char *src) {
    if (!n || n->type != t) return false;
    if (dst && !(n->has_dst && n->dst_op.mode == MODE_REG && str_case_eq(n->dst_op.reg, dst))) return false;
    if (src && !(n->has_src && n->src_op.mode == MODE_REG && str_case_eq(n->src_op.reg, src))) return false;
    return true;
}

static bool fc_int_imm(const Operand *op, long *v) {
    if (op->mode != MODE_IMMEDIATE || op->is_float || !is_numeric_immediate(op)) return false;
    *v = op->immediate;
    return true;
}

static bool fc_label_in(const char *name, AsmNode *from, AsmNode *limit) {
    char lbl[128];
    for (AsmNode *n = from; n && n != limit; n = n->next) {
        if (n->type != OP_LABEL) continue;
        get_label_name(n, lbl, sizeof(lbl));
        if (name_eq(lbl, name)) return true;
    }
    return false;
}

static int fc_function(AsmNode *entry, AsmNode *last) {
    AsmNode *first = entry->next, *limit = last->next;
    bool dir = false;

    // ---- prologue ----
    AsmNode *push_bp = fc_next_real(first, limit, &dir);
    if (!fc_is(push_bp, OP_PUSH, "BP", NULL)) return 0;
    AsmNode *mov_bp = fc_next_real(push_bp->next, limit, &dir);
    if (!fc_is(mov_bp, OP_MOV, "BP", "SP")) return 0;
    AsmNode *p = fc_next_real(mov_bp->next, limit, &dir);
    AsmNode *isub = NULL;
    long frame_n = 0;
    if (fc_is(p, OP_ISUB, "SP", NULL)) {
        if (!fc_int_imm(&p->src_op, &frame_n) || frame_n <= 0) return 0;
        isub = p;
        p = fc_next_real(p->next, limit, &dir);
    }
    AsmNode *save[FC_MAX_SAVES];
    int save_reg[FC_MAX_SAVES], nsaves = 0;
    while (p && p->type == OP_PUSH && fc_reg(p) > 0) {
        if (nsaves == FC_MAX_SAVES) return 0;
        save[nsaves] = p;
        save_reg[nsaves++] = fc_reg(p);
        p = fc_next_real(p->next, limit, &dir);
    }
    if (dir || !p) return 0;
    if (!isub && nsaves == 0) return 0;               // nothing to clean up
    AsmNode *body = p;

    // ---- epilogues: at every RET, the saves popped in reverse ----
    AsmNode *epi_first[FC_MAX_EPI], *epi_ret[FC_MAX_EPI];
    AsmNode *pops[FC_MAX_EPI][FC_MAX_SAVES];
    int nepi = 0;
    AsmNode *last_real = NULL;
    for (AsmNode *n = body; n && n != limit; n = n->next) {
        if (n->type == OP_OTHER) { if (is_directive_node(n)) return 0; continue; }
        if (n->type != OP_LABEL) last_real = n;
        if (n->type != OP_RET) continue;
        if (nepi == FC_MAX_EPI) return 0;
        AsmNode *pb = fc_prev_real(n);
        if (!fc_is(pb, OP_POP, "BP", NULL)) return 0;
        AsmNode *ms = fc_prev_real(pb);
        if (!fc_is(ms, OP_MOV, "SP", "BP")) return 0;
        AsmNode *q = ms;
        for (int i = 0; i < nsaves; i++) {           // POP of save[i] is the (i+1)-th above MOV SP, BP
            q = fc_prev_real(q);
            if (!q || q->type != OP_POP || fc_reg(q) != save_reg[i]) return 0;
            pops[nepi][i] = q;
        }
        AsmNode *before = fc_prev_real(q);
        if (before && before->type == OP_POP) return 0;   // more pops than saves: not ours to judge
        epi_first[nepi] = q;
        epi_ret[nepi] = n;
        nepi++;
    }
    if (nepi == 0 || !last_real ||
        !(last_real->type == OP_RET || last_real->type == OP_JMP || last_real->type == OP_HLT)) return 0;

    // ---- body walk ----
    long depth = 0;                                   // words the body has pushed below the saves
    long sp_excess = -0x7fffffffL;                    // max over [SP+k] operands of k - depth
    long bp_neg = 0;                                  // deepest [BP-k]
    bool mentioned[11] = { false };
    bool has_call = false;
    for (AsmNode *n = body; n && n != limit; n = n->next) {
        if (n->type == OP_OTHER) continue;            // comments (directives refused above)
        int e;
        for (e = 0; e < nepi; e++) if (n == epi_first[e]) break;
        if (e < nepi) {
            if (depth != 0) return 0;
            n = epi_ret[e];
            continue;
        }
        if (n->type == OP_LABEL) { if (depth != 0) return 0; continue; }
        if (n->type == OP_RET) return 0;              // (every RET is an epilogue's)
        if (promote_bp_value_escapes(n) || promote_sp_value_escapes(n)) return 0;

        if (n->type == OP_JMP || n->type == OP_JT || n->type == OP_JF) {
            char tb[128];
            const char *tgt = loop_jump_target(n, tb, sizeof(tb));
            if (!tgt || !fc_label_in(tgt, first, limit)) return 0;   // computed, or leaves the function
            if (depth != 0) return 0;
        }
        if (n->type == OP_CALL) has_call = true;

        long v;
        if (n->type == OP_PUSH) {
            depth++;
        } else if (n->type == OP_POP) {
            if (fc_is(n, OP_POP, "BP", NULL) || fc_is(n, OP_POP, "SP", NULL)) return 0;
            if (--depth < 0) return 0;
        } else if (fc_is(n, OP_ISUB, "SP", NULL)) {
            if (!fc_int_imm(&n->src_op, &v) || v < 0) return 0;
            depth += v;
        } else if (fc_is(n, OP_IADD, "SP", NULL)) {
            if (!fc_int_imm(&n->src_op, &v) || v < 0) return 0;
            depth -= v;
            if (depth < 0) return 0;
        } else if (modifies_register(n, "SP") && n->type != OP_CALL) {
            return 0;
        }
        if (n->type != OP_PUSH && modifies_register(n, "BP")) return 0;

        const Operand *ops[2] = { n->has_dst ? &n->dst_op : NULL, n->has_src ? &n->src_op : NULL };
        for (int i = 0; i < 2; i++) {
            const Operand *op = ops[i];
            if (!op || op->mode != MODE_INDIRECT) continue;
            if (str_case_eq(op->reg, "SP")) {
                if ((long)op->offset - depth > sp_excess) sp_excess = (long)op->offset - depth;
            } else if (str_case_eq(op->reg, "BP")) {
                if (op->offset < 0 && -(long)op->offset > bp_neg) bp_neg = -(long)op->offset;
            }
        }
        for (int r = 1; r <= 10; r++) {
            char nm[8];
            snprintf(nm, sizeof(nm), "R%d", r);
            if (!mentioned[r] && loop_mentions_reg(n, nm)) mentioned[r] = true;
        }
    }

    int removed = 0;

    // ---- (1) orphan saves, innermost first ----
    bool kept[FC_MAX_SAVES];
    int kept_after = 0;                               // kept saves pushed after save[i]
    for (int i = nsaves - 1; i >= 0; i--) {
        kept[i] = true;
        int r = save_reg[i];
        if (r <= 10 && !mentioned[r] && !has_call && bp_neg <= frame_n &&
            sp_excess < kept_after) {
            AsmNode *nodes[1 + FC_MAX_EPI];
            int cnt = 0;
            nodes[cnt++] = save[i];
            for (int e = 0; e < nepi; e++) nodes[cnt++] = pops[e][i];
            AsmNode *dummy = NULL;
            if (remove_with_debug(&dummy, nodes, cnt, OPT_OMIT_FRAME_POINTERS)) {
                removed += cnt;
                kept[i] = false;
            }
        }
        if (kept[i]) kept_after++;
    }

    // ---- (2) unused frame ----
    if (isub && bp_neg == 0 && sp_excess < kept_after) {
        AsmNode *nodes[1] = { isub };
        AsmNode *dummy = NULL;
        if (remove_with_debug(&dummy, nodes, 1, OPT_OMIT_FRAME_POINTERS)) removed++;
    }
    return removed;
}

// promote_labels_private() on the table omit_frame_pointers() already built (loop_labels_build_all:
// the same counting rules), so the frame cleanup does not build a second one on every iteration.
static int  *g_fc_inside = NULL;
static int  *g_fc_touched = NULL;
static int   g_fc_ntouched = 0;

static void fc_count_inside(LoopLabel *l, const char *tok, void *arg) {
    (void)tok; (void)arg;
    int i = (int)(l - g_loop_labels);
    if (g_fc_inside[i]++ == 0) g_fc_touched[g_fc_ntouched++] = i;
}

static bool fc_labels_private(AsmNode *first, AsmNode *last) {
    if (!g_fc_inside) return false;
    g_fc_ntouched = 0;
    bool ok = true;
    // every label token inside the range is counted once per occurrence; the touched list is at most
    // the number of distinct labels, which the arrays are sized for
    for (AsmNode *n = first; n; n = n->next) {
        if (n->type != OP_LABEL && !n->debug_note) loop_label_tokens_all(n, fc_count_inside, NULL);
        if (n == last) break;
    }
    for (AsmNode *n = first; n && ok; n = n->next) {
        if (n->type == OP_LABEL) {
            char nm[128];
            get_label_name(n, nm, sizeof(nm));
            LoopLabel *l = loop_label_find(nm);
            if (!l || l->duplicate || g_fc_inside[l - g_loop_labels] != l->refs) ok = false;
        }
        if (n == last) break;
    }
    for (int k = 0; k < g_fc_ntouched; k++) g_fc_inside[g_fc_touched[k]] = 0;
    return ok;
}

static int frame_cleanup(AsmNode *head)
{
    int removed = 0;
    AsmNode *curr = head->next;
    promote_bounds_begin(head);
    while (curr) {
        if (!promote_function_entry(curr)) { curr = curr->next; continue; }
        AsmNode *last = promote_function_last(curr);
        AsmNode *limit = last->next;
        // Refused too when a label inside the function is reached from elsewhere (a shared epilogue
        // that another function jumps into: its stack has that function's saves, not ours). The entry
        // label is exempt: whatever jumps there -- a CALL, or a tail JMP after the jumper has taken its
        // own frame down -- arrives with the stack a CALL would leave, and the prologue/epilogue pair
        // stays balanced whatever this pass removes from both.
        if (promote_function_complete(curr) && fc_labels_private(curr->next, last))
            removed += fc_function(curr, last);
        curr = limit;
    }
    return removed;
}

// omit-frame-pointers: a "_return" label inside the body may be jumped to only from this same function
// (from `push_bp` to the RET after it). BUG FIX: another function jumping to it (a shared epilogue) would,
// once the frame is gone, return with ITS frame still on the stack. Counts come from loop_labels_build_all().
static bool return_label_private(const AsmNode *label, AsmNode *push_bp) {
    char nm[128];
    get_label_name(label, nm, sizeof(nm));
    LoopLabel *l = loop_label_find(nm);
    if (!l || l->duplicate) return false;
    int inside = 0;
    for (AsmNode *n = push_bp; n; n = n->next) {
        if (n->type != OP_LABEL && !n->debug_note && loop_mentions_reg(n, nm)) inside++;
        if (n->type == OP_RET) break;
    }
    return inside >= l->refs;
}

// ===================================================================
// PASS: Frame Pointer Elimination (Stack Frame Elision)
// Scans entire functions. If BP is never referenced in the body,
// strips the standard prologue (push BP / mov BP, SP) and
// epilogue (mov SP, BP / pop BP).
// ===================================================================
int omit_frame_pointers(AsmNode *head)
{
    if (!head) return 0;
    loop_labels_build_all(head);
    g_fc_inside  = calloc((size_t)g_loop_labels_n + 1, sizeof(int));
    g_fc_touched = malloc(((size_t)g_loop_labels_n + 1) * sizeof(int));
    if (!g_fc_touched) { free(g_fc_inside); g_fc_inside = NULL; }
    int optimizations = frame_cleanup(head);
    free(g_fc_inside); free(g_fc_touched);
    g_fc_inside = NULL; g_fc_touched = NULL;
    AsmNode *curr = head->next;

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

                        if ((!is_return_label && !loop_label_unreferenced(scan)) || !return_label_private(scan, push_bp)) {   // an unreferenced label is only fallen into; a _return label only jumped to from this body
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

                            if ((!is_return_label && !loop_label_unreferenced(scan)) || !return_label_private(scan, push_bp)) {   // an unreferenced label is only fallen into; a _return label only jumped to from this body
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

                        // A jump may only go to a label of this body (in practice
                        // its "_return" label): one to code past this epilogue
                        // (another epilogue with its own POP BP) or out of the
                        // function, or a computed one, would run with a frame
                        // that is no longer there.
                        bool bad_jump = false;
                        if (scan->type == OP_JMP || scan->type == OP_JT || scan->type == OP_JF) {
                            char tb[128];
                            const char *tgt = loop_jump_target(scan, tb, sizeof(tb));
                            LoopLabel *tl = tgt ? loop_label_find(tgt) : NULL;
                            bad_jump = true;
                            if (tl && !tl->duplicate) {
                                for (AsmNode *q = mov_bp_sp->next; q && q != epilogue_mov; q = q->next)
                                    if (q == tl->node) { bad_jump = false; break; }
                            }
                        }

                        if (bp_used || sp_used || bad_jump) {
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

    loop_labels_free();
    return optimizations;
}
