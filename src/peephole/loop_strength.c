#include "v32opt.h"

// ===================================================================
// POINTER STRENGTH REDUCTION  (-floop-strength)        doc/pointer-strength-reduction.md
//
// An address "BASE + K*i" recomputed in every iteration of a counted loop
// becomes a pointer register that is set once in front of the loop and
// stepped with the counter:
//
//         MOV  R6, global_arr          ; preheader, once
//         IADD R6, R4
//   start:  MOV R0, R4 / MOV R1, [BP+2] / ILT R0, R1 / JF R0, end
//         MOV  R1, [R6]                ; was MOV R1, global_arr / IADD R1, R4 / MOV R1, [R1]
//         IADD R5, R1
//         IADD R6, 1                   ; the pointer step, right BEFORE the counter's
//         IADD R4, 1                   ; the step
//         JMP  start
//
// Invariant: everywhere in the loop except between the two steps, Rp == BASE + K*Ri
// (32-bit wrap-around: exact, no overflow condition).
//
// The pointer step goes immediately before the counter step, not after it as the
// design note had it: loop_parse_counted() (and so loop-unroll-partial, loop-invert
// and this pass) takes the instruction right before the back JMP as the step, so a
// pointer step there would hide the loop from the passes that run after this one.
// Nothing can sit between the two steps: the pointer step is linked directly in
// front of the step node, after any "continue" label in front of it.
//
// Address groups (consecutive instructions in [L.first, L.step), comments allowed
// between them, labels not):
//   unit    MOV Rt, BASE / IADD Rt, Ri                                  BASE + Ri
//   scaled  MOV Rt, BASE / MOV Ru, Ri / IMUL Ru, K (SHL Ru, s) / IADD Rt, Ru    BASE + K*Ri
//   frame   LEA Rt, [BP+off] / IADD Rt, Ri                              BP+off + Ri
//   offset  MOV Rt, BASE / MOV Ru, Ri / IADD Ru, c (ISUB) / IADD Rt, Ru  BASE + Ri + c
// ("offset" is the form both compilers emit for arr[i + c]; it uses the pointer of
// (BASE, 1) and adds c to the folded operand, or keeps one IADD Rt, c.)
// BASE is an immediate operand (number, label, %define) or a register R0..R10 that
// nothing in the loop writes.
//
// Each group is either folded into the next instruction (it uses Rt only as the base
// of one memory operand, Rt dies there or is reloaded by it: the operand becomes
// [Rp+o]) or reduced to "MOV Rt, Rp".
//
// Legality (numbers as in the design note): 1 loop_parse_counted(); 2 no CALL,
// computed jump or directive in the body, no volatile instruction in a group; 3 Ri
// written only by the step, BP not written in the loop; 4 no label of the loop
// referenced from outside it; 5 not cold in the profile; 6 Ru dead after its group;
// 7 K != 0 and K*step (and every folded offset) a valid literal; 8 Rt, Ru, Rb not
// Ri/SP/BP, Rb not written in the loop. Pointer registers follow promote-loops
// (promote_pick_loop_register): not mentioned in the loop, dead at its start label,
// already saved or a fresh PUSH/POP of R4..R10 (callee-save ABI). At most two
// pointers per loop, the one with the most groups first; a pointer is taken when the
// instructions it saves per iteration, less its step, are at least 1.
//
// TRIGGER CAP: one slot per rewritten loop. Runs once, after loop-index and before
// loop-unroll-partial and loop-invert (main.c).
// ===================================================================

#define LS_MAX_GROUPS 32
#define LS_MAX_PTRS   2

enum { G_UNIT, G_SCALED, G_FRAME, G_OFFSET };
enum { B_IMM, B_REG, B_FRAME };

