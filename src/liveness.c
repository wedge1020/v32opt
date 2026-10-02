#include "v32opt.h"

// ===================================================================
// REGISTER LIVENESS QUERY
//
//   liveness_begin(head);
//   ... reg_dead_from(node, "R0") ...
//   liveness_end();
//
// reg_dead_from(start, reg) answers: starting execution AT 'start', is
// the current value of 'reg' guaranteed never to be read on ANY path?
// Unlike the straight-line scans in helpers.c it follows control flow:
//
//   - JMP label        -> continue at the label
//   - JT/JF r, label   -> both the branch target and the fall-through
//   - CALL label       -> the callee is analysed: if it can read the
//                         register before writing it, the register is
//                         live; if it overwrites it on every path, it
//                         is dead; if some path returns with it
//                         untouched, analysis continues after the CALL
//   - RET              -> live iff is_live_out_register(reg)
//   - HLT              -> dead
//
// It is a "may be read" analysis and every unknown answers LIVE:
// computed JMP/CALL (register or non-label operand), a label that
// cannot be found, a data directive in the instruction stream, running
// off the end of the file, the step or call-depth budget running out.
// So "true" is a proof; "false" only means "could not prove it".
//
// Only R0..R10 can be queried. R11-R13 are also CR/SR/DR (implicitly
// read by MOVS/SETS/CMPS), R14/R15 are BP/SP; aliases make textual
// operand matching unreliable for those, so they always answer LIVE.
//
// The label index is built once by liveness_begin() and must be rebuilt
// if labels are added or removed. Removing or rewriting ordinary
// instructions between queries is fine: every query walks the live list.
// ===================================================================

#define LV_LIVE      1   // may be read before being written
#define LV_PASSTHRU  2   // reached a RET with the register untouched

#define LV_MAX_STEPS 20000
#define LV_MAX_DEPTH 8

typedef struct {
    char     name[128];
    AsmNode *node;
} LabelEntry;

static LabelEntry *g_labels      = NULL;
static int         g_label_count = 0;
static int        *g_stamp       = NULL;   // per-label visit stamp
static int         g_stamp_id    = 0;
static signed char (*g_callee)[11] = NULL; // per-label, per-reg cache (-1 = unknown)
static int         g_steps       = 0;

static int cmp_label(const void *a, const void *b) {
    return strcasecmp(((const LabelEntry *)a)->name, ((const LabelEntry *)b)->name);
}

void liveness_end(void) {
    free(g_labels); free(g_stamp); free(g_callee);
    g_labels = NULL; g_stamp = NULL; g_callee = NULL;
    g_label_count = 0;
}

void liveness_begin(AsmNode *head) {
    liveness_end();
    int cap = 0;
    for (AsmNode *n = head; n; n = n->next)
        if (n->type == OP_LABEL) cap++;
    if (cap == 0) return;
    g_labels = calloc((size_t)cap, sizeof(LabelEntry));
    g_stamp  = calloc((size_t)cap, sizeof(int));
    g_callee = malloc((size_t)cap * sizeof(*g_callee));
    if (!g_labels || !g_stamp || !g_callee) { liveness_end(); return; }
    memset(g_callee, -1, (size_t)cap * sizeof(*g_callee));
    for (AsmNode *n = head; n; n = n->next) {
        if (n->type != OP_LABEL) continue;
        get_label_name(n, g_labels[g_label_count].name, sizeof(g_labels[0].name));
        g_labels[g_label_count].node = n;
        g_label_count++;
    }
    qsort(g_labels, (size_t)g_label_count, sizeof(LabelEntry), cmp_label);
    // A duplicated label name makes the target ambiguous: drop both, so
    // any jump to it answers LIVE.
    for (int i = 0; i + 1 < g_label_count; i++)
        if (strcasecmp(g_labels[i].name, g_labels[i + 1].name) == 0)
            g_labels[i].node = g_labels[i + 1].node = NULL;
    g_stamp_id = 0;
}

static int find_label(const char *name) {
    if (!g_labels || !name) return -1;
    char buf[128];
    safe_str_copy(buf, name, sizeof(buf));
    LabelEntry key;
    safe_str_copy(key.name, trim(buf), sizeof(key.name));
    LabelEntry *e = bsearch(&key, g_labels, (size_t)g_label_count, sizeof(LabelEntry), cmp_label);
    if (!e || !e->node) return -1;
    return (int)(e - g_labels);
}

static int label_index_of(AsmNode *label_node) {
    char name[128];
    get_label_name(label_node, name, sizeof(name));
    int idx = find_label(name);
    return (idx >= 0 && g_labels[idx].node == label_node) ? idx : -1;
}

