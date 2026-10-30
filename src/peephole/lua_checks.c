#include "v32opt.h"

// ============================================================================
// peephole-lua-checks (-L lua only) -- delete the type checks v32lua emits that cannot fail,
// and use what ';@loop kind=for' says about counted loops.
//
// v32lua guards every table index and every arithmetic operand with a test of the NaN-boxed
// representation ("is it a number?", "is it a table?", "is it a whole number >= 1?") whose
// failing side calls a runtime helper. Most of those tests are provable from what the code
// has just done:
//
//     MOV R0, Rx ; AND R0, NAN_VALUE ; IEQ R0, NAN_VALUE ; JT R0, slow       Rx is a number?
//     MOV R0, Rx ; AND R0, BOXED_DATA ; IEQ R0, BOXED_TABLE ; JF R0, slow    Rx is a table?
//     MOV Rs, Rx ; CFI Rs ; MOV R0, Rs ; CIF R0 ; IEQ R0, Rx ; JF R0, slow  Rx is a whole number?
//     MOV R0, Ri ; ILT R0, 1 ; JT R0, slow                                   integer Ri >= 1?
//
// How it works
//   1. loop facts (only when ';@loop kind=for ...' came with the loop and the few things that can be
//      checked cheaply hold): `limit` / `step` loads become immediates when `to=` / `stepval=` are
//      given and their initializing stores go when nothing reads them any more;
//   2. a forward dataflow over the function (facts per register and per tracked frame word, met at
//      joins with AND, so a fact holds on EVERY path): a register is a number after FADD/FSUB/FMUL/
//      FDIV/FLR/CIF/MOV #float, after a guard that fell through, or when loaded from the counter of
//      a counted loop; a whole number >= 1 when it is such a counter, or a copy of one; a CALL
//      forgets everything it can clobber (`;@fn F clobbers=` keeps the rest);
//   3. a guard whose outcome the facts decide is deleted (with the dead coerce block that the
//      JF-over-a-block shape leaves behind). Deleting needs the scratch register (R0) dead
//      afterwards, which liveness.c proves.
//
// Assumptions worth naming
//   - the result of float arithmetic on numbers is a number: an overflow to Inf would test as "boxed"
//     in the original guard (exponent all ones) and be sent to the coercion helper; here it simply
//     continues as a number;
//   - the loop facts are true (a wrong hint is a compiler bug); cheap sanity checks (counter written
//     once in the loop, limit and step never, no jump into the body, no pointer into the frame) make
//     v32opt ignore the facts of a loop that visibly breaks them.
// ============================================================================

#define F_NUM    0x01     // a Lua number (not a NaN-boxed value)
#define F_WHOLE  0x02     // integer-valued, |v| <= 2^24: CFI / CIF round-trips exactly
#define F_GE1    0x04     // >= 1
#define F_TABLE  0x08     // a boxed table
#define F_IGE1   0x10     // (integer register) >= 1

#define NREG       11     // R0..R10 are tracked
#define MAX_SLOTS   8     // frame words tracked besides the loop counters
#define MAX_LOOPS  32
#define MAX_FN_NODES 20000
#define WHOLE_MAX  16777216L

typedef struct {
    unsigned char r[NREG];
    unsigned char s[MAX_SLOTS];
} State;

typedef struct {
    int  si, bi;                 // index of the start label, of the back-edge JMP
    int  ctr, lim, stp;          // BP offsets (valid: has_*)
    bool has_lim, has_stp;
    unsigned char ctr_facts;     // what a load of the counter slot inside [si, bi] is
    const LoopFacts *lf;
} Loop;

typedef enum { G_NONE, G_NAN, G_TABLE, G_WHOLE, G_IGE1 } GuardKind;

typedef struct {
    GuardKind kind;
    int  first;                  // index of the first node of the guard
    int  rx;                     // the register under test
    int  rt;                     // the scratch register
    bool jt;                     // the branch is JT (else JF)
} Guard;

typedef struct {
    AsmNode **nodes;
    int       n;
    State    *in;
    bool     *seen;
    Guard    *guard;             // per node index: the guard that ends at that branch
    int       nslots;
    int       slot_off[MAX_SLOTS];
    Loop      loops[MAX_LOOPS];
    int       nloops;
    bool      frame_ok;          // frame words reachable only by name
} Fn;