typedef struct {
    int      form;
    AsmNode *n[4];                 // the group's instructions
    int      len;
    char     rt[16], ru[16];
    int      bkind;                // B_IMM / B_REG / B_FRAME
    char     base[128];            // operand text (B_IMM), register (B_REG), "[BP+off]" text (B_FRAME)
    long     k;
    long     c;                    // G_OFFSET: the constant added
    AsmNode *use;                  // folding target, or NULL
    int      use_op;               // 0: dst_op, 1: src_op of `use`
    int      ptr;                  // index into the pointer table, -1: not taken
} Group;

typedef struct {
    int      bkind;
    char     base[128];
    long     k;
    int      ngroups;
    int      saved;
    int      reg;                  // chosen R1..R10
    bool     fresh;
} Ptr;

static int reg_num(const char *r)            // R0..R10 by name, else -1
{
    if (!r || toupper((unsigned char)r[0]) != 'R') return -1;
    int i = get_reg_index(r);
    return (i >= 0 && i <= 10) ? i : -1;
}

static bool is_reg(const Operand *op, const char *r)
{
    return op->mode == MODE_REG && str_case_eq(op->reg, r);
}

static bool int_imm(const Operand *op, long *v)
{
    if (op->mode != MODE_IMMEDIATE || op->is_float || !is_numeric_immediate(op)) return false;
    *v = (long)op->immediate;
    return true;
}

static AsmNode *next_code_nolabel(AsmNode *n, AsmNode *limit)   // next non-comment, NULL at a label/limit
{
    for (n = n->next; n && n != limit; n = n->next) {
        if (loop_is_comment(n)) continue;
        if (n->type == OP_LABEL || n->type == OP_OTHER) return NULL;
        return n;
    }
    return NULL;
}

static bool written_in(AsmNode *from, AsmNode *last, const char *r, AsmNode *except)
{
    for (AsmNode *n = from; n; n = n->next) {
        if (n != except && n->type != OP_LABEL && n->type != OP_OTHER && modifies_register(n, r)) return true;
        if (n == last) break;
    }
    return false;
}

// "MOV Rt, BASE" (BASE an immediate or a register Rb) or "LEA Rt, [BP+off]" opening a group.
static bool group_head(AsmNode *n, const char *ri, Group *g)
{
    if (!n->has_dst || !n->has_src || n->dst_op.mode != MODE_REG) return false;
    if (reg_num(n->dst_op.reg) < 0 || str_case_eq(n->dst_op.reg, ri)) return false;
    safe_str_copy(g->rt, n->dst_op.reg, sizeof(g->rt));
    if (n->type == OP_LEA) {
        if (n->src_op.mode != MODE_INDIRECT || !str_case_eq(n->src_op.reg, "BP")) return false;
        g->bkind = B_FRAME;
        snprintf(g->base, sizeof(g->base), "%d", n->src_op.offset);
        return true;
    }
    if (n->type != OP_MOV) return false;
    if (n->src_op.mode == MODE_IMMEDIATE && !n->src_op.is_float) {
        g->bkind = B_IMM;
        safe_str_copy(g->base, n->src_op.raw, sizeof(g->base));
        trim(g->base);
        return g->base[0] != '\0';
    }
    if (n->src_op.mode == MODE_REG && reg_num(n->src_op.reg) >= 0 &&
        !str_case_eq(n->src_op.reg, ri) && !str_case_eq(n->src_op.reg, g->rt)) {
        g->bkind = B_REG;
        safe_str_copy(g->base, n->src_op.reg, sizeof(g->base));
        return true;
    }
    return false;
}

