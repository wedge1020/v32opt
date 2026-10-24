#include "v32opt.h"

// ===================================================================
// LOOP INDEX FOLDING  (-floop-index)
//
// Array code indexes by "base + i":
//
//   start:  MOV R0, R4              the test
//           ILT R0, 64
//           JF  R0, end
//           MOV R1, global_arr      base + i
//           IADD R1, R4
//           MOV R1, [R1]
//           ...
//           IADD R4, 1
//           JMP start
//
// When the counter is used ONLY to form addresses of one base, the base can
// be folded into the counter itself -- no extra register:
//
//           IADD R4, global_arr     once, before the loop
//   start:  MOV R0, R4
//           ILT R0, 64+global_arr   the bound moves with the counter
//           JF  R0, end
//           MOV R1, R4              the address IS the counter
//           MOV R1, [R1]
//           ...
//
// That is one instruction fewer per use per iteration, for one instruction
// once.
//
// SOUNDNESS. The counter R4 now holds i + base, so every read of it must be
// an address computation of that very base, and the value must not be needed
// afterwards:
//   * every mention of R4 in the loop (besides the step and the test's copy)
//     is the second half of a directly adjacent "MOV Rt, BASE / IADD Rt, R4"
//     with the same BASE, a plain number or a %define'd one; the pair becomes
//     "MOV Rt, R4";
//   * no CALL in the body (the callee could read R4), no computed jump;
//   * R4 is dead at the loop's end label, at every jump out of the loop and at
//     every RET in it (liveness.c);
//   * the bound is an immediate (it is shifted by BASE at compile time) and
//     neither it nor BASE comes near the 32-bit limits;
//   * the loop is entered only through its start label's fall-through (no other
//     reference to it), so the one "IADD R4, BASE" placed in front of it
//     always runs.
// The loop shape is the one parsed by loop_parse_counted() (loops.c).
//
// Runs once, after loop-unroll (which removes the small constant loops
// completely) and before loop-invert.
// ===================================================================

#define LOOP_INDEX_MAX_PAIRS 16

typedef struct {
    AsmNode *movs[LOOP_INDEX_MAX_PAIRS];
    AsmNode *adds[LOOP_INDEX_MAX_PAIRS];
    int      npairs;
    AsmNode *cmp;
    AsmNode *start;
    char     ri[32];
    char     base_text[128];
    long     new_bound;
} IndexPlan;

// A base: a plain integer or a %define that resolves to one.
static bool base_value(const Operand *op, long *v)
{
    if (op->mode != MODE_IMMEDIATE || op->is_float) return false;
    if (is_numeric_immediate(op)) { *v = (long)op->immediate; return true; }
    int d;
    if (define_int_value(op->raw, &d)) { *v = d; return true; }
    return false;
}

static bool sane(long v) { return v > -(1L << 30) && v < (1L << 30); }

static void apply_plan(IndexPlan *pl)
{
    // IADD Ri, BASE in front of the loop
    char raw[200];
    snprintf(raw, sizeof(raw), "    IADD %s, %s", pl->ri, pl->base_text);
    loop_link_before(pl->start, create_node(raw, OP_IADD, "IADD", pl->ri, pl->base_text));

    // the bound moves with the counter
    char imm[24];
    snprintf(imm, sizeof(imm), "%ld", pl->new_bound);
    snprintf(raw, sizeof(raw), "    %s %s, %s", pl->cmp->mnemonic, pl->cmp->dst_op.raw, imm);
    node_set_raw(pl->cmp, raw);
    pl->cmp->src_op = parse_operand(imm);

    // MOV Rt, BASE / IADD Rt, Ri  ->  MOV Rt, Ri
    for (int i = 0; i < pl->npairs; i++) {
        AsmNode *mv = pl->movs[i];
        snprintf(raw, sizeof(raw), "    %s %s, %s", mv->mnemonic, mv->dst_op.raw, pl->ri);
        node_set_raw(mv, raw);
        mv->src_op = parse_operand(pl->ri);
        remove_node(pl->adds[i]);
    }
}

