#include "v32opt.h"

// ===================================================================
// FULL LOOP UNROLLING  (-floop-unroll)
//
// A counted loop whose trip count is a compile-time constant is replaced
// by that many copies of its body, with no test and no back edge:
//
//       MOV  Ri, 0                          MOV  Ri, 0
//   start:                                  ...body...
//       MOV  R0, Ri                         IADD Ri, 1
//       ILT  R0, 4                          ...body...
//       JF   R0, end                  ->    IADD Ri, 1
//       ...body...                          ...  (4 copies)
//       IADD Ri, 1                      end:
//       JMP  start
//   end:
//
// Per iteration that removes the test (3 instructions) and the back
// edge (1). Every copy keeps its "IADD Ri, S", so Ri holds the same
// value everywhere as before -- and, with Ri a known constant in each
// copy, constant-folding can turn "global_arr + i" into an address.
//
// The loop shape matched (what the compilers and promote-* leave):
//     MOV  Ri, <imm>            the last write of Ri before the start label,
//                               no label or branch in between
//   start:                      referenced by nothing but the back edge
//     MOV  Rc, Ri               \ the test: Rc = (Ri <cmp> <imm>)
//     I<cmp> Rc, <imm>          /  cmp: LT LE GT GE NE
//     JF   Rc, end
//     <body>                    straight code, jumps and labels of its own
//     IADD/ISUB Ri, <imm>       the step
//     JMP  start
//   end:                        the code right after the JMP
//
// SOUNDNESS
//   * The trip count comes from running the test on the constant init
//     and step (32-bit wrap-around, at most LOOP_UNROLL_MAX_TRIPS).
//   * Ri is written only by the step: no instruction of the body
//     modifies it, and the body has no CALL (a callee could change it).
//   * The loop can only be entered at the start label, from the INIT
//     (no other reference to it), and the body can only be entered from
//     inside: every label in the body is referenced only by jumps in the
//     body, and appears only as a jump target. The copies get fresh
//     labels, with the jumps inside each copy renamed to match; jumps to
//     labels outside the body (break, return) are kept.
//   * On the normal exit the original leaves Rc = 0; if Rc is live at the
//     end label that value is re-created ("MOV Rc, 0").
//   * Labels must be unique (the compilers' default .local labels repeat
//     in every function; use --standard).
//
// Size limits keep the growth bounded: at most LOOP_UNROLL_MAX_TRIPS
// iterations and LOOP_UNROLL_MAX_TOTAL instructions in all.
//
// Runs once, after constant-folding (the loop's init has become a plain
// "MOV Ri, <imm>" by then) and before loop-invert.
// ===================================================================

#define LOOP_UNROLL_MAX_TRIPS  16
#define LOOP_UNROLL_MAX_TOTAL  96

typedef struct {
    char name[128];
    AsmNode *node;
    bool duplicate;
    int  refs;           // occurrences as a token in non-label lines
} UlLabel;

static UlLabel *g_ul;
static int      g_ul_n;

static int cmp_ul(const void *a, const void *b)
{
    return strcmp(((const UlLabel *)a)->name, ((const UlLabel *)b)->name);
}

static UlLabel *ul_find(const char *name)
{
    UlLabel key;
    safe_str_copy(key.name, name, sizeof(key.name));
    return bsearch(&key, g_ul, (size_t)g_ul_n, sizeof(UlLabel), cmp_ul);
}

static bool ident_char(int c) { return isalnum(c) || c == '_' || c == '.' || c == '$'; }

// Calls fn(label) for every identifier token of the line that names a label.
static void ul_tokens(const AsmNode *n, void (*fn)(UlLabel *, void *), void *arg)
{
    const char *s = n->raw;
    while (*s && *s != ';') {
        if (ident_char((unsigned char)*s)) {
            const char *b = s;
            while (ident_char((unsigned char)*s)) s++;
            size_t len = (size_t)(s - b);
            if (len < 128) {
                char tok[128];
                memcpy(tok, b, len);
                tok[len] = '\0';
                UlLabel *l = ul_find(tok);
                if (l) fn(l, arg);
            }
        } else {
            s++;
        }
    }
}

static void count_ref(UlLabel *l, void *arg) { (void)arg; l->refs++; }

