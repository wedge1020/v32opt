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
//   - RET              -> live iff the ABI says the register is returned
//                         (is_live_out_register) or some caller of the
//                         function reads it after the call (see "outputs")
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
// OUTPUTS. Which registers a function returns is a convention, and not every
// callee follows the compiler's: v32cc's unsigned-divide helper returns the
// quotient in R0 and the remainder in R1, and a write to R1 just before its
// RET is not dead. For every function that is the target of a direct CALL,
// liveness_begin() finds the registers it writes without saving (no PUSH)
// and asks, at each call site, whether the caller reads them after the call;
// those become live at the function's RET, to a fixed point. Functions that
// are only called indirectly keep the ABI default.
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
static unsigned short *g_extra   = NULL;   // per-label: registers (bit n = Rn) a direct caller reads after the call
static signed char *g_is_fn      = NULL;   // per-label: target of a direct CALL
static unsigned short *g_in      = NULL;   // per-label: registers (bit n = Rn) the function at that label may READ before writing
                                           // (only valid where g_in_ok[label]; computed by compute_inputs, a fixed point
                                           // that, unlike the depth-limited walk, copes with recursion)
static signed char *g_in_ok      = NULL;
static bool g_summary_mode       = false;  // flow() is computing g_in: a CALL asks g_in instead of walking the callee

static int cmp_label(const void *a, const void *b) {
    return strcmp(((const LabelEntry *)a)->name, ((const LabelEntry *)b)->name);
}

void liveness_end(void) {
    free(g_labels); free(g_stamp); free(g_callee); free(g_extra); free(g_is_fn); free(g_in); free(g_in_ok);
    g_labels = NULL; g_stamp = NULL; g_callee = NULL; g_extra = NULL; g_is_fn = NULL; g_in = NULL; g_in_ok = NULL;
    g_label_count = 0;
}

static void compute_outputs(AsmNode *head);
static void compute_inputs(void);

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
        if (strcmp(g_labels[i].name, g_labels[i + 1].name) == 0)
            g_labels[i].node = g_labels[i + 1].node = NULL;
    g_stamp_id = 0;

    g_extra = calloc((size_t)g_label_count + 1, sizeof(unsigned short));
    g_is_fn = calloc((size_t)g_label_count + 1, 1);
    if (!g_extra || !g_is_fn) { liveness_end(); return; }
    compute_outputs(head);
    compute_inputs();
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

// Is Rridx live when `ret` executes: returned by the ABI, or read after a
// call by some direct caller of the function `ret` belongs to?
static bool ret_live_out(const AsmNode *ret, const char *reg, int ridx) {
    if (is_live_out_register(reg)) return true;
    return g_extra && ret->lv_fn > 0 && ((g_extra[ret->lv_fn - 1] >> ridx) & 1u);
}

// Callee summary for (label, reg): LV_LIVE and/or LV_PASSTHRU bits.
static int callee_summary(int lbl, const char *reg, int ridx, int depth) {
    if (g_callee[lbl][ridx] >= 0) return g_callee[lbl][ridx];
    if (depth >= LV_MAX_DEPTH) return LV_LIVE;
    int r = flow(g_labels[lbl].node, reg, ridx, depth + 1, ++g_stamp_id);
    // The walk gives up on recursion and deep call chains (answering LIVE); the fixed point does not. A register the
    // function can never read before writing (calls included) is never read by it: it passes through or is replaced.
    if ((r & LV_LIVE) && g_in_ok && g_in_ok[lbl] && !((g_in[lbl] >> ridx) & 1u))
        r = (r & ~LV_LIVE) | LV_PASSTHRU;
    // Only cache a result that didn't hit the step budget (LIVE from an
    // exhausted budget is query-specific, not a property of the callee).
    if (g_steps < LV_MAX_STEPS) g_callee[lbl][ridx] = (signed char)r;
    return r;
}