int peephole_loop_index(AsmNode *head)
{
    if (!head) return 0;
    loop_labels_build(head);
    liveness_begin(head);
    int optimizations = 0, nplans = 0, cap = 0;
    IndexPlan *plans = NULL;
    for (AsmNode *n = head; n; n = n->next) n->mark = 0;

    for (AsmNode *jmp = head->next; jmp; jmp = jmp->next) {
        CountedLoop L;
        if (jmp->type != OP_JMP || !loop_parse_counted(jmp, &L) || !L.bound_imm) continue;
        const char *ri = L.ri;

        // a loop already planned (inside this one, or this one's twin) is not touched again
        bool overlap = false;
        for (AsmNode *n = L.start; n; n = n->next) {
            if (n->mark) { overlap = true; break; }
            if (n == jmp) break;
        }
        if (overlap) continue;

        // labels defined inside the body (jumps to them stay in the loop)
        int nl = 0;
        for (AsmNode *n = L.first; n != jmp; n = n->next) if (n->type == OP_LABEL) nl++;
        char (*inside)[128] = malloc((size_t)(nl + 1) * sizeof(*inside));
        nl = 0;
        for (AsmNode *n = L.first; n != jmp; n = n->next)
            if (n->type == OP_LABEL) get_label_name(n, inside[nl++], sizeof(inside[0]));

        IndexPlan pl;
        memset(&pl, 0, sizeof(pl));
        long base = 0;
        bool ok = true;
        if (!reg_dead_at_label(L.end_name, ri)) ok = false;

        for (AsmNode *n = L.first; ok && n != jmp; n = n->next) {
            if (n->type == OP_LABEL || loop_is_comment(n)) continue;
            if (n->type == OP_OTHER || n->type == OP_CALL) { ok = false; break; }
            if (n == L.step) continue;

            if (n->type == OP_RET && !reg_dead_from(n, ri)) { ok = false; break; }
            if (n->type == OP_JMP || n->type == OP_JT || n->type == OP_JF) {
                char b[128];
                const char *t = loop_jump_target(n, b, sizeof(b));
                if (!t) { ok = false; break; }                       // computed jump
                bool in = false;
                for (int i = 0; i < nl && !in; i++) in = strcmp(inside[i], t) == 0;
                if (!in && strcmp(t, L.end_name) != 0 && !reg_dead_at_label(t, ri)) { ok = false; break; }
            }

            if (!loop_mentions_reg(n, ri)) continue;

            // the only allowed mention: IADD Rt, Ri right after MOV Rt, BASE
            AsmNode *mv = n->prev;
            while (mv && loop_is_comment(mv)) mv = mv->prev;
            long bv;
            if (n->type != OP_IADD || !n->has_dst || n->dst_op.mode != MODE_REG ||
                !n->has_src || n->src_op.mode != MODE_REG ||
                !str_case_eq(n->src_op.reg, ri) || str_case_eq(n->dst_op.reg, ri) ||
                !mv || mv->type != OP_MOV || !mv->has_dst || mv->dst_op.mode != MODE_REG ||
                !str_case_eq(mv->dst_op.reg, n->dst_op.reg) ||
                !mv->has_src || !base_value(&mv->src_op, &bv) || mv == L.step ||
                pl.npairs >= LOOP_INDEX_MAX_PAIRS) {
                ok = false;
                break;
            }
            if (pl.npairs == 0) {
                base = bv;
                safe_str_copy(pl.base_text, mv->src_op.raw, sizeof(pl.base_text));
            } else if (bv != base || strcmp(pl.base_text, mv->src_op.raw) != 0) {
                ok = false;
                break;
            }
            // the MOV must not itself be a pair member already (it is not: it never mentions Ri)
            pl.movs[pl.npairs] = mv;
            pl.adds[pl.npairs] = n;
            pl.npairs++;
        }
        free(inside);
        if (!ok || pl.npairs == 0 || !sane(base) || !sane(L.bound) || !sane(L.bound + base)) continue;

        pl.cmp = L.cmp;
        pl.start = L.start;
        safe_str_copy(pl.ri, ri, sizeof(pl.ri));
        pl.new_bound = L.bound + base;

        if (nplans == cap) {
            cap = cap ? cap * 2 : 16;
            plans = realloc(plans, (size_t)cap * sizeof(IndexPlan));
        }
        plans[nplans++] = pl;
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