static void ul_build(AsmNode *head)
{
    int n = 0;
    for (AsmNode *p = head->next; p; p = p->next) if (p->type == OP_LABEL) n++;
    g_ul   = calloc((size_t)n + 1, sizeof(UlLabel));
    g_ul_n = 0;
    for (AsmNode *p = head->next; p; p = p->next) {
        if (p->type != OP_LABEL) continue;
        get_label_name(p, g_ul[g_ul_n].name, sizeof(g_ul[0].name));
        g_ul[g_ul_n].node = p;
        g_ul_n++;
    }
    qsort(g_ul, (size_t)g_ul_n, sizeof(UlLabel), cmp_ul);
    for (int i = 1; i < g_ul_n; i++) {
        if (strcmp(g_ul[i].name, g_ul[i - 1].name) == 0) {
            g_ul[i].duplicate = g_ul[i - 1].duplicate = true;
        }
    }
    for (AsmNode *p = head->next; p; p = p->next) {
        if (p->type == OP_LABEL || p->type == OP_OTHER) continue;
        ul_tokens(p, count_ref, NULL);
    }
}

static bool is_comment_node(const AsmNode *n)
{
    return n->type == OP_OTHER && !is_directive_node(n);
}

static AsmNode *next_code(AsmNode *n)      // skips comments only
{
    for (n = n->next; n && is_comment_node(n); n = n->next) ;
    return n;
}

static const char *jump_target(const AsmNode *n, char *buf, size_t sz)
{
    const Operand *op = NULL;
    if (n->type == OP_JMP) op = n->has_dst ? &n->dst_op : &n->src_op;
    else if (n->type == OP_JT || n->type == OP_JF) op = n->has_src ? &n->src_op : NULL;
    if (!op || op->mode == MODE_REG || op->mode == MODE_INDIRECT) return NULL;
    safe_str_copy(buf, op->raw, sz);
    return trim(buf);
}

static bool imm_value(const Operand *op, long *v)
{
    if (!op || op->mode != MODE_IMMEDIATE || !is_numeric_immediate(op) || op->is_float) return false;
    *v = (long)op->immediate;
    return true;
}

// Is Ra written by a node that is none of the loop's own? (helper for the body scan)
static bool is_scratch_reg(const char *r)
{
    int i = get_reg_index(r);
    return i >= 1 && i <= 10;
}

typedef struct { int count; long init; } Trip;

// Number of times 'cmp' holds while i steps from init by step (32-bit), or -1.
static int trip_count(OpType cmp, long init, long step, long bound)
{
    int i = (int)init, b = (int)bound;
    unsigned st = (unsigned)(int)step;
    for (int n = 0; n <= LOOP_UNROLL_MAX_TRIPS; n++) {
        bool go;
        switch (cmp) {
            case OP_ILT: go = i <  b; break;
            case OP_ILE: go = i <= b; break;
            case OP_IGT: go = i >  b; break;
            case OP_IGE: go = i >= b; break;
            case OP_INE: go = i != b; break;
            default: return -1;
        }
        if (!go) return n;
        i = (int)((unsigned)i + st);
    }
    return -1;
}

// ---------------------------------------------------------------------
// Copying the body with fresh labels
// ---------------------------------------------------------------------
typedef struct { char from[128]; char to[160]; } Rename;

static int g_serial;

static const char *renamed(const Rename *r, int n, const char *name)
{
    for (int i = 0; i < n; i++) if (strcmp(r[i].from, name) == 0) return r[i].to;
    return NULL;
}

static void link_before(AsmNode *pos, AsmNode *nn)
{
    nn->prev = pos->prev;
    nn->next = pos;
    if (pos->prev) pos->prev->next = nn;
    pos->prev = nn;
}

static AsmNode *clone_renamed(const AsmNode *src, const Rename *r, int n)
{
    if (src->type == OP_LABEL) {
        char nm[128];
        get_label_name(src, nm, sizeof(nm));
        const char *to = renamed(r, n, nm);
        char raw[200];
        snprintf(raw, sizeof(raw), "%s:", to ? to : nm);
        return create_node(raw, OP_LABEL, NULL, NULL, NULL);
    }
    char buf[128];
    const char *tgt = jump_target(src, buf, sizeof(buf));
    const char *to  = tgt ? renamed(r, n, tgt) : NULL;
    if (!to) return clone_node((AsmNode *)src);
    char raw[260];
    if (src->type == OP_JMP) {
        snprintf(raw, sizeof(raw), "    JMP %s", to);
        return create_node(raw, OP_JMP, "JMP", to, NULL);
    }
    snprintf(raw, sizeof(raw), "    %s %s, %s", src->type == OP_JT ? "JT" : "JF", src->dst_op.reg, to);
    return create_node(raw, src->type, src->type == OP_JT ? "JT" : "JF", src->dst_op.reg, to);
}

