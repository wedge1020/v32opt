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
//                         (is_live_out_register), or the function the RET
//                         belongs to is "open", or some direct caller of it
//                         reads the register after the call (see "outputs")
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
// ENTRIES. A label is a function entry if it is the target of a direct
// CALL, or a compiler "__function_" label (not one ending in "_return"),
// or it ESCAPES: its name appears in an operand of an instruction other
// than CALL/JMP/JT/JF, or in a directive (data, %define). An escaped
// label may be called or jumped to through a register from anywhere.
//
// CLASSES. Every node belongs to the region that starts at the nearest
// entry label above it (region 0 is the code before the first entry).
// Regions that control can pass between without a CALL are merged into
// one class: a JMP/JT/JF whose target label lies in another region, or
// falling through into an entry label (the previous instruction is not a
// JMP, RET, HLT or data directive). So a tail jump, a shared epilogue or
// code that skips over a nested function puts both sides in one class,
// and a RET belongs to every function that can reach it. A passage into
// code from which no RET can be reached (an error handler that halts, an
// exit through a computed JMP) does not merge: no RET there returns to
// the source's callers (reaches_ret). node->lv_fn is 1 + the class; a
// node made after liveness_begin() has lv_fn 0.
//
// OPEN CLASSES. A class is open when control can come into it from an
// unknown place and reach a RET: from the program start (region 0), through
// an escaped entry, or through entries none of which is the target of a
// direct CALL. Its callers are not all known, so at its RETs every register
// R0..R10 is live; also at a RET with lv_fn 0. Compiler-ABI exemption
// (class_trusts_abi): an open class whose every unknown way in is a
// "__function_" entry keeps the ABI (plus outputs) instead, since compiled
// functions follow the compiler's convention for indirect callers too -- but
// only where a compiler vouches for that convention: the input is v32lua
// output (-L lua) or carries a ';@hints' header. Hand-written assembly
// without a header keeps the strict rule (a hand-written function reached
// through "CALL Rn" may well return a second value in R1). It matters for
// v32lua, where every function is reached through __builtin_exec / "JMP R0":
// all strict, no register is ever free for the promote passes. Measured on
// primes (corpus/primes, -O2 -L lua): promote-loops promotions 76 before the
// liveness fix (1e7f1a3), 0 with the strict rule, 70 (plus 72 promote-regs)
// with this one; 40 genlua seeds at -O2: 33,247,006 / 33,552,248 /
// 33,338,487 cycles. (With a header whose promise is wrong for a function --
// a second result in R1 to an indirect caller -- that write can be deleted.)
//
// OUTPUTS. Which registers a function returns is a convention, and not every
// callee follows the compiler's: v32cc's unsigned-divide helper returns the
// quotient in R0 and the remainder in R1, and a write to R1 just before its
// RET is not dead. For every class, liveness_begin() finds the registers it
// writes outside a save (below), itself or through a direct callee, and asks
// at each direct call site whether the caller reads them after the call;
// those become live at the class's RETs, to a fixed point.
//
// THE SAVE RULE. "PUSH Rn ... POP Rn" with the stack balanced in between
// (is_register_save, the compiler's prologue / epilogue) gives the caller's
// Rn back, so inside a callee the PUSH is not a read of Rn and the register
// passes through to the RET; the walk still follows the code between the
// PUSH and the POP, where a read of the incoming Rn makes it live. It is not
// a save when the pushed word can be a stack argument: a CALL made while the
// word is within the reach of the callee's stack arguments, or to an unknown
// callee. The reach of a call target is that of all the code it runs on its
// frame, jumps followed (reach_walk): [BP+k] reads word k-2 from the top,
// a read through SP or a computed JMP may read any. Writes between a save's
// PUSH and POP are not outputs, if no jump leaves that range and no entry
// label lies inside it.
//
// The label index is built once by liveness_begin() and must be rebuilt
// if labels are added or removed. Removing or rewriting ordinary
// instructions between queries is fine: every query walks the live list.
// The class summaries are computed once per session; they stay valid as
// long as a pass removes a register write in a callee only when
// reg_dead_from() proved it dead, and adds no read of a register value a
// function receives from its callers. (A pass that ADDS a write of a
// register to a function relies on the ABI: only registers a class writes
// at liveness_begin() are output candidates, so one a caller keeps across
// the call is not seen as live at the RET.)
// ===================================================================

#define LV_LIVE      1   // may be read before being written
#define LV_PASSTHRU  2   // reached a RET with the register untouched