static const char *branch_target(const AsmNode *n) {
    // JMP/CALL: sole operand. JT/JF: second operand.
    if (n->type == OP_JT || n->type == OP_JF) return n->has_src ? n->src_op.raw : NULL;
    return n->has_dst ? n->dst_op.raw : (n->has_src ? n->src_op.raw : NULL);
}

static int flow(AsmNode *node, const char *reg, int ridx, int depth, int stamp);

// Callee summary for (label, reg): LV_LIVE and/or LV_PASSTHRU bits.
static int callee_summary(int lbl, const char *reg, int ridx, int depth) {
    if (g_callee[lbl][ridx] >= 0) return g_callee[lbl][ridx];
    if (depth >= LV_MAX_DEPTH) return LV_LIVE;
    int r = flow(g_labels[lbl].node, reg, ridx, depth + 1, ++g_stamp_id);
    // Only cache a result that didn't hit the step budget (LIVE from an
    // exhausted budget is query-specific, not a property of the callee).
    if (g_steps < LV_MAX_STEPS) g_callee[lbl][ridx] = (signed char)r;
    return r;
}

static int flow(AsmNode *node, const char *reg, int ridx, int depth, int stamp) {
    int result = 0;
    for (AsmNode *n = node; ; n = n->next) {
        if (!n) return result | LV_LIVE;                 // fell off the file
        if (++g_steps > LV_MAX_STEPS) return result | LV_LIVE;

        if (n->type == OP_LABEL) {
            int li = label_index_of(n);
            if (li < 0) return result | LV_LIVE;
            if (g_stamp[li] == stamp) return result;     // already explored
            g_stamp[li] = stamp;
            continue;
        }
        if (n->type == OP_OTHER) {
            if (is_data_directive_node(n)) return result | LV_LIVE;
            continue;
        }

        if (n->type == OP_HLT) return result;
        if (n->type == OP_RET) return result | LV_PASSTHRU;

        if (n->type == OP_CALL) {
            const char *t = branch_target(n);
            int li = (t && !is_register_operand(t)) ? find_label(t) : -1;
            if (li < 0) return result | LV_LIVE;
            int cs = callee_summary(li, reg, ridx, depth);
            if (cs & LV_LIVE) return result | LV_LIVE;
            if (!(cs & LV_PASSTHRU)) return result;      // overwritten on every path
            continue;                                    // may survive the call
        }

        // POP Rd / IN Rd, port overwrite Rd without reading it.
        // (is_register_read() conservatively reports any non-MOV register
        // destination as a read; these two are pure writes.)
        if ((n->type == OP_POP || n->type == OP_IN) && n->has_dst &&
            n->dst_op.mode == MODE_REG && str_case_eq(n->dst_op.reg, reg))
            return result;

        // Ordinary instruction (and JT/JF's tested register): a read
        // makes it live; otherwise a write kills it on this path.
        if (is_register_read(n, reg)) return result | LV_LIVE;

        if (n->type == OP_JMP) {
            const char *t = branch_target(n);
            int li = (t && !is_register_operand(t)) ? find_label(t) : -1;
            if (li < 0) return result | LV_LIVE;
            if (g_stamp[li] == stamp) return result;
            n = g_labels[li].node;                       // loop's n->next steps past it
            g_stamp[li] = stamp;
            continue;
        }
        if (n->type == OP_JT || n->type == OP_JF) {
            const char *t = branch_target(n);
            int li = t ? find_label(t) : -1;
            if (li < 0) return result | LV_LIVE;
            if (g_stamp[li] != stamp) {
                g_stamp[li] = stamp;
                result |= flow(g_labels[li].node->next, reg, ridx, depth, stamp);
                if (result & LV_LIVE) return result;
            }
            continue;                                    // fall-through side
        }

        if (modifies_register(n, reg)) return result;
    }
}

bool reg_dead_from(AsmNode *start, const char *reg) {
    if (!g_labels || !start || !reg) return false;
    int ridx = get_reg_index(reg);
    if (ridx < 0 || ridx > 10) return false;
    if (toupper((unsigned char)reg[0]) != 'R') return false;   // no aliases
    g_steps = 0;
    int r = flow(start, reg, ridx, 0, ++g_stamp_id);
    if (r & LV_LIVE) return false;
    if ((r & LV_PASSTHRU) && is_live_out_register(reg)) return false;
    return true;
}

// As reg_dead_from(), starting at a label given by name.
bool reg_dead_at_label(const char *label, const char *reg) {
    int li = find_label(label);
    if (li < 0) return false;
    return reg_dead_from(g_labels[li].node, reg);
}