// In every copy Ri holds a known constant, so a register read of it where an
// immediate is accepted becomes that constant: MOV Rd,Ri / IADD Rd,Ri / ILT
// Rd,Ri ... Integer operations only (a float op may read the bits as a
// float; stores and PUSH keep their register operand).
static bool takes_immediate_src(const AsmNode *n)
{
    switch (n->type) {
        case OP_MOV: case OP_IADD: case OP_ISUB: case OP_IMUL: case OP_AND: case OP_OR:
        case OP_XOR: case OP_SHL: case OP_ILT: case OP_ILE: case OP_IGT: case OP_IGE:
        case OP_IEQ: case OP_INE:
            return n->has_dst && n->dst_op.mode == MODE_REG;
        default:
            return false;
    }
}

static void substitute_counter(AsmNode *from, AsmNode *to, const char *ri, long val)
{
    if (!fits_int_literal(val)) return;
    for (AsmNode *n = from; n; n = n->next) {
        if (takes_immediate_src(n) && n->has_src && n->src_op.mode == MODE_REG &&
            str_case_eq(n->src_op.reg, ri) && !str_case_eq(n->dst_op.reg, ri)) {
            char raw[96], imm[24];
            snprintf(imm, sizeof(imm), "%ld", val);
            snprintf(raw, sizeof(raw), "    %s %s, %s", n->mnemonic, n->dst_op.raw, imm);
            node_set_raw(n, raw);
            n->src_op = parse_operand(imm);
        }
        if (n == to) break;
    }
}

// ---------------------------------------------------------------------
// The pass: analysis first (liveness is asked about the program as it is),
// then every planned loop is rewritten.
// ---------------------------------------------------------------------
typedef struct {
    AsmNode *jmp, *start, *mv, *cmp, *jf, *first, *last;
    char     ri[32], rc[32];
    long     initv, stepv;
    int      trips;
    bool     rc_into_body;     // Rc is live at the first body instruction
    bool     rc_at_exit;       // Rc is live at the end label
    Rename  *ren;
    int      nren;
} Plan;

static void apply_plan(Plan *pl)
{
    char rc1[64], rc0[64];
    snprintf(rc1, sizeof(rc1), "    MOV %s, 1", pl->rc);
    snprintf(rc0, sizeof(rc0), "    MOV %s, 0", pl->rc);
    AsmNode *anchor = pl->jmp;                   // copies go before the JMP, which is then dropped
    for (int k = 1; k < pl->trips; k++) {
        ++g_serial;
        for (int i = 0; i < pl->nren; i++) {
            snprintf(pl->ren[i].to, sizeof(pl->ren[0].to), "%s_u%d", pl->ren[i].from, g_serial);
        }
        if (pl->rc_into_body) link_before(anchor, create_node(rc1, OP_MOV, "MOV", pl->rc, "1"));
        AsmNode *mark = anchor->prev;
        for (AsmNode *n = pl->first; ; n = n->next) {
            if (!is_comment_node(n)) link_before(anchor, clone_renamed(n, pl->ren, pl->nren));
            if (n == pl->last) break;
        }
        substitute_counter(mark->next, anchor->prev, pl->ri, pl->initv + (long)k * pl->stepv);
    }
    substitute_counter(pl->first, pl->last, pl->ri, pl->initv);   // copy 0, in place
    if (pl->rc_into_body) link_before(pl->first, create_node(rc1, OP_MOV, "MOV", pl->rc, "1"));
    if (pl->rc_at_exit) link_before(anchor, create_node(rc0, OP_MOV, "MOV", pl->rc, "0"));
    remove_node(pl->jmp);
    remove_node(pl->jf);
    remove_node(pl->cmp);
    remove_node(pl->mv);
    remove_node(pl->start);
    free(pl->ren);
}