// Can `n` take [Rp+o] for the group's Rt? Sets g->use / g->use_op.
static void plan_fold(Group *g)
{
    AsmNode *last = g->n[g->len - 1];
    AsmNode *n = next_code_nolabel(last, NULL);
    g->use = NULL;
    if (!n || n->type == OP_OTHER || node_is_volatile(n)) return;
    const char *rt = g->rt;
    int mem = -1;
    const Operand *ops[2] = { n->has_dst ? &n->dst_op : NULL, n->has_src ? &n->src_op : NULL };
    for (int i = 0; i < 2; i++) {
        const Operand *op = ops[i];
        if (!op) continue;
        if (op->mode == MODE_INDIRECT && str_case_eq(op->reg, rt)) {
            if (mem >= 0) return;                       // two memory operands on Rt
            mem = i;
        }
    }
    if (mem < 0) return;
    const Operand *other = ops[1 - mem];
    bool reload = false;
    if (other && (other->mode == MODE_REG || other->mode == MODE_INDIRECT) && str_case_eq(other->reg, rt)) {
        // only "MOV Rt, [Rt+o]": the destination is a pure write
        if (!(n->type == OP_MOV && mem == 1 && other->mode == MODE_REG)) return;
        reload = true;
    }
    // nothing else in the text may name Rt (the operands above are all there is)
    long o = ops[mem]->offset + (g->form == G_OFFSET ? g->c : 0);
    if (!fits_int_literal(o)) return;
    if (!reload && !(n->next && reg_dead_from(n->next, rt))) return;
    g->use = n;
    g->use_op = mem;
}

static AsmNode *emit_before(AsmNode *pos, const char *raw, OpType t, const char *mn,
                            const char *d, const char *s, const AsmNode *loc)
{
    AsmNode *nn = create_node(raw, t, mn, d, s);
    srcmarks_inherit(nn, loc);
    loop_link_before(pos, nn);
    return nn;
}

static void set_mem_operand(AsmNode *n, int which, const char *rp, long o)
{
    char txt[64];
    if (o > 0) snprintf(txt, sizeof(txt), "[%s+%ld]", rp, o);
    else if (o < 0) snprintf(txt, sizeof(txt), "[%s-%ld]", rp, -o);
    else snprintf(txt, sizeof(txt), "[%s]", rp);
    Operand op = parse_operand(txt);
    if (which == 0) n->dst_op = op; else n->src_op = op;
    char raw[300];
    if (n->has_dst && n->has_src) snprintf(raw, sizeof(raw), "    %s %s, %s", n->mnemonic, n->dst_op.raw, n->src_op.raw);
    else                          snprintf(raw, sizeof(raw), "    %s %s", n->mnemonic, n->dst_op.raw);
    node_set_raw(n, raw);
}

static void drop(AsmNode *n)
{
    insert_debug_comment(n->prev, OPT_LOOP_STRENGTH, n->raw);
    remove_node(n);
}