// Is `push` (a PUSH of `reg`) the first half of a register SAVE: the stack stays balanced from here up to a
// POP of the same register, which gives it back? Then a callee that starts with it neither reads the caller's value
// nor loses it, and the register reaches the callee's RET as it came in. Only the straight-line text is followed
// (the compiler's prologue / epilogue); any other use of SP, or a PUSH never popped back, is not a save.
static bool is_register_save(const AsmNode *push, const char *reg) {
    if (!push->has_dst || push->dst_op.mode != MODE_REG || !str_case_eq(push->dst_op.reg, reg)) return false;

    // Where the pushed word sits, in words below BP: count back to the frame setup "MOV BP, SP" (the prologue's
    // straight-line text). If it is not found, only the stack balance relative to this PUSH is followed.
    bool have_bp = false;
    long below = 0;                                      // words pushed between MOV BP, SP and this PUSH
    {
        int steps = 0;
        for (const AsmNode *q = push->prev; q && steps < 64; q = q->prev, steps++) {
            if (q->type == OP_OTHER) continue;
            if (q->type == OP_LABEL || q->type == OP_JMP || q->type == OP_JT || q->type == OP_JF || q->type == OP_RET) break;
            if (q->type == OP_MOV && q->has_dst && q->has_src && q->dst_op.mode == MODE_REG && q->src_op.mode == MODE_REG &&
                str_case_eq(q->dst_op.reg, "BP") && str_case_eq(q->src_op.reg, "SP")) { have_bp = true; break; }
            if (q->type == OP_PUSH) { below++; continue; }
            if (q->type == OP_POP) { below--; continue; }
            if ((q->type == OP_IADD || q->type == OP_ISUB) && q->has_dst && q->dst_op.mode == MODE_REG &&
                str_case_eq(q->dst_op.reg, "SP") && q->has_src && q->src_op.mode == MODE_IMMEDIATE && !q->src_op.is_float) {
                below += (q->type == OP_ISUB ? 1 : -1) * q->src_op.immediate;
                continue;
            }
            if ((q->has_dst && q->dst_op.mode == MODE_REG && str_case_eq(q->dst_op.reg, "SP")) ||
                (q->has_src && (q->src_op.mode == MODE_REG || q->src_op.mode == MODE_INDIRECT) && str_case_eq(q->src_op.reg, "SP"))) break;
        }
    }
    long mine = have_bp ? below + 1 : 1;                 // depth (below BP, or below this PUSH) right after it
    long depth = mine;
    int steps = 0;
    for (const AsmNode *q = push->next; q && steps < 600; q = q->next, steps++) {
        if (q->type == OP_OTHER || q->type == OP_LABEL) continue;
        if (q->type == OP_PUSH) { depth++; continue; }
        if (q->type == OP_POP) {
            if (depth == mine) return q->has_dst && q->dst_op.mode == MODE_REG && str_case_eq(q->dst_op.reg, reg);
            depth--;
            if (depth < mine) return false;
            continue;
        }
        bool sp_dst = q->has_dst && q->dst_op.mode == MODE_REG && str_case_eq(q->dst_op.reg, "SP");
        bool sp_src = q->has_src && ((q->src_op.mode == MODE_REG || q->src_op.mode == MODE_INDIRECT) &&
                                     str_case_eq(q->src_op.reg, "SP"));
        if (sp_dst) {
            if ((q->type == OP_IADD || q->type == OP_ISUB) && q->has_src && q->src_op.mode == MODE_IMMEDIATE &&
                !q->src_op.is_float) {
                depth += (q->type == OP_ISUB ? 1 : -1) * q->src_op.immediate;
                if (depth < mine) return false;
                continue;
            }
            if (have_bp && q->type == OP_MOV && q->has_src && q->src_op.mode == MODE_REG &&
                str_case_eq(q->src_op.reg, "BP")) { depth = 0; continue; }      // MOV SP, BP: nothing above BP
            return false;
        }
        if (sp_src) return false;
        if (q->type == OP_RET) return false;
    }
    return false;
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
        if (n->type == OP_RET) {
            // at the top level a RET hands the register to the caller; inside a
            // callee summary it passes through to the call site
            if (depth == 0 && ret_live_out(n, reg, ridx)) return result | LV_LIVE;
            return result | LV_PASSTHRU;
        }

        if (n->type == OP_CALL) {
            const char *t = branch_target(n);
            int li = (t && !is_register_operand(t)) ? find_label(t) : -1;
            if (li < 0) return result | LV_LIVE;
            if (g_summary_mode) {                        // g_in so far: a bit once set stays set
                if (g_in && ((g_in[li] >> ridx) & 1u)) return result | LV_LIVE;
                continue;
            }
            int cs = callee_summary(li, reg, ridx, depth);
            if (cs & LV_LIVE) return result | LV_LIVE;
            if (!(cs & LV_PASSTHRU)) return result;      // overwritten on every path
            continue;                                    // may survive the call
        }

        // inside a callee: "PUSH Rn ... POP Rn" saves and restores the caller's register
        if (depth > 0 && n->type == OP_PUSH && is_register_save(n, reg)) return result | LV_PASSTHRU;

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


// Fixed point of g_in over the functions (call targets and compiler "__function_" entries): a register is in g_in[f] when
// some path from f's entry may read it before writing it. A call reads what its callee reads (nothing is assumed killed).
// A register save (PUSH Rn ... POP Rn) is not a read (see is_register_save).
static void compute_inputs(void) {
    int nl = g_label_count;
    g_in    = calloc((size_t)nl + 1, sizeof(unsigned short));
    g_in_ok = calloc((size_t)nl + 1, 1);
    if (!g_in || !g_in_ok) { free(g_in); free(g_in_ok); g_in = NULL; g_in_ok = NULL; return; }
    bool *is_entry = calloc((size_t)nl + 1, 1);
    if (!is_entry) { free(g_in); free(g_in_ok); g_in = NULL; g_in_ok = NULL; return; }
    for (int i = 0; i < nl; i++) {
        if (!g_labels[i].node) continue;
        size_t ln = strlen(g_labels[i].name);
        bool fn = g_is_fn[i] || (strncmp(g_labels[i].name, "__function_", 11) == 0 &&
                                 !(ln > 7 && strcmp(g_labels[i].name + ln - 7, "_return") == 0));
        is_entry[i] = fn;
        g_in_ok[i] = fn;
    }
    g_summary_mode = true;
    for (int iter = 0; iter < 30; iter++) {
        bool changed = false;
        for (int i = 0; i < nl; i++) {
            if (!is_entry[i]) continue;
            for (int r = 0; r <= 10; r++) {
                if ((g_in[i] >> r) & 1u) continue;
                char nm[8];
                snprintf(nm, sizeof(nm), "R%d", r);
                g_steps = 0;
                int res = flow(g_labels[i].node, nm, r, 1, ++g_stamp_id);   // depth 1: a RET is a pass-through
                if (res & LV_LIVE) { g_in[i] |= (unsigned short)(1u << r); changed = true; }
            }
        }
        if (!changed) break;
        if (iter == 29) for (int i = 0; i < nl; i++) g_in_ok[i] = 0;        // did not settle: trust nothing
    }
    g_summary_mode = false;
    free(is_entry);
}

// ===================================================================
// OUTPUTS: registers a function leaves for its direct callers to read
// ===================================================================
typedef struct { AsmNode *call; int target; } CallSite;

#define LV_MAX_OUTPUT_QUERIES 400000

static void compute_outputs(AsmNode *head) {
    int nl = g_label_count;
    int ncalls = 0;
    for (AsmNode *n = head; n; n = n->next) if (n->type == OP_CALL) ncalls++;

    CallSite *sites = malloc((size_t)(ncalls ? ncalls : 1) * sizeof(CallSite));
    unsigned short *written = calloc((size_t)nl + 1, sizeof(unsigned short));
    unsigned short *pushed  = calloc((size_t)nl + 1, sizeof(unsigned short));
    if (!sites || !written || !pushed) {
        free(sites); free(written); free(pushed);
        for (AsmNode *n = head; n; n = n->next) n->lv_fn = 0;
        return;
    }

    // 1. direct CALL targets
    int ns = 0;
    for (AsmNode *n = head; n; n = n->next) {
        if (n->type != OP_CALL) continue;
        const char *t = branch_target(n);
        int li = (t && !is_register_operand(t)) ? find_label(t) : -1;
        if (li < 0) continue;
        g_is_fn[li] = 1;
        sites[ns].call = n; sites[ns].target = li; ns++;
    }

    // 2. which function each node belongs to (from one call-target label to the
    // next), and which registers each function writes / pushes
    int cur = 0;
    for (AsmNode *n = head; n; n = n->next) {
        if (n->type == OP_LABEL) {
            int li = label_index_of(n);
            if (li >= 0 && g_is_fn[li]) cur = li + 1;
        }
        n->lv_fn = cur;
        if (cur == 0 || !n->has_dst || n->dst_op.mode != MODE_REG) continue;
        const char *rn = n->dst_op.reg;
        int r = get_reg_index(rn);
        if (r < 1 || r > 10 || (rn[0] != 'R' && rn[0] != 'r')) continue;
        if (n->type == OP_PUSH) pushed[cur - 1] |= (unsigned short)(1u << r);
        else if (n->type != OP_JMP && n->type != OP_JT && n->type != OP_JF && n->type != OP_CALL)
            written[cur - 1] |= (unsigned short)(1u << r);
    }

    // registers the ABI already returns need no help
    unsigned short abi = 0;
    for (int r = 1; r <= 10; r++) {
        char nm[8]; snprintf(nm, sizeof(nm), "R%d", r);
        if (is_live_out_register(nm)) abi |= (unsigned short)(1u << r);
    }

    // 3. fixed point: a register a function writes without saving is an output
    // if some direct caller reads it after the call
    long queries = 0;
    bool changed = true, gave_up = false;
    for (int round = 0; changed && round < 8; round++) {
        changed = false;
        memset(g_callee, -1, (size_t)nl * sizeof(*g_callee));
        for (int i = 0; i < ns && !gave_up; i++) {
            int t = sites[i].target;
            unsigned short cand = (unsigned short)(written[t] & ~pushed[t] & ~abi & ~g_extra[t]);
            for (int r = 1; r <= 10 && cand; r++) {
                if (!((cand >> r) & 1u)) continue;
                char nm[8]; snprintf(nm, sizeof(nm), "R%d", r);
                if (++queries > LV_MAX_OUTPUT_QUERIES) { gave_up = true; break; }
                if (!sites[i].call->next || !reg_dead_from(sites[i].call->next, nm)) {
                    g_extra[t] |= (unsigned short)(1u << r);
                    changed = true;
                }
            }
        }
        if (gave_up) break;
    }
    if (gave_up || changed) {                      // no fixed point: assume the worst
        for (int t = 0; t < nl; t++) g_extra[t] |= (unsigned short)(written[t] & ~pushed[t] & ~abi);
    }
    memset(g_callee, -1, (size_t)nl * sizeof(*g_callee));
    free(sites); free(written); free(pushed);
}