// ---------------------------------------------------------------------------
// small helpers
// ---------------------------------------------------------------------------
static int reg_of(const Operand *op) {
    if (op->mode != MODE_REG) return -1;
    if (op->reg[0] != 'R' && op->reg[0] != 'r') return -1;
    int r = get_reg_index(op->reg);
    return (r >= 0 && r < NREG) ? r : -1;
}

static bool is_bp_slot(const Operand *op, int *off) {
    if (op->mode != MODE_INDIRECT || !str_case_eq(op->reg, "BP")) return false;
    if (off) *off = op->offset;
    return true;
}

static bool sym_is(const Operand *op, const char *sym) {
    return op->mode == MODE_IMMEDIATE && name_eq(op->raw, sym);
}

static const char *branch_label(const AsmNode *n) {
    if (n->type == OP_JT || n->type == OP_JF) return n->has_src ? n->src_op.raw : NULL;
    return n->has_dst ? n->dst_op.raw : NULL;
}

static int find_label_idx(const Fn *f, const char *name) {
    if (!name) return -1;
    char buf[128];
    safe_str_copy(buf, name, sizeof(buf));
    char *t = trim(buf);
    char lbl[128];
    for (int i = 0; i < f->n; i++) {
        if (f->nodes[i]->type != OP_LABEL) continue;
        get_label_name(f->nodes[i], lbl, sizeof(lbl));
        if (name_eq(lbl, t)) return i;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// guard recognition, ending at a JT/JF node b. All the nodes of a guard are adjacent in f->nodes
// (so no label sits between them).
// ---------------------------------------------------------------------------
static bool is_op(const AsmNode *n, OpType t) { return n->type == t; }

static bool match_guard(const Fn *f, int b, Guard *g) {
    const AsmNode *br = f->nodes[b];
    if (br->type != OP_JT && br->type != OP_JF) return false;
    if (!br->has_dst || !br->has_src || br->dst_op.mode != MODE_REG) return false;
    int rt = reg_of(&br->dst_op);
    if (rt < 0) return false;
    g->jt = br->type == OP_JT;
    g->rt = rt;

    // number / table tests: MOV Rt,Rx ; AND Rt,K1 ; IEQ Rt,K2 ; JT|JF Rt,L
    if (b >= 3) {
        const AsmNode *m = f->nodes[b - 3], *a = f->nodes[b - 2], *e = f->nodes[b - 1];
        if (is_op(m, OP_MOV) && is_op(a, OP_AND) && is_op(e, OP_IEQ) &&
            m->has_dst && m->has_src && a->has_dst && a->has_src && e->has_dst && e->has_src &&
            reg_of(&m->dst_op) == rt && reg_of(&a->dst_op) == rt && reg_of(&e->dst_op) == rt) {
            int rx = reg_of(&m->src_op);
            if (rx >= 0 && rx != rt) {
                if (sym_is(&a->src_op, "NAN_VALUE") && sym_is(&e->src_op, "NAN_VALUE")) {
                    g->kind = G_NAN; g->first = b - 3; g->rx = rx; return true;
                }
                if (sym_is(&a->src_op, "BOXED_DATA") && sym_is(&e->src_op, "BOXED_TABLE")) {
                    g->kind = G_TABLE; g->first = b - 3; g->rx = rx; return true;
                }
            }
        }
    }
    // integer >= 1: MOV Rt,Ri ; ILT Rt,1 ; JT Rt,L
    if (b >= 2 && g->jt) {
        const AsmNode *m = f->nodes[b - 2], *l = f->nodes[b - 1];
        if (is_op(m, OP_MOV) && is_op(l, OP_ILT) && m->has_dst && m->has_src && l->has_dst && l->has_src &&
            reg_of(&m->dst_op) == rt && reg_of(&l->dst_op) == rt &&
            l->src_op.mode == MODE_IMMEDIATE && !l->src_op.is_float && l->src_op.immediate == 1 &&
            strcmp(l->src_op.raw, "1") == 0) {
            int ri = reg_of(&m->src_op);
            if (ri >= 0 && ri != rt) { g->kind = G_IGE1; g->first = b - 2; g->rx = ri; return true; }
        }
    }
    // whole number: MOV Rs,Rx ; CFI Rs ; MOV Rt,Rs ; CIF Rt ; IEQ Rt,Rx ; JF Rt,L
    if (b >= 5 && !g->jt) {
        const AsmNode *m1 = f->nodes[b - 5], *c1 = f->nodes[b - 4], *m2 = f->nodes[b - 3],
                      *c2 = f->nodes[b - 2], *e = f->nodes[b - 1];
        if (is_op(m1, OP_MOV) && is_op(c1, OP_CFI) && is_op(m2, OP_MOV) && is_op(c2, OP_CIF) && is_op(e, OP_IEQ) &&
            m1->has_dst && m1->has_src && c1->has_dst && m2->has_dst && m2->has_src && c2->has_dst &&
            e->has_dst && e->has_src) {
            int rs = reg_of(&m1->dst_op), rx = reg_of(&m1->src_op);
            if (rs >= 0 && rx >= 0 && rs != rx && rs != rt && rx != rt &&
                reg_of(&c1->dst_op) == rs && reg_of(&m2->dst_op) == rt && reg_of(&m2->src_op) == rs &&
                reg_of(&c2->dst_op) == rt && reg_of(&e->dst_op) == rt && reg_of(&e->src_op) == rx) {
                g->kind = G_WHOLE; g->first = b - 5; g->rx = rx; return true;
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// transfer function
// ---------------------------------------------------------------------------
static int slot_index(const Fn *f, int off) {
    for (int i = 0; i < f->nslots; i++) if (f->slot_off[i] == off) return i;
    return -1;
}

static bool int_literal_whole(const Operand *op, long *v) {
    if (op->mode != MODE_IMMEDIATE || !op->is_float) return false;
    float x = op->float_value;
    if (x < 1.0f || x > (float)WHOLE_MAX) return false;
    if ((float)(long)x != x) return false;
    *v = (long)x;
    return true;
}

static bool is_float_arith(OpType t) {
    switch (t) {
        case OP_FADD: case OP_FSUB: case OP_FMUL: case OP_FDIV: case OP_FMOD:
        case OP_FLR: case OP_CEIL: case OP_ROUND: case OP_FABS: case OP_FMIN: case OP_FMAX:
        case OP_CIF:
            return true;
        default:
            return false;
    }
}

// st: in; after the call st is the state after the instruction (fall-through edge).
static void transfer(const Fn *f, int idx, State *st) {
    const AsmNode *n = f->nodes[idx];
    if (n->type == OP_LABEL) return;

    if (n->type == OP_CALL) {
        const FnHints *h = hints_fn_of_call(n);
        for (int r = 0; r < NREG; r++) {
            if (h && h->clobbers_known && !((h->clobbers >> r) & 1u)) continue;
            st->r[r] = 0;
        }
        return;                                    // frame words are out of the callee's reach (frame_ok)
    }

    // frame words written by name
    int off;
    if (n->has_dst && is_bp_slot(&n->dst_op, &off)) {
        int si = slot_index(f, off);
        if (si >= 0) {
            int rs = (n->type == OP_MOV && n->has_src) ? reg_of(&n->src_op) : -1;
            st->s[si] = rs >= 0 ? st->r[rs] : 0;
        }
    }

    // register written?
    if (!n->has_dst || n->dst_op.mode != MODE_REG) return;
    switch (n->type) {
        case OP_JMP: case OP_JT: case OP_JF: case OP_PUSH: case OP_CALL: return;
        default: break;
    }
    int d = reg_of(&n->dst_op);
    if (d < 0) return;

    unsigned char nf = 0;
    if (n->type == OP_MOV && n->has_src) {
        const Operand *s = &n->src_op;
        int rs = reg_of(s);
        long iv;
        if (rs >= 0) {
            nf = st->r[rs];
        } else if (is_bp_slot(s, &off)) {
            nf = 0;
            // the counter of a counted loop, read inside that loop
            for (int i = 0; i < f->nloops; i++) {
                const Loop *l = &f->loops[i];
                if (l->ctr == off && idx >= l->si && idx <= l->bi) { nf = l->ctr_facts; break; }
            }
            if (!nf) {
                int si = slot_index(f, off);
                if (si >= 0) nf = st->s[si];
            }
        } else if (s->mode == MODE_IMMEDIATE && s->is_float) {
            nf = F_NUM;
            if (int_literal_whole(s, &iv)) nf |= F_WHOLE | F_GE1;
        }
    } else if (is_float_arith(n->type)) {
        nf = F_NUM;
    } else if (n->type == OP_CFI) {
        // an integer now; >= 1 survives the conversion when the float was a whole number >= 1
        unsigned char old = st->r[d];
        nf = ((old & F_WHOLE) && (old & F_GE1)) ? F_IGE1 : 0;
    }
    st->r[d] = nf;
}

// facts the guard at branch b adds on its two edges
static void refine(const Guard *g, State *taken, State *fall) {
    switch (g->kind) {
        case G_NAN:
            (g->jt ? fall : taken)->r[g->rx] |= F_NUM;           // JT: boxed -> slow; JF: number -> skip the coerce
            break;
        case G_TABLE:
            (g->jt ? taken : fall)->r[g->rx] |= F_TABLE;
            break;
        case G_WHOLE:
            fall->r[g->rx] |= F_NUM | F_WHOLE;
            break;
        case G_IGE1:
            fall->r[g->rx] |= F_IGE1;
            break;
        default: break;
    }
}

static void meet(State *dst, const State *src) {
    for (int i = 0; i < NREG; i++) dst->r[i] &= src->r[i];
    for (int i = 0; i < MAX_SLOTS; i++) dst->s[i] &= src->s[i];
}

static void state_top(State *s) {
    memset(s->r, 0xFF, sizeof(s->r));
    memset(s->s, 0xFF, sizeof(s->s));
}

// ---------------------------------------------------------------------------
// dataflow over one function. Returns false if it did not settle (then nothing is deleted).
// ---------------------------------------------------------------------------
static bool run_dataflow(Fn *f) {
    int n = f->n;
    for (int i = 0; i < n; i++) { state_top(&f->in[i]); f->seen[i] = false; f->guard[i].kind = G_NONE; }

    bool computed = false;                       // a jump through a register may reach any label
    for (int i = 0; i < n; i++) {
        const AsmNode *x = f->nodes[i];
        if (x->type == OP_JMP && x->has_dst && x->dst_op.mode != MODE_IMMEDIATE) computed = true;
        if ((x->type == OP_JT || x->type == OP_JF) && x->has_src && x->src_op.mode != MODE_IMMEDIATE) computed = true;
        Guard g;
        memset(&g, 0, sizeof(g));
        if ((x->type == OP_JT || x->type == OP_JF) && !match_guard(f, i, &g)) g.kind = G_NONE;
        f->guard[i] = g;
    }

    // predecessors of labels: jumps to them, fall-through into them, their address taken
    bool *has_pred = calloc((size_t)n, sizeof(bool));
    bool *force0   = calloc((size_t)n, sizeof(bool));
    int  *target   = malloc((size_t)n * sizeof(int));
    if (!has_pred || !force0 || !target) { free(has_pred); free(force0); free(target); return false; }
    for (int i = 0; i < n; i++) {
        target[i] = -1;
        const AsmNode *x = f->nodes[i];
        if (x->type == OP_JMP || x->type == OP_JT || x->type == OP_JF) {
            int t = find_label_idx(f, branch_label(x));
            target[i] = t;
            if (t >= 0) has_pred[t] = true;
        } else if (x->type != OP_LABEL && x->has_src && x->src_op.mode == MODE_IMMEDIATE &&
                   x->type != OP_CALL) {
            int t = find_label_idx(f, x->src_op.raw);          // "MOV R0, label": the address escapes
            if (t >= 0) force0[t] = true;
        }
    }
    for (int i = 0; i < n; i++) {
        if (f->nodes[i]->type != OP_LABEL) continue;
        bool falls = i > 0 && f->nodes[i - 1]->type != OP_JMP && f->nodes[i - 1]->type != OP_RET &&
                     f->nodes[i - 1]->type != OP_HLT;
        if (computed || force0[i] || (!has_pred[i] && !falls)) {
            memset(&f->in[i], 0, sizeof(State));               // unknown entry: nothing is known
            f->seen[i] = true;
        } else if (i > 0 && falls) {
            // fall-through from the node before is handled by the edge below
        }
    }
    memset(&f->in[0], 0, sizeof(State));
    f->seen[0] = true;

    bool changed = true;
    for (int round = 0; changed && round < 40; round++) {
        changed = false;
        for (int i = 0; i < n; i++) {
            if (!f->seen[i]) continue;
            const AsmNode *x = f->nodes[i];
            State fall = f->in[i], taken;
            transfer(f, i, &fall);
            taken = fall;
            if ((x->type == OP_JT || x->type == OP_JF) && f->guard[i].kind != G_NONE)
                refine(&f->guard[i], &taken, &fall);

            // edges
            int succ[2] = {-1, -1};
            const State *sst[2] = {&fall, &taken};
            bool ends = x->type == OP_JMP || x->type == OP_RET || x->type == OP_HLT;
            if (!ends && i + 1 < n) succ[0] = i + 1;
            if ((x->type == OP_JMP || x->type == OP_JT || x->type == OP_JF) && target[i] >= 0) succ[1] = target[i];
            for (int k = 0; k < 2; k++) {
                int t = succ[k];
                if (t < 0) continue;
                if (!f->seen[t]) { f->in[t] = *sst[k]; f->seen[t] = true; changed = true; }
                else {
                    State m = f->in[t];
                    meet(&m, sst[k]);
                    if (memcmp(&m, &f->in[t], sizeof(State)) != 0) { f->in[t] = m; changed = true; }
                }
            }
        }
    }
    free(has_pred); free(force0); free(target);
    return !changed;
}

// ---------------------------------------------------------------------------
// loop facts: validate, and use
// ---------------------------------------------------------------------------
static int count_writes(const Fn *f, int from, int to, int off, bool mov_only_ok, int *first_idx) {
    int c = 0;
    for (int i = from; i <= to && i < f->n; i++) {
        const AsmNode *x = f->nodes[i];
        int o;
        if (x->has_dst && is_bp_slot(&x->dst_op, &o) && o == off) {
            // PUSH/JMP/CALL read their operand; everything else with an indirect destination writes it
            if (x->type == OP_PUSH || x->type == OP_JMP || x->type == OP_CALL || x->type == OP_JT || x->type == OP_JF) continue;
            if (mov_only_ok && x->type != OP_MOV) c += 100;
            if (c == 0 || first_idx) { if (first_idx && c == 0) *first_idx = i; }
            c++;
        }
    }
    return c;
}

static bool add_loop(Fn *f, int si) {
    if (f->nloops >= MAX_LOOPS) return false;
    const LoopFacts *lf = hints_loop_facts(f->nodes[si]);
    if (!lf || !lf->has_counter) return false;

    // the back edge: the first jump back to the start label
    char lbl[128];
    get_label_name(f->nodes[si], lbl, sizeof(lbl));
    int bi = -1;
    for (int i = si + 1; i < f->n; i++) {
        const AsmNode *x = f->nodes[i];
        if (x->type == OP_JMP && x->has_dst && name_eq(x->dst_op.raw, lbl)) { bi = i; break; }
    }
    if (bi < 0) return false;

    // nothing outside the loop may jump into its body (the start label itself is the entry)
    for (int i = 0; i < f->n; i++) {
        if (i >= si && i <= bi) continue;
        const AsmNode *x = f->nodes[i];
        if (x->type != OP_JMP && x->type != OP_JT && x->type != OP_JF) continue;
        int t = find_label_idx(f, branch_label(x));
        if (t > si && t <= bi) return false;
        if (t == si) return false;                      // a jump to the start from outside would skip its init
    }
    // the counter is written once in the loop (its increment), limit and step never
    int w = count_writes(f, si, bi, lf->counter, true, NULL);
    if (w != 1) return false;
    if (lf->has_limit && count_writes(f, si, bi, lf->limit, false, NULL) != 0) return false;
    if (lf->has_step  && count_writes(f, si, bi, lf->step,  false, NULL) != 0) return false;

    Loop *l = &f->loops[f->nloops++];
    memset(l, 0, sizeof(*l));
    l->si = si; l->bi = bi; l->lf = lf;
    l->ctr = lf->counter;
    l->has_lim = lf->has_limit; l->lim = lf->limit;
    l->has_stp = lf->has_step;  l->stp = lf->step;
    l->ctr_facts = F_NUM;
    if (lf->has_from && lf->has_stepval && lf->from >= 1 && lf->stepval >= 1) {
        l->ctr_facts |= F_GE1;
        if (lf->has_to && lf->to >= lf->from && lf->to + lf->stepval <= WHOLE_MAX) l->ctr_facts |= F_WHOLE;
    }
    return true;
}

// Does anything but the stores in `skip` read the frame word `off` of this function?
static bool slot_read_anywhere(const Fn *f, int off) {
    for (int i = 0; i < f->n; i++) {
        const AsmNode *x = f->nodes[i];
        if (!x) continue;
        int o;
        if (x->has_src && is_bp_slot(&x->src_op, &o) && o == off) return true;
        if (x->has_dst && is_bp_slot(&x->dst_op, &o) && o == off && x->type != OP_MOV) return true;
    }
    return false;
}

static bool set_imm_load(AsmNode *n, long v) {
    if (!trigger_allowed()) return false;
    insert_debug_comment(n->prev, OPT_PEEPHOLE_LUA_CHECKS, n->raw);
    node_set_rawf(n, "    MOV %s, %ld.0", n->dst_op.raw, v);
    char imm[48];
    snprintf(imm, sizeof(imm), "%ld.0", v);
    n->src_op = parse_operand(imm);
    return true;
}

// `step` / `limit` loads of a counted loop become immediates; their initializing stores go when unread.
static int fold_loop_constants(Fn *f) {
    int done = 0;
    for (int li = 0; li < f->nloops; li++) {
        Loop *l = &f->loops[li];
        const LoopFacts *lf = l->lf;
        for (int i = l->si; i <= l->bi; i++) {
            AsmNode *x = f->nodes[i];
            int o;
            if (x->type != OP_MOV || !x->has_src || !x->has_dst || x->dst_op.mode != MODE_REG) continue;
            if (!is_bp_slot(&x->src_op, &o)) continue;
            if (l->has_stp && lf->has_stepval && o == l->stp) { if (set_imm_load(x, lf->stepval)) done++; }
            else if (l->has_lim && lf->has_to && o == l->lim) { if (set_imm_load(x, lf->to)) done++; }
        }
    }
    return done;
}

static int drop_dead_inits(Fn *f) {
    int done = 0;
    for (int li = 0; li < f->nloops; li++) {
        const Loop *l = &f->loops[li];
        const LoopFacts *lf = l->lf;
        int offs[2] = { (l->has_stp && lf->has_stepval) ? l->stp : 1000, (l->has_lim && lf->has_to) ? l->lim : 1000 };
        for (int k = 0; k < 2; k++) {
            int off = offs[k];
            if (off == 1000 || slot_read_anywhere(f, off)) continue;
            for (int i = 0; i < f->n; i++) {
                AsmNode *x = f->nodes[i];
                int o;
                if (!x) continue;
                if (x->type != OP_MOV || !x->has_dst || !is_bp_slot(&x->dst_op, &o) || o != off) continue;
                if (!x->has_src || x->src_op.mode != MODE_REG) continue;
                AsmNode *nodes[1] = { x };
                AsmNode *cur = x;
                if (remove_with_debug(&cur, nodes, 1, OPT_PEEPHOLE_LUA_CHECKS)) { done++; f->nodes[i] = NULL; }
            }
        }
    }
    return done;
}

// ---------------------------------------------------------------------------
// the deletions
// ---------------------------------------------------------------------------
static bool label_referenced(const Fn *f, int lidx) {
    char lbl[128];
    get_label_name(f->nodes[lidx], lbl, sizeof(lbl));
    for (int i = 0; i < f->n; i++) {
        const AsmNode *x = f->nodes[i];
        if (!x || x->type == OP_LABEL) continue;
        if (x->has_dst && x->dst_op.mode == MODE_IMMEDIATE && name_eq(x->dst_op.raw, lbl)) return true;
        if (x->has_src && x->src_op.mode == MODE_IMMEDIATE && name_eq(x->src_op.raw, lbl)) return true;
    }
    return false;
}

static int apply_guards(Fn *f) {
    int total = 0;
    int cap = f->n;
    // phase 1: choose
    typedef struct { int from, to; int dead_label; } Del;     // delete nodes [from, to]; for the JF shape also up to the label
    Del *dels = malloc((size_t)(cap ? cap : 1) * sizeof(Del));
    if (!dels) return 0;
    int nd = 0;
    for (int i = 0; i < f->n; i++) f->nodes[i] ? (void)(f->nodes[i]->mark = 0) : (void)0;

    for (int b = 0; b < f->n; b++) {
        const Guard *g = &f->guard[b];
        if (g->kind == G_NONE || !f->seen[g->first]) continue;
        const State *st = &f->in[g->first];
        unsigned char fx = st->r[g->rx];
        int del_from = g->first, del_to = b, dead_label = -1;
        bool prove = false;
        switch (g->kind) {
            case G_NAN:
                if (fx & F_NUM) {
                    if (g->jt) prove = true;                                    // never boxed: JT never taken
                    else {                                                      // JF over a coerce block: always taken
                        int lt = find_label_idx(f, branch_label(f->nodes[b]));
                        if (lt > b && lt - b <= 64) {
                            bool simple = true;
                            for (int k = b + 1; k < lt && simple; k++) {
                                const AsmNode *y = f->nodes[k];
                                if (y->type == OP_JMP || y->type == OP_JT || y->type == OP_JF ||
                                    y->type == OP_RET || y->type == OP_HLT) simple = false;
                                if (y->type == OP_LABEL && label_referenced(f, k)) simple = false;
                            }
                            // the target must not be reached by anything that needs the block's effects
                            if (simple) { prove = true; dead_label = lt; del_to = lt - 1; }
                        }
                    }
                }
                break;
            case G_TABLE:
                if ((fx & F_TABLE) && !g->jt) prove = true;
                break;
            case G_WHOLE:
                if ((fx & F_WHOLE) && (fx & F_NUM)) { prove = true; del_from = g->first + 2; }
                break;
            case G_IGE1:
                if (fx & F_IGE1) prove = true;
                break;
            default: break;
        }
        if (!prove) continue;

        // the scratch register must be dead afterwards (on the fall-through side, or at the join label)
        char rname[8];
        snprintf(rname, sizeof(rname), "R%d", g->rt);
        AsmNode *after = (dead_label >= 0) ? f->nodes[dead_label] : f->nodes[b]->next;
        if (!after || !reg_dead_from(after, rname)) continue;
        // and the JF-shape block deletes the writes of other registers too: they must not be read after the
        // join, which the block's own semantics would make unnecessary only if the branch was always taken --
        // it is, so nothing it would have written is ever observed (the block never runs).

        // do not overlap an earlier choice
        bool clash = false;
        for (int k = del_from; k <= del_to && !clash; k++) if (f->nodes[k]->mark) clash = true;
        if (clash) continue;
        for (int k = del_from; k <= del_to; k++) f->nodes[k]->mark = 1;
        dels[nd].from = del_from; dels[nd].to = del_to; dels[nd].dead_label = dead_label;
        nd++;
    }

    // phase 2: delete (instructions only: labels stay, liveness.c indexes them)
    for (int d = 0; d < nd; d++) {
        AsmNode *list[PEEPHOLE_MAX_SCAN_DISTANCE];
        int cnt = 0;
        bool too_big = false;
        for (int k = dels[d].from; k <= dels[d].to; k++) {
            if (f->nodes[k]->type == OP_LABEL) continue;
            if (cnt >= PEEPHOLE_MAX_SCAN_DISTANCE) { too_big = true; break; }
            list[cnt++] = f->nodes[k];
        }
        if (too_big || cnt == 0) continue;
        // all-or-nothing: trigger_allowed() is consulted once for the whole group by remove_with_debug
        AsmNode *cur = list[0];
        bool volatile_hit = false;
        for (int k = 0; k < cnt; k++) if (node_is_volatile(list[k])) volatile_hit = true;
        if (volatile_hit) continue;
        // remove_with_debug removes them one by one from a snapshot; nodes are not adjacent in the list when
        // labels/comments sit between, which is fine for it
        if (remove_with_debug(&cur, list, cnt, OPT_PEEPHOLE_LUA_CHECKS)) {
            total += cnt;
        }
    }
    free(dels);
    return total;
}


// Spill removal around calls (3.4) is deliberately NOT done here: v32lua pushes call arguments and removes them with
// "IADD SP,k ; POP Rn", so "PUSH Rn ... CALL f ... POP Rn" is often an argument, not a spill (an earlier version of this
// pass broke __mv_to_table's call that way). Telling them apart needs the callee's arity.

// ---------------------------------------------------------------------------
// per function
// ---------------------------------------------------------------------------
static int process_function(AsmNode *entry) {
    AsmNode *last = promote_function_last(entry);
    Fn f;
    memset(&f, 0, sizeof(f));

    for (int pass = 0; pass < 2; pass++) {          // 0: count, 1: fill
        int n = 0;
        for (AsmNode *x = entry->next; x; x = x->next) {
            if (x->type != OP_OTHER) {
                if (pass == 1) f.nodes[n] = x;
                n++;
            }
            if (x == last) break;
        }
        if (pass == 0) {
            if (n < 8 || n > MAX_FN_NODES) return 0;
            f.nodes = malloc((size_t)n * sizeof(AsmNode *));
            if (!f.nodes) return 0;
            f.n = n;
        }
    }
    f.in    = malloc((size_t)f.n * sizeof(State));
    f.seen  = malloc((size_t)f.n * sizeof(bool));
    f.guard = malloc((size_t)f.n * sizeof(Guard));
    int total = 0;
    if (!f.in || !f.seen || !f.guard) goto out;

    f.frame_ok = promote_frame_unaliased(entry->next);

    // loops with facts
    if (f.frame_ok) {
        for (int i = 0; i < f.n; i++)
            if (f.nodes[i]->type == OP_LABEL && (f.nodes[i]->hints & NH_LOOP_FACTS)) add_loop(&f, i);
    }

    // 1. constants
    if (f.nloops > 0) {
        total += fold_loop_constants(&f);
        total += drop_dead_inits(&f);
        if (total) {
            // some nodes were deleted: rebuild the array (labels are still all there)
            int n = 0;
            for (AsmNode *x = entry->next; x; x = x->next) {
                if (x->type != OP_OTHER) {
                    // map the loops' indices by recomputing them below
                    f.nodes[n++] = x;
                }
                if (x == last) break;
            }
            f.n = n;
            f.nloops = 0;
            for (int i = 0; i < f.n; i++)
                if (f.nodes[i]->type == OP_LABEL && (f.nodes[i]->hints & NH_LOOP_FACTS)) add_loop(&f, i);
        }
    }

    // 2. frame words worth tracking: the ones stored from a register inside a counted loop (the loop variable)
    for (int li = 0; li < f.nloops; li++) {
        const Loop *l = &f.loops[li];
        for (int i = l->si; i <= l->bi; i++) {
            const AsmNode *x = f.nodes[i];
            int o;
            if (x->type == OP_MOV && x->has_dst && is_bp_slot(&x->dst_op, &o) && x->has_src && x->src_op.mode == MODE_REG &&
                o != l->ctr && slot_index(&f, o) < 0 && f.nslots < MAX_SLOTS)
                f.slot_off[f.nslots++] = o;
        }
    }

    // 3. facts, and the guards they decide
    if (run_dataflow(&f)) total += apply_guards(&f);

out:
    free(f.nodes); free(f.in); free(f.seen); free(f.guard);
    return total;
}

int peephole_lua_checks(AsmNode *head)
{
    if (!is_lua_mode() || !head) return 0;
    int total = 0;
    liveness_begin(head);
    for (AsmNode *n = head->next; n; n = n->next) {
        if (!promote_function_entry(n)) continue;
        total += process_function(n);
    }
    liveness_end();
    return total;
}