#define LV_MAX_STEPS 20000
#define LV_MAX_DEPTH 8

// Compiler-ABI exemption for open classes (see OPEN CLASSES above). 1 (default): the run-time rule in
// class_trusts_abi(); 0 (build with -DLV_TRUST_COMPILER_ABI=0, for testing): never, every open RET all-live.
#ifndef LV_TRUST_COMPILER_ABI
#define LV_TRUST_COMPILER_ABI 1
#endif

#define LV_REACH_ALL 0x7fffffff  // code that may read any word of its caller's stack

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
static signed char *g_is_entry   = NULL;   // per-label: function entry (see ENTRIES)
static int        *g_lbl_cls     = NULL;   // per-label: class of the region the label sits in
static int         g_ncls        = 0;      // number of regions; a class is named by its representative region
static unsigned short *g_extra   = NULL;   // per-class: registers (bit n = Rn) a direct caller reads after the call
static signed char *g_open       = NULL;   // per-class: callers not all known (see OPEN CLASSES)
static signed char *g_trust      = NULL;   // per-class: every way in from an unknown place is a "__function_" entry
static int        *g_reach       = NULL;   // per-label (call targets): words of the caller's stack (from the top, at the CALL)
                                           // it may read as arguments; LV_REACH_ALL if unbounded
static unsigned short *g_in      = NULL;   // per-label: registers (bit n = Rn) the function at that label may READ before writing
                                           // (only valid where g_in_ok[label]; computed by compute_inputs, a fixed point
                                           // that, unlike the depth-limited walk, copes with recursion)
static signed char *g_in_ok      = NULL;
static bool g_summary_mode       = false;  // flow() is computing g_in: a CALL asks g_in instead of walking the callee

static int cmp_label(const void *a, const void *b) {
    return strcmp(((const LabelEntry *)a)->name, ((const LabelEntry *)b)->name);
}

// ---------------------------------------------------------------------------
// Session reuse. liveness_begin() costs several walks of the whole program; the fixed-point loop
// calls it from six passes per iteration, and on a large input most of them see exactly the
// program the previous one analysed (it found nothing to change). The begin-time tables are a
// function of the program alone, so when nothing was edited since the last begin
// (g_program_edits, counted by node_set_raw*/create_node/clone_node/remove_node) the tables are
// kept, and the per-query state -- the callee cache and the visit stamps -- is put back exactly as
// begin left it. The answers are the ones a fresh session gives. The counter is only the fast first
// test: a reuse also needs the program's fingerprint (lv_fp: every node's type, hints, operand modes
// and texts) to equal the one taken at the build, so an edit some future pass makes without the
// counted helpers still gets a rebuild (and a diagnostic under -v / -d). Costs about 7% of the run
// time on a 160K-line input, still far less than the rebuilds it saves. V32OPT_LV_NOREUSE=1 turns
// reuse off (a test aid).
// ---------------------------------------------------------------------------
static bool          g_active       = false;  // between liveness_begin() and liveness_end()
static bool          g_kept         = false;  // the tables below are still those of g_kept_head
static AsmNode      *g_kept_head    = NULL;
static unsigned long g_kept_edits   = 0;
static signed char (*g_callee0)[11] = NULL;   // g_callee as begin left it
static int          *g_stamp0       = NULL;
static int           g_stamp_id0    = 0;

static void liveness_free(void) {
    free(g_callee0); free(g_stamp0);
    g_callee0 = NULL; g_stamp0 = NULL;
    g_kept = false;
    free(g_labels); free(g_stamp); free(g_callee); free(g_extra); free(g_is_entry); free(g_lbl_cls);
    free(g_open); free(g_trust); free(g_reach); free(g_in); free(g_in_ok);
    g_labels = NULL; g_stamp = NULL; g_callee = NULL; g_extra = NULL; g_is_entry = NULL; g_lbl_cls = NULL;
    g_open = NULL; g_trust = NULL; g_reach = NULL; g_in = NULL; g_in_ok = NULL;
    g_label_count = 0; g_ncls = 0;
}

void liveness_end(void) {
    g_active = false;                       // tables kept for a possible reuse (see above)
}

static bool find_entries(AsmNode *head);
static bool build_classes(AsmNode *head);
static void compute_outputs(AsmNode *head);
static void compute_inputs(void);

