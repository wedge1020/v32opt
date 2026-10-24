#include "v32opt.h"

// ===================================================================
// PARTIAL LOOP UNROLLING  (-floop-unroll-partial)
//
// A counted loop whose trip count is not known at compile time still pays,
// on every iteration, for its test and its back edge. This pass runs K
// iterations per round of a new main loop, behind ONE guard that checks
// that all K will pass the test, and keeps the original loop to finish
// the remaining 0..K-1 iterations (a "remainder loop"):
//
//   start:  <test>                      main:   <guard: Ri + (K-1)*step passes?>
//           JF  Rc, end                         JF  Rc, start
//           <body>                              <body>  IADD Ri, step     (K times)
//           IADD Ri, step         ->            ...
//           JMP start                           JMP main
//   end:                                start:  <test>        the original loop,
//                                               JF Rc, end    now the remainder
//                                               <body> ...
//                                       end:
//
// For a 4-way unroll that removes 3 of every 4 tests and back edges.
//
// THE GUARD. For "Ri < X" with a positive step S, K iterations all pass
// exactly when Ri + (K-1)*S < X, i.e. Ri < X - (K-1)*S; the other
// comparisons are the mirror images. It is built so that only Rc, the
// register the original test already uses, is needed:
//     bound X an immediate:    MOV Rc, Ri / I<cmp> Rc, X-(K-1)*S
//     bound in a register or   MOV Rc, <bound> / ISUB Rc, (K-1)*S /
//     a frame slot:                I<mirror cmp> Rc, Ri
// (an immediate bound is shifted at compile time, so it cannot overflow;
// a runtime bound within (K-1)*|S| of the 32-bit limit would wrap -- no
// real loop bound is).
//
// SOUNDNESS
//   * The comparison must agree with the step's direction (ILT/ILE with a
//     positive step, IGT/IGE with a negative one; INE is left alone): then
//     "the last of the K iterations passes" implies all earlier ones do.
//   * The bound must not change inside the body: an immediate; a register
//     the body never writes; or a [BP+N] frame slot that no body
//     instruction writes and whose function never lets its frame address
//     escape (promote_frame_unaliased). No CALL in the body.
//   * The body must not write the counter except by its step.
//   * The test's scratch registers (Rc, and the one the bound is loaded
//     into) are rewritten by the guard in a different way, so they must be
//     dead on entry to the body, at the loop's end label and at every jump
//     out of the loop.
//   * Labels of the body are unique and referenced only from inside it; the
//     copies get fresh names and the jumps inside each copy follow
//     (break/return jumps keep their targets).
//   * At most LOOP_PARTIAL_MAX_BODY instructions per copy; K is 4, or 2 for
//     larger bodies.
//
// Runs after loop-unroll (small constant loops are gone) and loop-index,
// before loop-invert, which then also inverts the new main loop.
// ===================================================================


typedef struct {
    CountedLoop L;
    int         k;
    LoopRename *ren;
    int         nren;
    // guard
    bool        imm_bound;
    long        guard_imm;          // X - (K-1)*S
    char        bound_src[128];     // register or [BP+N] for the runtime form
    long        delta;              // (K-1)*S
    OpType      mirror;
} PartialPlan;

static OpType mirror_cmp(OpType t)
{
    switch (t) {
        case OP_ILT: return OP_IGT;
        case OP_ILE: return OP_IGE;
        case OP_IGT: return OP_ILT;
        case OP_IGE: return OP_ILE;
        default:     return t;
    }
}

static const char *cmp_name(OpType t)
{
    switch (t) {
        case OP_ILT: return "ILT";
        case OP_ILE: return "ILE";
        case OP_IGT: return "IGT";
        case OP_IGE: return "IGE";
        default:     return "INE";
    }
}

static bool sane(long v) { return v > -(1L << 30) && v < (1L << 30); }

static int g_serial;