// One loop. Returns 1 if rewritten.
static int strength_loop(AsmNode *jmp)
{
    CountedLoop L;
    if (!loop_parse_counted(jmp, &L)) return 0;                                   // 1
    const char *ri = L.ri;
    if (reg_num(ri) < 0) return 0;
    if (loop_heat(L.start, jmp) < 0) return 0;                                    // 5

    // 2, 3, 4 over the whole loop
    bool bp_written = false;
    for (AsmNode *n = L.start; n != jmp; n = n->next) {
        if (n->type == OP_LABEL) {
            if (n == L.start) continue;
            char nm[128];
            get_label_name(n, nm, sizeof(nm));
            LoopLabel *l = loop_label_find(nm);
            if (!l || l->duplicate) return 0;
            int inside = 0;                                                       // 4
            for (AsmNode *q = L.start; ; q = q->next) {
                if (q->type != OP_LABEL && !q->debug_note) {
                    char b[128];
                    const char *t = (q->type == OP_JMP || q->type == OP_JT || q->type == OP_JF)
                                    ? loop_jump_target(q, b, sizeof(b)) : NULL;
                    if (t && strcmp(t, nm) == 0) inside++;
                    else if (loop_mentions_reg(q, nm)) inside++;   // any other mention (token)
                }
                if (q == jmp) break;
            }
            if (inside != l->refs) return 0;
            continue;
        }
        if (loop_is_comment(n)) continue;
        if (n->type == OP_OTHER) return 0;                                        // 2: directive
        if (n->type == OP_CALL) return 0;                                         // 2
        if (n->type == OP_JMP || n->type == OP_JT || n->type == OP_JF) {
            char b[128];
            if (!loop_jump_target(n, b, sizeof(b))) return 0;                     // 2: computed
        }
        if (n != L.step && modifies_register(n, ri)) return 0;                    // 3
        if (modifies_register(n, "BP")) bp_written = true;
    }

    // the groups
    Group g[LS_MAX_GROUPS];
    int ng = 0;
    for (AsmNode *n = L.first; n && n != L.step; n = n->next) {
        if (n->type == OP_LABEL || loop_is_comment(n)) continue;
        Group cand;
        memset(&cand, 0, sizeof(cand));
        cand.ptr = -1;
        if (!group_head(n, ri, &cand)) continue;
        AsmNode *a = next_code_nolabel(n, L.step);
        if (!a) continue;
        bool ok = false;
        if (a->type == OP_IADD && is_reg(&a->dst_op, cand.rt) && a->has_src && is_reg(&a->src_op, ri)) {
            cand.form = (cand.bkind == B_FRAME) ? G_FRAME : G_UNIT;
            cand.n[0] = n; cand.n[1] = a; cand.len = 2; cand.k = 1;
            ok = true;
        } else if (cand.bkind != B_FRAME && a->type == OP_MOV && a->has_dst && a->dst_op.mode == MODE_REG &&
                   a->has_src && is_reg(&a->src_op, ri) && reg_num(a->dst_op.reg) >= 0 &&
                   !str_case_eq(a->dst_op.reg, cand.rt) && !str_case_eq(a->dst_op.reg, ri) &&
                   !(cand.bkind == B_REG && str_case_eq(a->dst_op.reg, cand.base))) {
            const char *ru = a->dst_op.reg;
            AsmNode *b = next_code_nolabel(a, L.step);
            AsmNode *c = b ? next_code_nolabel(b, L.step) : NULL;
            long v;
            if (b && c && b->has_dst && is_reg(&b->dst_op, ru) && b->has_src && int_imm(&b->src_op, &v) &&
                c->type == OP_IADD && is_reg(&c->dst_op, cand.rt) && c->has_src && is_reg(&c->src_op, ru)) {
                if (b->type == OP_IMUL) { cand.form = G_SCALED; cand.k = v; ok = true; }
                else if (b->type == OP_SHL && v >= 0 && v <= 30) { cand.form = G_SCALED; cand.k = 1L << v; ok = true; }
                else if (b->type == OP_IADD || b->type == OP_ISUB) {
                    cand.form = G_OFFSET; cand.k = 1; cand.c = (b->type == OP_IADD) ? v : -v; ok = true;
                }
                if (ok) {
                    safe_str_copy(cand.ru, ru, sizeof(cand.ru));
                    cand.n[0] = n; cand.n[1] = a; cand.n[2] = b; cand.n[3] = c; cand.len = 4;
                }
            }
        }
        if (!ok) continue;
        bool vol = false;
        for (int i = 0; i < cand.len; i++) if (node_is_volatile(cand.n[i])) vol = true;
        if (vol) return 0;                                                        // 2
        if (cand.bkind == B_FRAME && bp_written) continue;                        // 3
        if (cand.bkind == B_REG && written_in(L.start, jmp, cand.base, NULL)) continue;   // 8
        if (cand.k == 0 || !fits_int_literal(cand.k * L.stepv) ||
            !fits_int_literal(labs(cand.k * L.stepv))) continue;                  // 7
        if (cand.ru[0] && !reg_dead_from(cand.n[3]->next, cand.ru)) continue;     // 6
        plan_fold(&cand);
        if (ng < LS_MAX_GROUPS) g[ng++] = cand;
        n = cand.n[cand.len - 1];
    }
    if (ng == 0) return 0;

    // pointers: one per (base, K), most groups first, at most two
    Ptr p[LS_MAX_GROUPS];
    int np = 0;
    for (int i = 0; i < ng; i++) {
        int j;
        for (j = 0; j < np; j++)
            if (p[j].bkind == g[i].bkind && p[j].k == g[i].k && strcmp(p[j].base, g[i].base) == 0) break;
        if (j == np) {
            memset(&p[np], 0, sizeof(p[np]));
            p[np].bkind = g[i].bkind; p[np].k = g[i].k;
            safe_str_copy(p[np].base, g[i].base, sizeof(p[np].base));
            np++;
        }
        p[j].ngroups++;
        p[j].saved += g[i].use ? g[i].len : (g[i].form == G_OFFSET ? g[i].len - 2 : g[i].len - 1);
        g[i].ptr = j;
    }
    int order[LS_MAX_GROUPS];
    for (int i = 0; i < np; i++) order[i] = i;
    for (int i = 1; i < np; i++) {
        int v = order[i], j = i - 1;
        while (j >= 0 && p[order[j]].ngroups < p[v].ngroups) { order[j + 1] = order[j]; j--; }
        order[j + 1] = v;
    }

    FrameInfo fi;
    if (!promote_loop_frame(&fi, L.start)) return 0;
    int take[LS_MAX_PTRS], nt = 0;
    unsigned exclude = 0;
    for (int oi = 0; oi < np && nt < LS_MAX_PTRS; oi++) {
        Ptr *q = &p[order[oi]];
        if (q->saved - 1 < 1) continue;                                           // profit
        bool fresh = false;
        int r = promote_pick_loop_register(L.start, jmp, &fi, exclude, &fresh);
        if (r < 0) break;
        q->reg = r;
        q->fresh = fresh;
        exclude |= 1u << r;
        take[nt++] = order[oi];
    }
    if (nt == 0) return 0;

    if (!trigger_allowed()) return 0;

    // ---- apply ----
    for (int t = 0; t < nt; t++) {
        Ptr *q = &p[take[t]];
        if (q->fresh && !promote_frame_save_reg(&fi, q->reg, OPT_LOOP_STRENGTH)) {
            // cannot happen (picked only when frame_save_reg can add it); keep the pointer out
            q->reg = -1;
            continue;
        }
        char rp[8], raw[300], kb[32];
        snprintf(rp, sizeof(rp), "R%d", q->reg);
        AsmNode *loc = L.test_first;
        AsmNode *first_new = NULL;
        if (q->k == 1) {
            if (q->bkind == B_FRAME) {
                char m[48];
                snprintf(m, sizeof(m), "[BP%s%ld]", atol(q->base) < 0 ? "-" : "+", labs(atol(q->base)));
                snprintf(raw, sizeof(raw), "    LEA %s, %s", rp, m);
                first_new = emit_before(L.start, raw, OP_LEA, "LEA", rp, m, loc);
            } else {
                snprintf(raw, sizeof(raw), "    MOV %s, %s", rp, q->base);
                first_new = emit_before(L.start, raw, OP_MOV, "MOV", rp, q->base, loc);
            }
            snprintf(raw, sizeof(raw), "    IADD %s, %s", rp, ri);
            emit_before(L.start, raw, OP_IADD, "IADD", rp, ri, loc);
        } else {
            snprintf(kb, sizeof(kb), "%ld", q->k);
            snprintf(raw, sizeof(raw), "    MOV %s, %s", rp, ri);
            first_new = emit_before(L.start, raw, OP_MOV, "MOV", rp, ri, loc);
            snprintf(raw, sizeof(raw), "    IMUL %s, %s", rp, kb);
            emit_before(L.start, raw, OP_IMUL, "IMUL", rp, kb, loc);
            snprintf(raw, sizeof(raw), "    IADD %s, %s", rp, q->base);
            emit_before(L.start, raw, OP_IADD, "IADD", rp, q->base, loc);
        }
        if (config.debug) {
            char dbg[200];
            snprintf(dbg, sizeof(dbg), "LOOP-STRENGTH: %s = %s + %ld*%s through the loop below", rp,
                     q->bkind == B_FRAME ? "BP" : q->base, q->k, ri);
            insert_debug_comment(first_new->prev, OPT_LOOP_STRENGTH, dbg);
        }
        // the pointer step, right in front of the counter's
        long d = q->k * L.stepv;
        snprintf(kb, sizeof(kb), "%ld", labs(d));
        snprintf(raw, sizeof(raw), "    %s %s, %s", d >= 0 ? "IADD" : "ISUB", rp, kb);
        AsmNode *st = create_node(raw, d >= 0 ? OP_IADD : OP_ISUB, d >= 0 ? "IADD" : "ISUB", rp, kb);
        srcmarks_inherit(st, L.step);
        loop_link_before(L.step, st);
    }

    for (int i = 0; i < ng; i++) {
        if (g[i].ptr < 0) continue;
        Ptr *q = &p[g[i].ptr];
        bool taken = false;
        for (int t = 0; t < nt; t++) if (take[t] == g[i].ptr && q->reg >= 0) taken = true;
        if (!taken) continue;
        char rp[8], raw[300];
        snprintf(rp, sizeof(rp), "R%d", q->reg);
        if (g[i].use) {
            AsmNode *u = g[i].use;
            const Operand *op = g[i].use_op == 0 ? &u->dst_op : &u->src_op;
            long o = op->offset + (g[i].form == G_OFFSET ? g[i].c : 0);
            insert_debug_comment(u->prev, OPT_LOOP_STRENGTH, u->raw);
            set_mem_operand(u, g[i].use_op, rp, o);
            for (int k = 0; k < g[i].len; k++) drop(g[i].n[k]);
        } else {
            AsmNode *h = g[i].n[0];
            insert_debug_comment(h->prev, OPT_LOOP_STRENGTH, h->raw);
            snprintf(raw, sizeof(raw), "    MOV %s, %s", g[i].rt, rp);
            node_set_raw(h, raw);
            h->type = OP_MOV;
            safe_str_copy(h->mnemonic, "MOV", sizeof(h->mnemonic));
            h->src_op = parse_operand(rp);
            if (g[i].form == G_OFFSET) {
                AsmNode *c = g[i].n[3];                         // IADD Rt, Ru -> IADD Rt, c
                char cb[32];
                snprintf(cb, sizeof(cb), "%ld", labs(g[i].c));
                insert_debug_comment(c->prev, OPT_LOOP_STRENGTH, c->raw);
                snprintf(raw, sizeof(raw), "    %s %s, %s", g[i].c >= 0 ? "IADD" : "ISUB", g[i].rt, cb);
                node_set_raw(c, raw);
                c->type = g[i].c >= 0 ? OP_IADD : OP_ISUB;
                safe_str_copy(c->mnemonic, g[i].c >= 0 ? "IADD" : "ISUB", sizeof(c->mnemonic));
                c->src_op = parse_operand(cb);
                drop(g[i].n[1]);
                drop(g[i].n[2]);
            } else {
                for (int k = 1; k < g[i].len; k++) drop(g[i].n[k]);
            }
        }
    }
    return 1;
}

int peephole_loop_strength(AsmNode *head)
{
    if (!head) return 0;
    loop_labels_build_all(head);
    liveness_begin(head);
    int optimizations = 0;
    for (AsmNode *jmp = head->next; jmp; jmp = jmp->next) {
        if (jmp->type != OP_JMP) continue;
        optimizations += strength_loop(jmp);
    }
    liveness_end();
    loop_labels_free();
    return optimizations;
}