static void liveness_build(AsmNode *head);
static unsigned long long lv_fp(AsmNode *head) {
    unsigned long long h = 1469598103934665603ULL;
    for (AsmNode *n = head; n; n = n->next) {
        h = (h ^ (unsigned)n->type) * 1099511628211ULL;
        h = (h ^ (unsigned)n->hints) * 1099511628211ULL;
        for (const char *c = n->raw; c && *c; c++) h = (h ^ (unsigned char)*c) * 1099511628211ULL;
        for (const char *c = n->dst_op.raw; *c; c++) h = (h ^ (unsigned char)*c) * 1099511628211ULL;
        for (const char *c = n->src_op.raw; *c; c++) h = (h ^ (unsigned char)*c) * 1099511628211ULL;
        h = (h ^ (unsigned)n->dst_op.mode ^ ((unsigned)n->src_op.mode << 4) ^ ((unsigned)n->has_dst << 8) ^ ((unsigned)n->has_src << 9)) * 1099511628211ULL;
    }
    return h;
}
static unsigned long long g_kept_fp = 0;

void liveness_begin(AsmNode *head) {
    static int noreuse = -1;
    if (noreuse < 0) noreuse = getenv("V32OPT_LV_NOREUSE") != NULL;
    bool reuse = !noreuse && g_kept && head == g_kept_head && g_program_edits == g_kept_edits;
    if (reuse && lv_fp(head) != g_kept_fp) {
        // the program changed without going through the counted helpers: never trust the tables
        if (config.verbose || config.debug)
            fprintf(stderr, "v32opt: liveness: an edit the edit count missed was detected (tables rebuilt) -- please report this\n");
        reuse = false;
    }
    if (reuse) {
        if (g_labels) {
            memcpy(g_callee, g_callee0, (size_t)g_label_count * sizeof(*g_callee));
            memcpy(g_stamp, g_stamp0, (size_t)g_label_count * sizeof(int));
        }
        g_stamp_id = g_stamp_id0;
        g_steps = 0;
        g_active = true;
        return;
    }
    liveness_free();
    g_active = true;                        // compute_outputs() asks reg_dead_from() while building
    liveness_build(head);
    g_kept = true;
    g_kept_head = head;
    g_kept_edits = g_program_edits;
    g_kept_fp = lv_fp(head);
    if (g_labels && g_label_count > 0) {
        g_callee0 = malloc((size_t)g_label_count * sizeof(*g_callee));
        g_stamp0  = malloc((size_t)g_label_count * sizeof(int));
        if (!g_callee0 || !g_stamp0) { free(g_callee0); free(g_stamp0); g_callee0 = NULL; g_stamp0 = NULL; g_kept = false; }
        else {
            memcpy(g_callee0, g_callee, (size_t)g_label_count * sizeof(*g_callee));
            memcpy(g_stamp0, g_stamp, (size_t)g_label_count * sizeof(int));
        }
    }
    g_stamp_id0 = g_stamp_id;
    g_active = true;
}