static void apply_plan(PartialPlan *pl)
{
    CountedLoop *L = &pl->L;
    AsmNode *at = L->start;                     // everything goes in front of the start label
    char name[160], raw[260], tmp[64];

    // main label
    bool dot = false;
    {
        char sn[128];
        get_label_name(L->start, sn, sizeof(sn));
        dot = sn[0] == '.';
    }
    snprintf(name, sizeof(name), "%sv32opt_unroll_%d", dot ? "." : "__", ++g_serial);
    snprintf(raw, sizeof(raw), "%s:", name);
    loop_link_before(at, create_node(raw, OP_LABEL, NULL, NULL, NULL));

    // the guard
    char start_name[128];
    get_label_name(L->start, start_name, sizeof(start_name));
    if (pl->imm_bound) {
        snprintf(raw, sizeof(raw), "    MOV %s, %s", L->rc, L->ri);
        loop_link_before(at, create_node(raw, OP_MOV, "MOV", L->rc, L->ri));
        snprintf(tmp, sizeof(tmp), "%ld", pl->guard_imm);
        snprintf(raw, sizeof(raw), "    %s %s, %s", cmp_name(L->cmp_type), L->rc, tmp);
        loop_link_before(at, create_node(raw, L->cmp_type, cmp_name(L->cmp_type), L->rc, tmp));
    } else {
        snprintf(raw, sizeof(raw), "    MOV %s, %s", L->rc, pl->bound_src);
        loop_link_before(at, create_node(raw, OP_MOV, "MOV", L->rc, pl->bound_src));
        long d = pl->delta;
        snprintf(tmp, sizeof(tmp), "%ld", d < 0 ? -d : d);
        snprintf(raw, sizeof(raw), "    %s %s, %s", d < 0 ? "IADD" : "ISUB", L->rc, tmp);
        loop_link_before(at, create_node(raw, d < 0 ? OP_IADD : OP_ISUB, d < 0 ? "IADD" : "ISUB", L->rc, tmp));
        snprintf(raw, sizeof(raw), "    %s %s, %s", cmp_name(pl->mirror), L->rc, L->ri);
        loop_link_before(at, create_node(raw, pl->mirror, cmp_name(pl->mirror), L->rc, L->ri));
    }
    snprintf(raw, sizeof(raw), "    JF %s, %s", L->rc, start_name);
    loop_link_before(at, create_node(raw, OP_JF, "JF", L->rc, start_name));

    // K copies of the body and the step
    AsmNode *last = L->jmp->prev;
    for (int k = 0; k < pl->k; k++) {
        ++g_serial;
        for (int i = 0; i < pl->nren; i++) {
            snprintf(pl->ren[i].to, sizeof(pl->ren[0].to), "%s_p%d", pl->ren[i].from, g_serial);
        }
        for (AsmNode *n = L->first; ; n = n->next) {
            if (!loop_is_comment(n)) loop_link_before(at, loop_clone_renamed(n, pl->ren, pl->nren));
            if (n == last) break;
        }
    }
    snprintf(raw, sizeof(raw), "    JMP %s", name);
    loop_link_before(at, create_node(raw, OP_JMP, "JMP", name, NULL));
    free(pl->ren);
}