static int unroll_once(AsmNode *head)
{
    if (!head) return 0;
    ul_build(head);
    liveness_begin(head);
    int optimizations = 0, nplans = 0, cap = 0;
    Plan *plans = NULL;
    for (AsmNode *n = head; n; n = n->next) n->mark = 0;

    for (AsmNode *jmp = head->next; jmp; ) {
        AsmNode *after_jmp = jmp->next;
        char sbuf[128];
        const char *start_name = (jmp->type == OP_JMP) ? jump_target(jmp, sbuf, sizeof(sbuf)) : NULL;
        UlLabel *start = start_name ? ul_find(start_name) : NULL;
        if (!start || start->duplicate || start->refs != 1 ||
            strncmp(start_name, "__function_", 11) == 0) {
            jmp = after_jmp;
            continue;
        }

        // A loop inside one that is already planned waits for the next round.
        bool overlap = false;
        for (AsmNode *n = start->node; n; n = n->next) {
            if (n->mark) { overlap = true; break; }
            if (n == jmp) break;
        }
        if (overlap) { jmp = after_jmp; continue; }

        // --- the test: MOV Rc,Ri / I<cmp> Rc,imm / JF Rc,end ---------------
        AsmNode *mv = next_code(start->node);
        if (mv && mv->type == OP_LABEL) { jmp = after_jmp; continue; }   // an alias label: leave it alone
        AsmNode *cmp = mv ? next_code(mv) : NULL;
        AsmNode *jf  = cmp ? next_code(cmp) : NULL;
        long bound;
        if (!mv || !cmp || !jf || mv->type != OP_MOV ||
            !mv->has_dst || mv->dst_op.mode != MODE_REG ||
            !mv->has_src || mv->src_op.mode != MODE_REG ||
            !(cmp->type == OP_ILT || cmp->type == OP_ILE || cmp->type == OP_IGT ||
              cmp->type == OP_IGE || cmp->type == OP_INE) ||
            !cmp->has_dst || cmp->dst_op.mode != MODE_REG ||
            !str_case_eq(cmp->dst_op.reg, mv->dst_op.reg) ||
            !imm_value(&cmp->src_op, &bound) ||
            jf->type != OP_JF || !jf->has_dst || jf->dst_op.mode != MODE_REG ||
            !str_case_eq(jf->dst_op.reg, mv->dst_op.reg)) {
            jmp = after_jmp;
            continue;
        }
        const char *rc = mv->dst_op.reg;
        const char *ri = mv->src_op.reg;
        if (!is_scratch_reg(ri) || str_case_eq(rc, ri)) { jmp = after_jmp; continue; }

        char ebuf[128];
        const char *end_name = jump_target(jf, ebuf, sizeof(ebuf));
        UlLabel *end = end_name ? ul_find(end_name) : NULL;
        if (!end || end->duplicate) { jmp = after_jmp; continue; }

        // The JMP is followed directly by the end label.
        bool falls = false;
        for (AsmNode *p = jmp->next; p; p = p->next) {
            if (p->type == OP_LABEL) {
                char b[128];
                get_label_name(p, b, sizeof(b));
                if (strcmp(b, end_name) == 0) { falls = true; break; }
            } else if (!is_comment_node(p)) break;
        }
        if (!falls) { jmp = after_jmp; continue; }

        // --- the step, just before the JMP --------------------------------
        AsmNode *step = jmp->prev;
        while (step && (is_comment_node(step) || step->type == OP_LABEL)) step = step->prev;
        long sv;
        if (!step || (step->type != OP_IADD && step->type != OP_ISUB) ||
            !step->has_dst || step->dst_op.mode != MODE_REG ||
            !str_case_eq(step->dst_op.reg, ri) || !imm_value(&step->src_op, &sv)) {
            jmp = after_jmp;
            continue;
        }
        long stepv = (step->type == OP_IADD) ? sv : -sv;

        // --- the init: the last write of Ri before the start label, within a few
        // straight-line instructions (no label, branch or call in between) ---
        AsmNode *ini = start->node->prev;
        long initv = 0;
        bool have_init = false;
        for (int back = 0; ini && back < 12; ini = ini->prev) {
            if (is_comment_node(ini)) continue;
            if (ini->type == OP_LABEL || ini->type == OP_OTHER || is_branch_or_call(ini) ||
                is_control_flow_boundary(ini)) break;
            back++;
            if (modifies_register(ini, ri)) {
                have_init = ini->type == OP_MOV && ini->has_dst && ini->dst_op.mode == MODE_REG &&
                            ini->has_src && imm_value(&ini->src_op, &initv);
                break;
            }
        }
        if (!have_init) { jmp = after_jmp; continue; }

        int trips = trip_count(cmp->type, initv, stepv, bound);
        if (trips < 1) { jmp = after_jmp; continue; }

        // --- the body: after the JF up to (excluding) the step ------------
        AsmNode *first = jf->next;
        int body_ins = 0;
        bool ok = (first != NULL);
        int nlabels = 0;
        for (AsmNode *n = first; ok && n != step; n = n->next) {
            if (!n) { ok = false; break; }
            if (n->type == OP_LABEL) { nlabels++; continue; }
            if (n->type == OP_OTHER) { if (!is_comment_node(n)) ok = false; continue; }
            if (n->type == OP_CALL) ok = false;
            if (modifies_register(n, ri)) ok = false;
            if (n->type == OP_JMP || n->type == OP_JT || n->type == OP_JF) {
                char b[128];
                if (!jump_target(n, b, sizeof(b))) ok = false;     // computed jump
            }
            body_ins++;
        }
        if (!ok || body_ins == 0 ||
            (long)trips * (body_ins + 1) > LOOP_UNROLL_MAX_TOTAL) {
            jmp = after_jmp;
            continue;
        }

        // Body labels (including the ones between the body and the step):
        // unique, referenced only from inside the body.
        Rename *ren = calloc((size_t)nlabels + 2, sizeof(Rename));
        int nren = 0;
        for (AsmNode *n = first; ok && n != jmp; n = n->next) {
            if (n->type != OP_LABEL) continue;
            if (n == end->node) { ok = false; break; }
            get_label_name(n, ren[nren].from, sizeof(ren[0].from));
            UlLabel *l = ul_find(ren[nren].from);
            if (!l || l->duplicate) { ok = false; break; }
            nren++;
        }
        if (ok && nren > 0) {
            // references inside [first, jmp) must account for every reference
            int *inside = calloc((size_t)g_ul_n + 1, sizeof(int));
            for (AsmNode *n = first; n != jmp; n = n->next) {
                if (n->type == OP_LABEL || n->type == OP_OTHER) continue;
                // jump targets inside the body are fine; any other use of the
                // label (an address taken, a data reference) is not
                char b[128];
                const char *t = jump_target(n, b, sizeof(b));
                for (int i = 0; i < nren; i++) {
                    UlLabel *l = ul_find(ren[i].from);
                    int idx = (int)(l - g_ul);
                    // count tokens of this label in this line
                    const char *s = n->raw; int occ = 0;
                    size_t len = strlen(ren[i].from);
                    while ((s = strstr(s, ren[i].from)) != NULL) {
                        bool lb = (s == n->raw) || !ident_char((unsigned char)s[-1]);
                        bool rb = !ident_char((unsigned char)s[len]);
                        if (lb && rb) occ++;
                        s += len;
                    }
                    if (occ && !(t && strcmp(t, ren[i].from) == 0 && occ == 1)) ok = false;
                    inside[idx] += occ;
                }
            }
            for (int i = 0; ok && i < nren; i++) {
                UlLabel *l = ul_find(ren[i].from);
                if (inside[(int)(l - g_ul)] != l->refs) ok = false;   // referenced from outside
            }
            free(inside);
        }
        if (!ok) { free(ren); jmp = after_jmp; continue; }

        // --- plan it ------------------------------------------------------
        if (nplans == cap) {
            cap = cap ? cap * 2 : 16;
            plans = realloc(plans, (size_t)cap * sizeof(Plan));
        }
        Plan *pl = &plans[nplans++];
        pl->jmp = jmp; pl->start = start->node; pl->mv = mv; pl->cmp = cmp; pl->jf = jf;
        pl->first = first; pl->last = jmp->prev;
        safe_str_copy(pl->ri, ri, sizeof(pl->ri));
        safe_str_copy(pl->rc, rc, sizeof(pl->rc));
        pl->initv = initv; pl->stepv = stepv; pl->trips = trips;
        // The test left Rc = 1 on every iteration and Rc = 0 on the normal exit.
        pl->rc_into_body = !reg_dead_from(first, rc);
        pl->rc_at_exit   = !reg_dead_at_label(end_name, rc);
        pl->ren = ren; pl->nren = nren;
        for (AsmNode *n = start->node; ; n = n->next) {
            n->mark = 1;
            if (n == jmp) break;
        }
        jmp = after_jmp;
    }

    for (int i = 0; i < nplans; i++) {
        apply_plan(&plans[i]);
        optimizations++;
    }
    free(plans);

    liveness_end();
    free(g_ul);
    g_ul = NULL;
    g_ul_n = 0;
    return optimizations;
}

// Unrolling an inner loop makes the outer one a candidate (its body is now
// straight code): go again on a fresh label table until nothing changes.
int peephole_loop_unroll(AsmNode *head)
{
    int total = 0, n;
    for (int round = 0; round < 3 && (n = unroll_once(head)) > 0; round++) total += n;
    return total;
}