static void liveness_build(AsmNode *head) {
    int cap = 0;
    for (AsmNode *n = head; n; n = n->next)
        if (n->type == OP_LABEL) cap++;
    if (cap == 0) return;
    g_labels = calloc((size_t)cap, sizeof(LabelEntry));
    g_stamp  = calloc((size_t)cap, sizeof(int));
    g_callee = malloc((size_t)cap * sizeof(*g_callee));
    if (!g_labels || !g_stamp || !g_callee) { liveness_free(); return; }
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

    // Any failure below drops the whole session (g_labels NULL): every query then answers LIVE.
    if (!find_entries(head) || !build_classes(head)) { liveness_free(); return; }
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

// Label index of a JMP/JT/JF/CALL's target, or -1 (register, unknown or ambiguous label).
static int target_label(const AsmNode *n) {
    const char *t = branch_target(n);
    return (t && !is_register_operand(t)) ? find_label(t) : -1;
}

static bool is_function_label_name(const char *nm) {
    size_t ln = strlen(nm);
    return strncmp(nm, "__function_", 11) == 0 && !(ln > 7 && strcmp(nm + ln - 7, "_return") == 0);
}

// The compiler-ABI exemption: may an open class keep "ABI + outputs" at its RETs instead of all-live?
static bool class_trusts_abi(int cls) {
#if LV_TRUST_COMPILER_ABI
    if (!g_trust || !g_trust[cls]) return false;
    // Only where a compiler vouches for the convention: v32lua output (-L lua), or any input with a
    // ';@hints' header. Hand-written assembly without one keeps the strict all-live rule.
    return config.lang_mode == LANG_LUA || hints_header_seen();
#else
    (void)cls;
    return false;
#endif
}

static int flow(AsmNode *node, const char *reg, int ridx, int depth, int stamp);

// Is Rridx live when `ret` executes: returned by the ABI, an open class's RET,
// or read after a call by some direct caller of the class `ret` belongs to?
static bool ret_live_out(const AsmNode *ret, const char *reg, int ridx) {
    if (is_live_out_register(reg)) return true;
    if (!g_extra || ret->lv_fn <= 0 || ret->lv_fn > g_ncls) return true;   // made after liveness_begin(): unknown
    int c = ret->lv_fn - 1;
    if (g_open[c] && !class_trusts_abi(c)) return true;
    return (g_extra[c] >> ridx) & 1u;
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
// POP of the same register, which gives it back? Then a callee that starts with it does not lose the caller's
// value, and the register reaches the callee's RET as it came in. Only the straight-line text is followed
// (the compiler's prologue / epilogue); any other use of SP, or a PUSH never popped back, is not a save.
// Nor is it one when the pushed word may be read as a stack argument: a CALL made while the word lies within
// the callee's reach (g_reach), or to a callee that is not a known label. On success *pop_out (if given) is
// the matching POP.
static bool is_register_save(const AsmNode *push, const char *reg, const AsmNode **pop_out) {
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
            if (depth == mine) {
                bool ok = q->has_dst && q->dst_op.mode == MODE_REG && str_case_eq(q->dst_op.reg, reg);
                if (ok && pop_out) *pop_out = q;
                return ok;
            }
            depth--;
            if (depth < mine) return false;
            continue;
        }
        if (q->type == OP_CALL) {
            // The callee sees the word (depth - mine) words below the top of the stack: [BP + 2 + (depth - mine)]
            // after its "PUSH BP / MOV BP, SP".
            if (depth >= mine) {
                int li = target_label(q);
                if (li < 0 || !g_reach) return false;
                if (depth - mine < (long)g_reach[li]) return false;
            }
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
            int li = target_label(n);
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

        // inside a callee: "PUSH Rn ... POP Rn" saves and restores the caller's register, so the register
        // passes through; but the code up to the POP may still read the caller's value (a register argument)
        if (depth > 0 && n->type == OP_PUSH && is_register_save(n, reg, NULL)) {
            int sub = flow(n->next, reg, ridx, depth, stamp);
            return result | (sub & LV_LIVE) | LV_PASSTHRU;
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
            int li = target_label(n);
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
    if (!g_active || !g_labels || !start || !reg) return false;
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

// ===================================================================
// ENTRIES AND CLASSES
// ===================================================================

// Mark every label named in `s` (an operand or a directive line) as escaped. String literals and the
// comment are skipped; a token that starts with a digit is a number.
static void escape_tokens(const char *s, signed char *escaped) {
    char tok[128];
    while (*s) {
        if (*s == ';') return;
        if (*s == '"') {
            s++;
            while (*s && *s != '"') { if (*s == '\\' && s[1]) s++; s++; }
            if (*s) s++;
            continue;
        }
        if (isalpha((unsigned char)*s) || *s == '_') {
            size_t k = 0;
            while (isalnum((unsigned char)*s) || *s == '_') { if (k + 1 < sizeof(tok)) tok[k++] = *s; s++; }
            tok[k] = '\0';
            int li = find_label(tok);
            if (li >= 0) escaped[li] = 1;
            continue;
        }
        if (isdigit((unsigned char)*s)) {
            while (isalnum((unsigned char)*s) || *s == '_' || *s == '.') s++;
            continue;
        }
        s++;
    }
}

// g_is_entry[label]: nonzero for a function entry, as flags: 1 = escaped, 2 = direct CALL target,
// 4 = "__function_" label (a label that is only 4 is an entry too; build_classes reads the flags).
static bool find_entries(AsmNode *head) {
    int nl = g_label_count;
    g_is_entry = calloc((size_t)nl + 1, 1);
    if (!g_is_entry) return false;
    signed char *esc = calloc((size_t)nl + 1, 1);
    if (!esc) return false;
    for (AsmNode *n = head; n; n = n->next) {
        if (n->type == OP_LABEL) continue;
        if (n->type == OP_OTHER) {
            if (is_directive_node(n)) escape_tokens(n->raw, esc);
            continue;
        }
        if (n->type == OP_CALL) {
            int li = target_label(n);
            if (li >= 0) g_is_entry[li] |= 2;
            continue;
        }
        if (n->type == OP_JMP || n->type == OP_JT || n->type == OP_JF) continue;
        if (n->has_dst) escape_tokens(n->dst_op.raw, esc);
        if (n->has_src) escape_tokens(n->src_op.raw, esc);
    }
    for (int i = 0; i < nl; i++) {
        if (!g_labels[i].node) continue;
        if (esc[i]) g_is_entry[i] |= 1;
        if (is_function_label_name(g_labels[i].name)) g_is_entry[i] |= 4;
    }
    free(esc);
    return true;
}

static int uf_find(int *parent, int x) {
    while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
    return x;
}

static void uf_union(int *parent, int a, int b) {
    a = uf_find(parent, a); b = uf_find(parent, b);
    if (a == b) return;
    if (a < b) parent[b] = a; else parent[a] = b;            // region 0 stays its class's name
}

// A line that emits data words: control cannot meaningfully fall through it.
static bool is_data_line(const AsmNode *n) {
    if (!is_data_directive_node(n)) return false;
    const char *p = n->raw;
    while (*p == ' ' || *p == '\t') p++;
    static const char *const kw[] = { "integer", "float", "string", "pointer", "datafile" };
    for (size_t i = 0; i < sizeof(kw) / sizeof(kw[0]); i++) {
        size_t l = strlen(kw[i]);
        if (strncasecmp(p, kw[i], l) == 0 && (p[l] == '\0' || isspace((unsigned char)p[l]))) return true;
    }
    return false;
}

// Can control reach the label `lab` by falling through from the code above it?
static bool falls_into(const AsmNode *lab) {
    for (const AsmNode *q = lab->prev; q; q = q->prev) {
        if (q->type == OP_LABEL) return true;            // a label above may be jumped to
        if (q->type == OP_OTHER) {
            if (!is_directive_node(q) || is_preprocessor_node(q)) continue;
            return !is_data_line(q);                     // an unknown instruction falls through
        }
        return q->type != OP_JMP && q->type != OP_RET && q->type != OP_HLT;
    }
    return false;
}

// Can control entering at `node` reach a RET without returning through a CALL it makes? (Followed: fall-through,
// JMP/JT/JF to labels, over CALLs. A computed JMP leaves for a label that escapes, and is checked on its own, or
// for a return address, which is a return to a known caller; both end the path. Unknowns answer true.)
// Stack words above the return address (counted from the top at the CALL) that `n` may read as arguments:
// [BP+k] reads word k-2 (after the usual "PUSH BP / MOV BP, SP"); a read through SP may read any, and so may
// the unknown code a computed JMP goes to (it runs on the same stack: a Lua trampoline jumps to the function).
static int node_reach(const AsmNode *n) {
    int reach = 0;
    if (n->type == OP_OTHER || n->type == OP_LABEL) return 0;
    if (n->type == OP_JMP) {
        const char *t = branch_target(n);
        return (!t || is_register_operand(t)) ? LV_REACH_ALL : 0;
    }
    for (int side = 0; side < 2; side++) {
        const Operand *op = side ? &n->src_op : &n->dst_op;
        if (!(side ? n->has_src : n->has_dst) || op->mode != MODE_INDIRECT) continue;
        int r = get_reg_index(op->reg);
        if (r == 14 && op->offset >= 2 && op->offset - 1 > reach) reach = op->offset - 1;
        if (r == 15 && (side == 1 || n->type != OP_MOV)) return LV_REACH_ALL;
    }
    return reach;
}

// Walk state of reaches_ret / reach_walk: step budget, and the largest node_reach() of the code reach_walk went through.
static int g_rr_steps;
static int g_rr_reach;
static bool reaches_ret(AsmNode *node, int stamp) {
    for (AsmNode *n = node; ; n = n->next) {
        if (!n || ++g_rr_steps > LV_MAX_STEPS) return true;
        switch (n->type) {
        case OP_LABEL: {
            int li = label_index_of(n);
            if (li < 0) return true;
            if (g_stamp[li] == stamp) return false;
            g_stamp[li] = stamp;
            continue;
        }
        case OP_OTHER: if (is_data_directive_node(n)) return true; continue;
        case OP_RET:   return true;
        case OP_HLT:   return false;
        case OP_JMP: {
            const char *t = branch_target(n);
            if (!t || is_register_operand(t)) return false;
            int li = find_label(t);
            if (li < 0) return true;
            if (g_stamp[li] == stamp) return false;
            g_stamp[li] = stamp;
            n = g_labels[li].node;
            continue;
        }
        case OP_JT: case OP_JF: {
            int li = target_label(n);
            if (li < 0) return true;
            if (g_stamp[li] != stamp) {
                g_stamp[li] = stamp;
                if (reaches_ret(g_labels[li].node->next, stamp)) return true;
            }
            continue;
        }
        default: continue;
        }
    }
}

// The stack reach (node_reach) of all the code that runs from `node` on the same stack frame: fall-through,
// JMP/JT/JF, over CALLs, up to RET / HLT. Unknowns (data, the end of the file, the step budget) reach all.
static void reach_walk(AsmNode *node, int stamp) {
    for (AsmNode *n = node; g_rr_reach < LV_REACH_ALL; n = n->next) {
        if (!n || ++g_rr_steps > LV_MAX_STEPS) { g_rr_reach = LV_REACH_ALL; return; }
        int r = node_reach(n);
        if (r > g_rr_reach) g_rr_reach = r;
        switch (n->type) {
        case OP_LABEL: {
            int li = label_index_of(n);
            if (li < 0) { g_rr_reach = LV_REACH_ALL; return; }
            if (g_stamp[li] == stamp) return;
            g_stamp[li] = stamp;
            continue;
        }
        case OP_OTHER: if (is_data_directive_node(n)) { g_rr_reach = LV_REACH_ALL; return; } continue;
        case OP_RET: case OP_HLT: return;
        case OP_JMP: case OP_JT: case OP_JF: {
            const char *t = branch_target(n);
            if (n->type == OP_JMP && (!t || is_register_operand(t))) return;   // node_reach said all
            int li = t ? find_label(t) : -1;
            if (li < 0) { g_rr_reach = LV_REACH_ALL; return; }
            if (g_stamp[li] == stamp) { if (n->type == OP_JMP) return; continue; }
            g_stamp[li] = stamp;
            if (n->type == OP_JMP) { n = g_labels[li].node; continue; }
            reach_walk(g_labels[li].node->next, stamp);
            continue;
        }
        default: continue;
        }
    }
}

// Regions, classes (lv_fn, g_lbl_cls), the per-class facts g_open, g_trust, and g_reach; allocates g_extra.
static bool build_classes(AsmNode *head) {
    int nl = g_label_count;
    int nreg = 1;
    for (AsmNode *n = head; n; n = n->next)
        if (n->type == OP_LABEL) { int li = label_index_of(n); if (li >= 0 && g_is_entry[li]) nreg++; }
    int *parent  = malloc((size_t)nreg * sizeof(int));
    g_lbl_cls    = malloc(((size_t)nl + 1) * sizeof(int));
    g_extra      = calloc((size_t)nreg, sizeof(unsigned short));
    g_open       = calloc((size_t)nreg, 1);
    g_trust      = calloc((size_t)nreg, 1);
    g_reach      = calloc((size_t)nl + 1, sizeof(int));
    signed char *bad = calloc((size_t)nreg, 1);        // has an entry that is not a "__function_" label
    signed char *called = calloc((size_t)nreg, 1);     // has an entry with a direct CALL site
    signed char *has = calloc((size_t)nreg, 1);        // has an entry
    if (!parent || !g_lbl_cls || !g_extra || !g_open || !g_trust || !g_reach || !bad || !called || !has) {
        free(parent); free(bad); free(called); free(has);
        return false;
    }
    g_ncls = nreg;
    for (int i = 0; i < nreg; i++) parent[i] = i;
    for (int i = 0; i <= nl; i++) g_lbl_cls[i] = 0;

    // 1. regions (lv_fn holds 1 + region for now)
    int cur = 0;
    for (AsmNode *n = head; n; n = n->next) {
        if (n->type == OP_LABEL) {
            int li = label_index_of(n);
            if (li >= 0 && g_is_entry[li]) cur++;
            if (li >= 0) g_lbl_cls[li] = cur;
        }
        n->lv_fn = cur + 1;
    }

    // 2. merge regions that control passes between without a CALL. A passage into code that cannot reach a RET
    // (an error handler that halts, a shared exit through a computed JMP) does not merge: no RET of the target
    // returns to the source's callers. (Its stack reads still count for g_reach: reach_walk follows jumps.)
    signed char *rr = malloc((size_t)nl + 1);           // per-label memo of reaches_ret (-1 unknown)
    if (!rr) {
        free(parent); free(bad); free(called); free(has);
        return false;
    }
    memset(rr, -1, (size_t)nl + 1);
    for (AsmNode *n = head; n; n = n->next) {
        int from = -1, to = -1, li = -1;
        if (n->type == OP_JMP || n->type == OP_JT || n->type == OP_JF) {
            li = target_label(n);
            if (li >= 0) { from = n->lv_fn - 1; to = g_lbl_cls[li]; }
        } else if (n->type == OP_LABEL && n->prev) {
            li = label_index_of(n);
            if (li >= 0 && g_is_entry[li] && falls_into(n)) { from = n->prev->lv_fn - 1; to = n->lv_fn - 1; }
        }
        if (from < 0 || from == to) continue;
        if (rr[li] < 0) { g_rr_steps = 0; rr[li] = reaches_ret(g_labels[li].node, ++g_stamp_id); }
        if (rr[li]) uf_union(parent, from, to);
    }
    free(rr);
    for (AsmNode *n = head; n; n = n->next) n->lv_fn = uf_find(parent, n->lv_fn - 1) + 1;
    for (int i = 0; i < nl; i++) g_lbl_cls[i] = uf_find(parent, g_lbl_cls[i]);

    // 3. per-class facts. A class is open when control can enter it from an unknown place and reach a RET:
    // from the program start (region 0), through an escaped entry, or through entries none of which has a
    // direct CALL site. g_trust: every such way in is a "__function_" entry (see class_trusts_abi).
    for (int c = 0; c < nreg; c++) g_trust[c] = 1;
    g_rr_steps = 0;
    if (reaches_ret(head, ++g_stamp_id)) { int c0 = uf_find(parent, 0); g_open[c0] = 1; g_trust[c0] = 0; }
    for (int i = 0; i < nl; i++) {
        if (!g_labels[i].node || !g_is_entry[i]) continue;
        int c = g_lbl_cls[i];
        bool fn = (g_is_entry[i] & 4) != 0;
        if (g_is_entry[i] & 2) called[c] = 1;
        if (!fn) bad[c] = 1;
        has[c] = 1;
        if (g_is_entry[i] & 1) {                       // escaped: opens its class if it leads to a RET
            g_rr_steps = 0;
            if (reaches_ret(g_labels[i].node, ++g_stamp_id)) { g_open[c] = 1; if (!fn) g_trust[c] = 0; }
        }
    }
    for (int c = 0; c < nreg; c++) {
        if (parent[c] != c || called[c]) continue;
        g_open[c] = 1;
        if (bad[c] || !has[c]) g_trust[c] = 0;
    }
    // stack arguments of each call target: what the code it runs on its frame may read (see is_register_save)
    for (int i = 0; i < nl; i++) {
        if (!g_labels[i].node || !(g_is_entry[i] & 2)) continue;
        g_rr_steps = 0; g_rr_reach = 0;
        reach_walk(g_labels[i].node, ++g_stamp_id);
        g_reach[i] = g_rr_reach;
    }
    free(parent); free(bad); free(called); free(has);
    return true;
}

// Fixed point of g_in over the function entries: a register is in g_in[f] when some path from f's entry may
// read it before writing it. A call reads what its callee reads (nothing is assumed killed). A register save
// (PUSH Rn ... POP Rn) is not a read (see is_register_save), but what lies between them is walked.
static void compute_inputs(void) {
    int nl = g_label_count;
    g_in    = calloc((size_t)nl + 1, sizeof(unsigned short));
    g_in_ok = calloc((size_t)nl + 1, 1);
    if (!g_in || !g_in_ok) { free(g_in); free(g_in_ok); g_in = NULL; g_in_ok = NULL; return; }
    for (int i = 0; i < nl; i++) g_in_ok[i] = g_labels[i].node && g_is_entry[i];
    g_summary_mode = true;
    for (int iter = 0; iter < 30; iter++) {
        bool changed = false;
        for (int i = 0; i < nl; i++) {
            if (!g_in_ok[i]) continue;
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
}

// ===================================================================
// OUTPUTS: registers a class leaves for its direct callers to read
// ===================================================================
typedef struct { AsmNode *call; int target; int from; unsigned short covered; } CallSite;

#define LV_MAX_OUTPUT_QUERIES 400000
#define LV_MAX_SAVE_LABELS    64

// Does a save's PUSH..POP range keep its writes in: no entry label inside, and every jump in it
// lands on a label inside it?
static bool save_range_closed(const AsmNode *push, const AsmNode *pop) {
    int labels[LV_MAX_SAVE_LABELS];
    int nlab = 0;
    for (const AsmNode *q = push->next; q != pop; q = q->next) {
        if (!q) return false;
        if (q->type != OP_LABEL) continue;
        int li = label_index_of((AsmNode *)q);
        if (li < 0 || g_is_entry[li] || nlab == LV_MAX_SAVE_LABELS) return false;
        labels[nlab++] = li;
    }
    for (const AsmNode *q = push->next; q != pop; q = q->next) {
        if (q->type != OP_JMP && q->type != OP_JT && q->type != OP_JF) continue;
        int li = target_label(q);
        if (li < 0) return false;
        bool inside = false;
        for (int k = 0; k < nlab && !inside; k++) inside = labels[k] == li;
        if (!inside) return false;
    }
    return true;
}

static void compute_outputs(AsmNode *head) {
    int nc = g_ncls;
    int ncalls = 0;
    for (AsmNode *n = head; n; n = n->next) if (n->type == OP_CALL) ncalls++;

    CallSite *sites = malloc((size_t)(ncalls ? ncalls : 1) * sizeof(CallSite));
    unsigned short *written = calloc((size_t)nc + 1, sizeof(unsigned short));
    if (!sites || !written) {
        free(sites); free(written);
        for (int c = 0; c < nc; c++) g_open[c] = 1, g_trust[c] = 0;   // no outputs known: every RET all-live
        return;
    }

    // 1. direct call sites, and the registers each class writes itself outside a save
    int ns = 0;
    const AsmNode *cover[11] = { 0 };                   // cover[r]: the POP that ends the save of Rr we are in
    for (AsmNode *n = head; n; n = n->next) {
        if (n->type == OP_OTHER || n->type == OP_LABEL) continue;
        int c = n->lv_fn - 1;
        unsigned short covered = 0;
        for (int r = 1; r <= 10; r++) if (cover[r]) covered |= (unsigned short)(1u << r);
        if (n->type == OP_CALL) {
            int li = target_label(n);
            if (li < 0) continue;
            sites[ns].call = n; sites[ns].target = g_lbl_cls[li]; sites[ns].from = c; sites[ns].covered = covered; ns++;
            continue;
        }
        if (!n->has_dst || n->dst_op.mode != MODE_REG) continue;
        const char *rn = n->dst_op.reg;
        int r = get_reg_index(rn);
        if (r < 1 || r > 10 || (rn[0] != 'R' && rn[0] != 'r')) continue;
        if (n->type == OP_PUSH) {
            const AsmNode *pop = NULL;
            if (!cover[r] && is_register_save(n, rn, &pop) && save_range_closed(n, pop)) cover[r] = pop;
            continue;
        }
        if (n == cover[r]) { cover[r] = NULL; continue; }   // the POP that gives the caller's value back
        if (n->type == OP_JMP || n->type == OP_JT || n->type == OP_JF) continue;
        if (!cover[r]) written[c] |= (unsigned short)(1u << r);
    }

    // 2. a class also writes what its direct callees write (outside a save around the call)
    for (bool changed = true; changed; ) {
        changed = false;
        for (int i = 0; i < ns; i++) {
            unsigned short w = (unsigned short)(written[sites[i].from] | (written[sites[i].target] & ~sites[i].covered));
            if (w != written[sites[i].from]) { written[sites[i].from] = w; changed = true; }
        }
    }

    // registers the ABI already returns need no help
    unsigned short abi = 0;
    for (int r = 1; r <= 10; r++) {
        char nm[8]; snprintf(nm, sizeof(nm), "R%d", r);
        if (is_live_out_register(nm)) abi |= (unsigned short)(1u << r);
    }

    // 3. fixed point: a register a class writes is an output if some direct caller reads it after the call
    long queries = 0;
    bool changed = true, gave_up = false;
    for (int round = 0; changed && round < 8; round++) {
        changed = false;
        memset(g_callee, -1, (size_t)g_label_count * sizeof(*g_callee));
        for (int i = 0; i < ns && !gave_up; i++) {
            int t = sites[i].target;
            unsigned short cand = (unsigned short)(written[t] & ~abi & ~g_extra[t]);
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
        for (int t = 0; t < nc; t++) g_extra[t] |= (unsigned short)(written[t] & ~abi);
    }
    memset(g_callee, -1, (size_t)g_label_count * sizeof(*g_callee));
    free(sites); free(written);
}