int peephole_loop_unroll_partial(AsmNode *head)
{
    if (!head) return 0;
    loop_labels_build(head);
    liveness_begin(head);
    int optimizations = 0, nplans = 0, cap = 0;
    PartialPlan *plans = NULL;
    for (AsmNode *n = head; n; n = n->next) n->mark = 0;

    for (AsmNode *jmp = head->next; jmp; jmp = jmp->next) {
        CountedLoop L;
        if (jmp->type != OP_JMP || !loop_parse_counted(jmp, &L)) continue;
        const char *ri = L.ri, *rc = L.rc;

        // the comparison must agree with the step's direction
        bool up = L.cmp_type == OP_ILT || L.cmp_type == OP_ILE;
        bool down = L.cmp_type == OP_IGT || L.cmp_type == OP_IGE;
        if (!((up && L.stepv > 0) || (down && L.stepv < 0))) continue;

        bool overlap = false;
        for (AsmNode *n = L.start; n; n = n->next) {
            if (n->mark) { overlap = true; break; }
            if (n == jmp) break;
        }
        if (overlap) continue;

        // --- the bound -----------------------------------------------------
        // Forms: (A) cmp Rc, imm            (no loads)
        //        (B) mov Rb, <src> / cmp Rc, Rb
        //        (C) cmp Rc, Rx            (no loads)
        const char *rb = NULL;               // register the test loads the bound into
        char bsrc[128] = "";
        bool imm = false;
        long bval = 0;
        AsmNode *mid = loop_next_code(L.test_first);
        if (mid == L.cmp) {
            if (loop_imm_value(&L.cmp->src_op, &bval)) imm = true;
            else if (L.cmp->src_op.mode == MODE_REG) safe_str_copy(bsrc, L.cmp->src_op.reg, sizeof(bsrc));
            else continue;
        } else if (loop_next_code(mid) == L.cmp && mid->type == OP_MOV && mid->has_dst &&
                   mid->dst_op.mode == MODE_REG && L.cmp->src_op.mode == MODE_REG &&
                   str_case_eq(mid->dst_op.reg, L.cmp->src_op.reg) && mid->has_src) {
            rb = mid->dst_op.reg;
            if (loop_imm_value(&mid->src_op, &bval)) imm = true;
            else if (mid->src_op.mode == MODE_REG || mid->src_op.mode == MODE_INDIRECT)
                safe_str_copy(bsrc, mid->src_op.raw, sizeof(bsrc));
            else continue;
        } else {
            continue;
        }
        if (!imm && bsrc[0] == '\0') continue;
        // the runtime bound: a register (not Ri/Rc) or a plain [BP+N] / [BP-N] slot
        bool bound_is_reg = false, bound_is_slot = false;
        if (!imm) {
            if (is_register_operand(bsrc)) {
                bound_is_reg = true;
                if (str_case_eq(bsrc, ri) || str_case_eq(bsrc, rc) || (rb && str_case_eq(bsrc, rb))) continue;
                int idx = get_reg_index(bsrc);
                if (idx < 1 || idx > 10) continue;
            } else {
                // a plain frame slot: "[BP+N]" or "[BP-N]"
                bool slot = strncasecmp(bsrc, "[BP", 3) == 0 && (bsrc[3] == '+' || bsrc[3] == '-');
                for (const char *c = bsrc + 4; slot && *c && *c != ']'; c++) slot = isdigit((unsigned char)*c) != 0;
                if (!slot || !promote_frame_unaliased(L.start)) continue;
                bound_is_slot = true;
            }
        }

        // --- the body ------------------------------------------------------
        int body_ins = 0, nlabels = 0;
        bool ok = true;
        for (AsmNode *n = L.first; ok && n != jmp; n = n->next) {
            if (n->type == OP_LABEL) { nlabels++; continue; }
            if (n->type == OP_OTHER) { if (!loop_is_comment(n)) ok = false; continue; }
            if (n->type == OP_CALL) { ok = false; break; }
            if (n != L.step && modifies_register(n, ri)) { ok = false; break; }
            if (bound_is_reg && modifies_register(n, bsrc)) { ok = false; break; }
            if (bound_is_slot && n->has_dst && n->dst_op.mode == MODE_INDIRECT &&
                strcmp(n->dst_op.raw, bsrc) == 0) { ok = false; break; }
            if (n->type == OP_JMP || n->type == OP_JT || n->type == OP_JF) {
                char b[128];
                if (!loop_jump_target(n, b, sizeof(b))) { ok = false; break; }
            }
            body_ins++;
        }
        if (!ok || body_ins < 1) continue;
        int per_copy = body_ins;            // the step is among them
        if (per_copy > g_loop_tune.partial_max_body || per_copy < g_loop_tune.partial_min_body) continue;
        int k = g_loop_tune.partial_factor ? g_loop_tune.partial_factor
                                           : (per_copy <= g_loop_tune.partial_big_body ? 4 : 2);
        if (k < 2) continue;

        long delta = (long)(k - 1) * L.stepv;
        long gimm = bval - delta;
        if (imm && (!sane(bval) || !sane(gimm))) continue;
        if (!imm && !sane(delta)) continue;

        // --- the test's scratch registers must be dead around the loop -------
        const char *scratch[2] = { rc, rb };
        for (int i = 0; ok && i < 2; i++) {
            if (!scratch[i]) continue;
            if (!reg_dead_from(L.first, scratch[i])) ok = false;
            else if (!reg_dead_at_label(L.end_name, scratch[i])) ok = false;
        }
        // jumps out of the body: the scratch registers dead there too
        int nl = 0;
        for (AsmNode *n = L.first; n != jmp; n = n->next) if (n->type == OP_LABEL) nl++;
        char (*inside)[128] = malloc((size_t)(nl + 1) * sizeof(*inside));
        nl = 0;
        for (AsmNode *n = L.first; n != jmp; n = n->next)
            if (n->type == OP_LABEL) get_label_name(n, inside[nl++], sizeof(inside[0]));
        for (AsmNode *n = L.first; ok && n != jmp; n = n->next) {
            if (n->type != OP_JMP && n->type != OP_JT && n->type != OP_JF) continue;
            char b[128];
            const char *t = loop_jump_target(n, b, sizeof(b));
            bool in = false;
            for (int i = 0; i < nl && !in; i++) in = strcmp(inside[i], t) == 0;
            if (in || strcmp(t, L.end_name) == 0) continue;
            for (int i = 0; ok && i < 2; i++)
                if (scratch[i] && !reg_dead_at_label(t, scratch[i])) ok = false;
        }
        free(inside);
        if (!ok) continue;

        // --- body labels: unique, referenced only from inside the body ---------
        LoopRename *ren = calloc((size_t)nlabels + 2, sizeof(LoopRename));
        int nren = 0;
        LoopLabel *endl = loop_label_find(L.end_name);
        for (AsmNode *n = L.first; ok && n != jmp; n = n->next) {
            if (n->type != OP_LABEL) continue;
            if (endl && n == endl->node) { ok = false; break; }
            get_label_name(n, ren[nren].from, sizeof(ren[0].from));
            LoopLabel *l = loop_label_find(ren[nren].from);
            if (!l || l->duplicate) { ok = false; break; }
            nren++;
        }
        if (ok && nren > 0) {
            int *count = calloc((size_t)g_loop_labels_n + 1, sizeof(int));
            for (AsmNode *n = L.first; ok && n != jmp; n = n->next) {
                if (n->type == OP_LABEL || n->type == OP_OTHER) continue;
                char b[128];
                const char *t = loop_jump_target(n, b, sizeof(b));
                for (int i = 0; i < nren; i++) {
                    LoopLabel *l = loop_label_find(ren[i].from);
                    const char *s = n->raw;
                    size_t len = strlen(ren[i].from);
                    int occ = 0;
                    while ((s = strstr(s, ren[i].from)) != NULL) {
                        bool lb = (s == n->raw) || !loop_ident_char((unsigned char)s[-1]);
                        bool rb2 = !loop_ident_char((unsigned char)s[len]);
                        if (lb && rb2) occ++;
                        s += len;
                    }
                    if (occ && !(t && strcmp(t, ren[i].from) == 0 && occ == 1)) ok = false;
                    count[(int)(l - g_loop_labels)] += occ;
                }
            }
            for (int i = 0; ok && i < nren; i++) {
                LoopLabel *l = loop_label_find(ren[i].from);
                if (count[(int)(l - g_loop_labels)] != l->refs) ok = false;
            }
            free(count);
        }
        if (!ok) { free(ren); continue; }

        if (nplans == cap) {
            cap = cap ? cap * 2 : 16;
            plans = realloc(plans, (size_t)cap * sizeof(PartialPlan));
        }
        PartialPlan *pl = &plans[nplans++];
        pl->L = L;
        pl->k = k;
        pl->ren = ren;
        pl->nren = nren;
        pl->imm_bound = imm;
        pl->guard_imm = gimm;
        safe_str_copy(pl->bound_src, bsrc, sizeof(pl->bound_src));
        pl->delta = delta;
        pl->mirror = mirror_cmp(L.cmp_type);
        for (AsmNode *n = L.start; ; n = n->next) {
            n->mark = 1;
            if (n == jmp) break;
        }
    }

    for (int i = 0; i < nplans; i++) {
        apply_plan(&plans[i]);
        optimizations++;
    }
    free(plans);
    liveness_end();
    loop_labels_free();
    return optimizations;
}
