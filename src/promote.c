#include "v32opt.h"

// ============================================================================
// promote -- stack-slot-to-register promotion passes
// ============================================================================
//
// Three optimization passes (-fpromote-leaf, -fpromote-regs, -fpromote-loops;
// all part of -O2 and up, and only worth it together with the -O1 copy
// cleanup, peephole-dead-regs / peephole-copies, which cancels the register
// copies promotion leaves behind):
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
//     preheader load (runs once, before the loop header label) and a store
//     at the loop's exit.
//
// All three follow the same shape: find a [BP-off] stack slot referenced
// enough times in a safe, CALL-free region to be worth a register, rewrite
// every reference in that region to the register FIRST, then splice in
// the load(s)/store(s) needed to keep the stack slot's own backing memory
// correct at the region's boundaries -- always AFTER the rewrite has
// finished with the original node range, never before (see the
// "BUG FIX" comments at each insertion site for why that order matters).
//
// What may be promoted
//   - locals ([BP-N]) only. Parameters ([BP+N]) and the saved BP / return
//     address slots are never touched (see slot_promotable).
//
// What makes a function ineligible (checked over the WHOLE function, not
// just the region being promoted -- a pointer made outside a loop is just
// as capable of aliasing a slot inside it):
//   - anything that captures or destroys the frame address (see
//     bp_value_escapes: "MOV R1, BP", "LEA R1, [BP-N]", "IADD R1, BP", ...);
//   - anything that captures the stack pointer (sp_value_escapes);
//   - no recognizable "MOV BP, SP" prologue to anchor the first load to.
//
// How the register is chosen
//   A candidate R1..R10 must (a) not appear anywhere in the region, and
//   (b) be PROVEN dead at the point the load is inserted by the
//   control-flow-aware liveness query (liveness.c) -- never read on any
//   path before it is written again, with the ABI's return registers
//   (R0, and R2/R3 for v32lua's multi-value returns) treated as live at
//   every RET. (a) alone is what the passes used to rely on; it is
//   not enough when a value reaches the region from before it (or leaves
//   it for after) without being named inside it -- e.g. a function that
//   only passes R2/R3 through from a callee, or a loop whose preheader
//   sits in front of a register computed earlier and used after the loop.
//   R11-R13 are never used: they double as MOVS/SETS/CMPS's implicit
//   CR/SR/DR operands, which no textual scan can see.
//
// Stores
//   A promoted local is NOT stored back before RET: this function's frame
//   is dead the moment it returns, and no pointer into it can exist (the
//   eligibility checks above), so the store could never be observed. The
//   stores that remain are exactly the ones that can be: before the CALL
//   that ends a segment (promote-regs, the next segment reloads), and at
//   the loop exit (promote-loops, code after the loop reads the slot).
//
// All three report through `-d` like every other pass in this project:
// insert_debug_comment logs an existing instruction's original text right
// before it's rewritten in place, and every newly-spliced load/store node
// gets its own descriptive debug comment immediately after insertion.
// ============================================================================

#define SLOT_BIAS        128        // slot index = BP-relative offset + SLOT_BIAS
#define SLOT_COUNT       129        // offsets -128..0
#define SLOT_MAX_ABS     127        // |offset| tracked; farther slots are left alone
#define PROMOTE_FIRST_REG  1        // R1 ... R10 (R0 returns values; R11-13 are CR/SR/DR)
#define PROMOTE_LAST_REG  10

#define MIN_REFS_FUNC     3         // weighted references to promote a slot in a
                                    // function / CALL-free segment
#define MIN_REFS_LOOP     2         // ... and in a loop body

// A reference inside a nested loop runs about this many times more often
// than one outside it.
#define LOOP_WEIGHT       8
#define MAX_LOOP_DEPTH    3

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
// Helper: does this instruction take or destroy the BP register's VALUE
// in a way that makes a [BP-off] slot's address untrackable?
//
// BUG FIX: all three promotion passes used to exclude EVERY MOV from
// this check (so the prologue/epilogue and ordinary "MOV [BP-off], x"
// local accesses wouldn't false-positive). But that exclusion also hid
// "MOV R1, BP" -- which CAPTURES the frame address into a general
// register, after which a [BP-off] reference and an R1-based reference
// can alias -- and "MOV BP, x", which destroys the frame. Only the two
// frame idioms ("MOV BP, SP" / "MOV SP, BP") and indirect [BP-off]
// accesses are exempt now; a DIRECT BP register operand on any other
// MOV (or on any non-MOV instruction, as before) blocks promotion.
// -------------------------------------------------------------------
static bool bp_value_escapes(AsmNode *n) {
    if (!n) return false;
    if (n->type == OP_PUSH || n->type == OP_POP) return false;

    bool dst_is_bp = (n->has_dst && n->dst_op.mode == MODE_REG &&
                      str_case_eq(n->dst_op.reg, "BP"));
    bool src_is_bp = (n->has_src && n->src_op.mode == MODE_REG &&
                      str_case_eq(n->src_op.reg, "BP"));

    if (n->type == OP_MOV) {
        // exempt the frame idioms
        if (dst_is_bp && n->has_src && n->src_op.mode == MODE_REG &&
            str_case_eq(n->src_op.reg, "SP")) {
            return false;                       // "MOV BP, SP" (prologue)
        }
        if (src_is_bp && n->has_dst && n->dst_op.mode == MODE_REG &&
            str_case_eq(n->dst_op.reg, "SP")) {
            return false;                       // "MOV SP, BP" (epilogue)
        }
        // any other direct BP operand on a MOV escapes/destroys BP;
        // indirect "MOV [BP-off], x" accesses are fine (that is exactly
        // the access pattern being promoted).
        return dst_is_bp || src_is_bp;
    }

    // Any other instruction: a direct BP operand ("IADD R1, BP" computes a
    // local's address) escapes, and so does LEA of a BP-relative operand
    // ("LEA R1, [BP-3]" is the address-of-local idiom). A plain indirect
    // BP-relative OPERAND of any other instruction ("IADD R0, [BP+2]") reads
    // or writes the slot's contents and is exactly the access pattern being
    // promoted.
    if (dst_is_bp || src_is_bp) return true;
    if (n->type == OP_LEA) {
        return (n->has_dst && n->dst_op.mode == MODE_INDIRECT &&
                str_case_eq(n->dst_op.reg, "BP")) ||
               (n->has_src && n->src_op.mode == MODE_INDIRECT &&
                str_case_eq(n->src_op.reg, "BP"));
    }
    return false;
}

// -------------------------------------------------------------------
// Helper: does this instruction copy the STACK POINTER's value out where
// it can be used as a pointer (and so reach a local, which lives just
// above SP: "MOV R1, SP", "IADD R1, SP", "LEA R1, [SP+N]")?
//
// Plain accesses through SP ("MOV [SP+1], R0" -- staging a call's
// arguments) are NOT escapes: the argument area sits below the locals,
// and CALL/PUSH/POP/ISUB SP/MOV SP,BP merely move SP.
// -------------------------------------------------------------------
static bool sp_value_escapes(AsmNode *n) {
    if (!n || n->type == OP_PUSH || n->type == OP_POP) return false;

    if (n->type == OP_LEA) {
        return (n->has_src && n->src_op.mode == MODE_INDIRECT &&
                str_case_eq(n->src_op.reg, "SP"));
    }
    // SP read as a plain register operand (source side), other than the
    // "MOV BP, SP" frame setup
    if (n->has_src && n->src_op.mode == MODE_REG &&
        str_case_eq(n->src_op.reg, "SP")) {
        if (n->type == OP_MOV && n->has_dst && n->dst_op.mode == MODE_REG &&
            str_case_eq(n->dst_op.reg, "BP")) {
            return false;
        }
        return true;
    }
    // Non-MOV instructions with SP as the register operand also read it
    // ("IADD SP, 1" modifies it and is fine; "IADD R1, SP" is the src case
    // above), so nothing more to check on the destination side.
    return false;
}

// -------------------------------------------------------------------
// Helper: is a branch instruction (JMP/JT/JF)?
// -------------------------------------------------------------------
static bool is_jump(const AsmNode *n) {
    return n && (str_case_eq(n->mnemonic, "JMP") || str_case_eq(n->mnemonic, "JT") ||
                 str_case_eq(n->mnemonic, "JF"));
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

static void branch_target(AsmNode *n, char *out, size_t sz) {
    char tmp[128];
    safe_str_copy(tmp, branch_target_raw(n), sizeof(tmp));
    safe_str_copy(out, trim(tmp), sz);
}

// -------------------------------------------------------------------
// Helper: is there a label named `name` in [from, to)?
// -------------------------------------------------------------------
static bool label_in_range(const char *name, AsmNode *from, AsmNode *to) {
    char lbl[128];
    for (AsmNode *n = from; n && n != to; n = n->next) {
        if (n->type != OP_LABEL) continue;
        get_label_name(n, lbl, sizeof(lbl));
        if (str_case_eq(lbl, name)) return true;
    }
    return false;
}

// -------------------------------------------------------------------
// Helper: does any branch OUTSIDE [from, to) target a label that sits
// inside [from, to)?  (`scope_first`..`scope_last` bound the search to
// the enclosing function: label targets are function-local by this
// compiler family's own naming convention, and re-scanning the whole
// program for every candidate region was measured to take minutes on a
// 300K-line real program.)
//
// With `except` non-NULL, branches to that one label are tolerated
// (a loop's own back-edge target when the caller handles it separately).
// -------------------------------------------------------------------
static bool branch_enters_region(AsmNode *scope_first, AsmNode *scope_last,
                                 AsmNode *from, AsmNode *to, const char *except) {
    bool inside = false;
    for (AsmNode *n = scope_first; n; n = n->next) {
        if (n == from) inside = true;
        if (n == to)   inside = false;
        if (!inside && is_jump(n)) {
            char target[128];
            branch_target(n, target, sizeof(target));
            if ((!except || !str_case_eq(target, except)) &&
                label_in_range(target, from, to)) return true;
        }
        if (n == scope_last) break;
    }
    return false;
}

// -------------------------------------------------------------------
// Helper: function boundaries
// -------------------------------------------------------------------
static bool is_return_label_text(const char *lbl) {
    size_t len = strlen(lbl);
    return len >= 7 && str_case_eq(lbl + len - 7, "_return");
}

// A function's entry label: "__function_<name>" (not "..._return"), or the
// Lua compiler's "__global_scope_initialization".
static bool is_function_entry(const AsmNode *n) {
    if (!n || n->type != OP_LABEL) return false;
    char lbl[128];
    get_label_name(n, lbl, sizeof(lbl));
    if (str_case_eq(lbl, "__global_scope_initialization")) return true;
    return strncmp(lbl, "__function_", 11) == 0 && !is_return_label_text(lbl);
}

// The last node belonging to the function whose entry label is `entry`.
static AsmNode *function_last(AsmNode *entry) {
    AsmNode *last = entry;
    for (AsmNode *s = entry->next; s; s = s->next) {
        if (s->type == OP_LABEL) {
            char lbl[128];
            get_label_name(s, lbl, sizeof(lbl));
            if ((strncmp(lbl, "__function_", 11) == 0 && !is_return_label_text(lbl)) ||
                strncmp(lbl, "__literal_", 10) == 0 ||
                (strncmp(lbl, "__global_", 9) == 0 &&
                 !str_case_eq(lbl, "__global_scope_initialization"))) {
                break;
            }
        }
        last = s;
    }
    return last;
}

// -------------------------------------------------------------------
// Helper: whole-function eligibility. A frame address (or SP value) that
// escapes anywhere in the function lets a pointer alias any slot at any
// time, so no slot of that function can be kept in a register.
// -------------------------------------------------------------------
static bool function_frame_escapes(AsmNode *first, AsmNode *last) {
    for (AsmNode *n = first; n; n = n->next) {
        if (bp_value_escapes(n) || sp_value_escapes(n)) return true;
        if (n == last) break;
    }
    return false;
}

// -------------------------------------------------------------------
// Helper: find the prologue's "MOV BP, SP" in [from, to) and return the
// node after which function-frame loads may go -- the "ISUB SP, N" that
// reserves the locals when it follows (BUG FIX: a load spliced in between
// "MOV BP, SP" and "ISUB SP, N" reads memory outside the frame --
// uninitialized stack, not the local's real initial value. Confirmed live
// on nostalgick.asm's __function_Game_draw_pause_overlay.). NULL if the
// prologue isn't there: there is no other verified-safe place for the
// initial load, so the caller must skip the function.
// -------------------------------------------------------------------
static AsmNode *prologue_anchor(AsmNode *from, AsmNode *to) {
    for (AsmNode *p = from; p && p != to; p = p->next) {
        if (p->type == OP_MOV && p->has_dst && p->has_src &&
            str_case_eq(p->dst_op.reg, "BP") && str_case_eq(p->src_op.reg, "SP")) {
            AsmNode *q = p->next;
            while (q && q != to && q->type == OP_OTHER) q = q->next;
            if (q && q != to && str_case_eq(q->mnemonic, "ISUB") &&
                q->has_dst && str_case_eq(q->dst_op.reg, "SP")) {
                return q;
            }
            return p;
        }
    }
    return NULL;
}

// -------------------------------------------------------------------
// Region scan: which registers appear, and how often (and how often
// when weighted by loop nesting) each BP-relative slot is referenced.
// -------------------------------------------------------------------
typedef struct {
    bool reg_used[16];
    int  refs[SLOT_COUNT];       // weighted reference count
    bool written[SLOT_COUNT];    // appears as a destination at least once
} RegionInfo;

// Only locals ([BP-N]). Parameters ([BP+N], N >= 2) are deliberately left
// alone: promoting one costs a load and, with the cleanup passes there are
// today, saves nothing (the register copy that replaces each read still
// executes). [BP+0] / [BP+1] are the saved BP and the return address.
static bool slot_promotable(int off) {
    return off <= -1 && off >= -SLOT_MAX_ABS;
}

static void note_reg(RegionInfo *ri, const Operand *op) {
    if ((op->mode == MODE_REG || (op->mode == MODE_INDIRECT && op->reg[0] != '\0'))) {
        int idx = get_reg_index(op->reg);
        if (idx >= 0 && idx < 16) ri->reg_used[idx] = true;
    }
}

// weights[i] is the weight of the i-th node of the region, or NULL for 1.
static void scan_region(AsmNode *from, AsmNode *to, const int *weights, RegionInfo *ri) {
    memset(ri, 0, sizeof(*ri));
    int i = 0;
    for (AsmNode *n = from; n && n != to; n = n->next, i++) {
        int w = weights ? weights[i] : 1;
        // (a PUSH/POP of a register is bookkeeping -- the prologue and
        // epilogue of a callee-save function, or a spill: liveness decides
        // whether the register is free, so they don't count as "used")
        if (n->type != OP_PUSH && n->type != OP_POP) {
            if (n->has_dst) note_reg(ri, &n->dst_op);
            if (n->has_src) note_reg(ri, &n->src_op);
        }
        if (n->has_dst && n->dst_op.mode == MODE_INDIRECT && str_case_eq(n->dst_op.reg, "BP") &&
            slot_promotable(n->dst_op.offset)) {
            ri->refs[n->dst_op.offset + SLOT_BIAS] += w;
            ri->written[n->dst_op.offset + SLOT_BIAS] = true;
        }
        if (n->has_src && n->src_op.mode == MODE_INDIRECT && str_case_eq(n->src_op.reg, "BP") &&
            slot_promotable(n->src_op.offset)) {
            ri->refs[n->src_op.offset + SLOT_BIAS] += w;
        }
    }
}

// -------------------------------------------------------------------
// Loop-nesting weights for the nodes of [from, to): every backward branch
// (to a label earlier in the region) marks the nodes between its target
// label and itself as one loop deeper. Returns a malloc'd array of
// per-node weights (NULL if the region is empty or allocation failed --
// callers treat NULL as "all 1"), and the node count in *count.
// -------------------------------------------------------------------
typedef struct { char name[128]; int idx; } LabelRef;

static int cmp_labelref(const void *a, const void *b) {
    const LabelRef *x = a, *y = b;
    int c = strcasecmp(x->name, y->name);
    return c ? c : x->idx - y->idx;
}

static int *loop_weights(AsmNode *from, AsmNode *to, int *count) {
    int n_nodes = 0, n_labels = 0;
    for (AsmNode *n = from; n && n != to; n = n->next) {
        n_nodes++;
        if (n->type == OP_LABEL) n_labels++;
    }
    *count = n_nodes;
    if (n_nodes == 0) return NULL;

    AsmNode **nodes = malloc((size_t)n_nodes * sizeof(AsmNode *));
    int *delta = calloc((size_t)n_nodes + 1, sizeof(int));
    int *weights = malloc((size_t)n_nodes * sizeof(int));
    LabelRef *labels = malloc((size_t)(n_labels ? n_labels : 1) * sizeof(LabelRef));
    if (!nodes || !delta || !weights || !labels) {
        free(nodes); free(delta); free(weights); free(labels);
        return NULL;
    }

    int i = 0, li = 0;
    for (AsmNode *n = from; n && n != to; n = n->next, i++) {
        nodes[i] = n;
        if (n->type == OP_LABEL) {
            get_label_name(n, labels[li].name, sizeof(labels[li].name));
            labels[li].idx = i;
            li++;
        }
    }
    qsort(labels, (size_t)n_labels, sizeof(LabelRef), cmp_labelref);

    char target[128];
    for (int j = 0; j < n_nodes; j++) {
        if (!is_jump(nodes[j])) continue;
        branch_target(nodes[j], target, sizeof(target));
        // the label with this name that sits furthest back, at or before j
        LabelRef key;
        safe_str_copy(key.name, target, sizeof(key.name));
        int lo = 0, hi = n_labels - 1, first = -1;
        while (lo <= hi) {                                   // first entry with this name
            int mid = (lo + hi) / 2;
            int c = strcasecmp(labels[mid].name, key.name);
            if (c < 0) lo = mid + 1; else { if (c == 0) first = mid; hi = mid - 1; }
        }
        if (first >= 0 && labels[first].idx <= j) {
            delta[labels[first].idx]++;
            delta[j + 1]--;
        }
    }
    int depth = 0;
    for (int k = 0; k < n_nodes; k++) {
        depth += delta[k];
        int d = depth > MAX_LOOP_DEPTH ? MAX_LOOP_DEPTH : depth;
        int w = 1;
        while (d-- > 0) w *= LOOP_WEIGHT;
        weights[k] = w;
    }
    free(nodes); free(delta); free(labels);
    return weights;
}

// -------------------------------------------------------------------
// Helper: can control come back to [from, to) after leaving it -- is there
// a jump AFTER the region to a label BEFORE it (an enclosing loop)? A loop
// that is entered again re-reads its slots from memory in the preheader, so
// what it left in a register at the last exit still has to be stored.
// -------------------------------------------------------------------
static bool region_reentered(AsmNode *scope_first, AsmNode *scope_last, AsmNode *from, AsmNode *to) {
    int before = 0;
    for (AsmNode *n = scope_first; n && n != from; n = n->next) if (n->type == OP_LABEL) before++;
    if (before == 0) return false;
    LabelRef *labels = malloc((size_t)before * sizeof(LabelRef));
    if (!labels) return true;                        // can't tell: assume it can
    int li = 0;
    for (AsmNode *n = scope_first; n && n != from; n = n->next) {
        if (n->type != OP_LABEL) continue;
        get_label_name(n, labels[li].name, sizeof(labels[li].name));
        labels[li].idx = li;
        li++;
    }
    qsort(labels, (size_t)before, sizeof(LabelRef), cmp_labelref);

    bool again = false;
    bool after = false;
    for (AsmNode *n = scope_first; n && !again; n = n->next) {
        if (n == to) after = true;
        if (after && is_jump(n)) {
            LabelRef key;
            branch_target(n, key.name, sizeof(key.name));
            int lo = 0, hi = before - 1;
            while (lo <= hi) {
                int mid = (lo + hi) / 2;
                int c = strcasecmp(labels[mid].name, key.name);
                if (c == 0) { again = true; break; }
                if (c < 0) lo = mid + 1; else hi = mid - 1;
            }
        }
        if (n == scope_last) break;
    }
    free(labels);
    return again;
}

static void link_before(AsmNode *pos, AsmNode *nn) {
    nn->prev = pos->prev;
    nn->next = pos;
    if (pos->prev) pos->prev->next = nn;
    pos->prev = nn;
}

static void link_after(AsmNode *pos, AsmNode *nn) {
    nn->prev = pos;
    nn->next = pos->next;
    if (pos->next) pos->next->prev = nn;
    pos->next = nn;
}

// -------------------------------------------------------------------
// Calling convention. The stock Vircon32 C compiler is CALLEE-SAVE: a
// function pushes every register it uses (R1 and up) right after "ISUB SP,
// N" and pops them before "MOV SP, BP", because its callers keep live
// temporaries in R1, R2, R3, ... across a CALL ("h(f(1), f(2)) + f(3) *
// (f(4) + ...)" holds results in up to R8 across calls). v32lua is
// caller-save: it spills around calls and its callees clobber freely.
//
// A promoted local lives in a register the function did not use before, so
// without a save/restore the promotion clobbers the caller's value (the
// passes did exactly that to stock-compiled code; confirmed on a deeply
// nested call expression). So in every mode but -L lua a register is only
// used if the function already saves it, or if a push/pop pair for it can
// be added to the prologue and to every epilogue.
// -------------------------------------------------------------------
#define MAX_EPILOGUES 8
// A fresh push/pop costs 2 instructions per call: ask for this many more
// (weighted) references before spending one. Tuned on the stock-compiler
// corpus (any value from 1 to 6 landed within 0.15 points).
#define FRESH_SAVE_EXTRA      3
#define FRESH_SAVE_EXTRA_LOOP 1

typedef struct {
    AsmNode *entry_end;             // last node of the prologue: loads go right after it
    bool     saved[16];             // registers the function saves/restores
    bool     lua;                   // caller-save ABI: nothing needs saving
    bool     can_save;              // a push/pop can be added (every exit is a recognised epilogue)
    int      n_epi;
    AsmNode *epi_first[MAX_EPILOGUES];   // our pops go right before this node, per epilogue
} FrameInfo;

static AsmNode *prev_real(AsmNode *n) {
    n = n ? n->prev : NULL;
    while (n && n->type == OP_OTHER) n = n->prev;
    return n;
}

static int pushpop_reg(const AsmNode *n) {            // register number of PUSH/POP Rn, else -1
    const Operand *op = n->has_dst ? &n->dst_op : (n->has_src ? &n->src_op : NULL);
    if (!op || op->mode != MODE_REG) return -1;
    int r = get_reg_index(op->reg);
    return (r >= 1 && r <= 13) ? r : -1;
}

static void frame_info(FrameInfo *fi, AsmNode *anchor, AsmNode *first, AsmNode *limit) {
    memset(fi, 0, sizeof(*fi));
    fi->entry_end = anchor;
    fi->lua = (config.lang_mode == LANG_LUA);
    for (AsmNode *n = anchor->next; n && n != limit; n = n->next) {
        if (n->type == OP_OTHER) continue;
        int r;
        if (n->type == OP_PUSH && (r = pushpop_reg(n)) > 0) { fi->saved[r] = true; fi->entry_end = n; continue; }
        break;
    }
    if (fi->lua) return;

    fi->can_save = true;
    for (AsmNode *n = first; n && n != limit && fi->can_save; n = n->next) {
        if (is_jump(n)) {                                // leaving the function would skip our pops
            char target[128];
            branch_target(n, target, sizeof(target));
            if (!label_in_range(target, first, limit)) fi->can_save = false;
        }
        if (n->type != OP_RET) continue;
        AsmNode *p = prev_real(n);
        if (!p || p->type != OP_POP || !str_case_eq(p->dst_op.reg, "BP")) { fi->can_save = false; break; }
        AsmNode *m = prev_real(p);
        if (!m || m->type != OP_MOV || !m->has_dst || !m->has_src ||
            !str_case_eq(m->dst_op.reg, "SP") || !str_case_eq(m->src_op.reg, "BP")) { fi->can_save = false; break; }
        AsmNode *ins = m;
        for (AsmNode *q = prev_real(m); q && q->type == OP_POP && pushpop_reg(q) > 0; q = prev_real(q)) ins = q;
        if (fi->n_epi == MAX_EPILOGUES) { fi->can_save = false; break; }
        fi->epi_first[fi->n_epi++] = ins;
    }
}

// Make sure the function saves and restores Rr; false if it can't.
static bool frame_save_reg(FrameInfo *fi, int r, OptType t) {
    if (fi->lua || fi->saved[r]) return true;
    if (!fi->can_save) return false;
    char nm[8], raw[24];
    snprintf(nm, sizeof(nm), "R%d", r);
    snprintf(raw, sizeof(raw), "    PUSH %s", nm);
    AsmNode *push = create_node(raw, OP_PUSH, "PUSH", nm, NULL);
    link_after(fi->entry_end, push);
    fi->entry_end = push;
    char dbg[96];
    snprintf(dbg, sizeof(dbg), "PROMOTE: %s is promoted below, so the function now saves it (callee-save ABI)", nm);
    insert_debug_comment(push, t, dbg);
    snprintf(raw, sizeof(raw), "    POP %s", nm);
    for (int i = 0; i < fi->n_epi; i++) {
        AsmNode *pop = create_node(raw, OP_POP, "POP", nm, NULL);
        link_before(fi->epi_first[i], pop);
        fi->epi_first[i] = pop;
    }
    fi->saved[r] = true;
    return true;
}

// -------------------------------------------------------------------
// Helper: choose a register for a promotion whose load will be spliced in
// BEFORE `at`: not mentioned in the region (ri->reg_used), provably dead
// from `at` on (see the file header), and usable under the calling
// convention above: already saved by the function (free), or -- when
// `allow_fresh` -- one that can get a push/pop (R4-R10: R2/R3 stay out, as
// v32lua returns values there). Marks it used and returns its number, or
// -1; *fresh says a push/pop still has to be added (frame_save_reg).
// -------------------------------------------------------------------
static int pick_register(AsmNode *at, RegionInfo *ri, const FrameInfo *fi, bool allow_fresh, bool *fresh) {
    *fresh = false;
    for (int pass = 0; pass < 2; pass++) {
        for (int r = PROMOTE_FIRST_REG; r <= PROMOTE_LAST_REG; r++) {
            if (ri->reg_used[r]) continue;
            bool is_saved = fi->lua || fi->saved[r];
            if (pass == 0 && !is_saved) continue;
            if (pass == 1) {
                if (is_saved || !allow_fresh || !fi->can_save || r < 4) continue;
            }
            char name[8];
            snprintf(name, sizeof(name), "R%d", r);
            if (!reg_dead_from(at, name)) continue;
            ri->reg_used[r] = true;
            *fresh = (pass == 1);
            return r;
        }
    }
    return -1;
}

// -------------------------------------------------------------------
// Helper: rewrite every reference to [BP+off] in [from, to) to `reg_name`.
// BUG FIX (kept from the original passes): callers rewrite BEFORE they
// splice the load in. Inserting the load first put "MOV R_free,
// [BP-off]" inside the very range scanned here, where it was caught by
// its own rewrite and corrupted into "MOV R_free, R_free" -- a no-op that
// never loads anything. Confirmed by direct reproduction. Doing the
// rewrite first, over the original body, makes that structurally
// impossible.
// Returns the number of rewritten instructions.
// -------------------------------------------------------------------
static int rewrite_slot(AsmNode *from, AsmNode *to, int off, const char *reg_name, OptType t) {
    int count = 0;
    for (AsmNode *n = from; n && n != to; n = n->next) {
        bool modified = false;
        if (n->has_dst && n->dst_op.mode == MODE_INDIRECT && str_case_eq(n->dst_op.reg, "BP") &&
            n->dst_op.offset == off) {
            promote_operand_to_reg(&n->dst_op, reg_name);
            modified = true;
        }
        if (n->has_src && n->src_op.mode == MODE_INDIRECT && str_case_eq(n->src_op.reg, "BP") &&
            n->src_op.offset == off) {
            promote_operand_to_reg(&n->src_op, reg_name);
            modified = true;
        }
        if (modified) {
            // Log the original instruction text before it's overwritten
            // below -- same convention every other pass follows.
            insert_debug_comment(n->prev, t, n->raw);
            if (n->has_dst && n->has_src) {
                node_set_rawf(n, "    %s %s, %s", n->mnemonic, n->dst_op.raw, n->src_op.raw);
            } else if (n->has_dst) {
                node_set_rawf(n, "    %s %s", n->mnemonic, n->dst_op.raw);
            }
            count++;
        }
    }
    return count;
}

// -------------------------------------------------------------------
// Helper: is [BP+off] READ anywhere in the function outside [from, to)?
// Slots are only reachable through their literal operands here (the
// escape checks guarantee it), so a slot nobody reads outside the region
// has an unobservable value once the region is left: the store that would
// keep memory current can be left out. A source operand is a read; so is
// the destination of anything but a plain MOV (a read-modify-write).
// -------------------------------------------------------------------
static bool slot_read_outside(AsmNode *scope_first, AsmNode *scope_last,
                              AsmNode *from, AsmNode *to, int off) {
    bool inside = false;
    for (AsmNode *n = scope_first; n; n = n->next) {
        if (n == from) inside = true;
        if (n == to)   inside = false;
        if (!inside) {
            if (n->has_src && n->src_op.mode == MODE_INDIRECT &&
                str_case_eq(n->src_op.reg, "BP") && n->src_op.offset == off) return true;
            if (n->has_dst && n->dst_op.mode == MODE_INDIRECT &&
                str_case_eq(n->dst_op.reg, "BP") && n->dst_op.offset == off &&
                n->type != OP_MOV) return true;
        }
        if (n == scope_last) break;
    }
    return false;
}

// -------------------------------------------------------------------
// Helper: build "MOV <dst>, <src>" and splice it in before / after a node
// -------------------------------------------------------------------
static AsmNode *make_mov(const char *dst, const char *src) {
    char raw[96];
    snprintf(raw, sizeof(raw), "    MOV %s, %s", dst, src);
    return create_node(raw, OP_MOV, "MOV", dst, src);
}

static void slot_text(int off, char *buf, size_t sz) {
    snprintf(buf, sz, "[BP%+d]", off);
}

// ===================================================================
// Pass: Stack Slot to Register Promotion (Scalar Replacement)
// ===================================================================
int pass_promote_stack_slots(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;
    liveness_begin(head);

    while (curr) {
        if (!is_function_entry(curr)) { curr = curr->next; continue; }

        AsmNode *end_of_func = function_last(curr);
        AsmNode *first = curr->next;
        AsmNode *limit = end_of_func->next;

        // Phase A: Analyze Function Safety & Register Liveness
        bool is_leaf = true;
        for (AsmNode *n = first; n && n != limit; n = n->next) {
            if (str_case_eq(n->mnemonic, "CALL")) { is_leaf = false; break; }
        }
        AsmNode *anchor = NULL;
        if (is_leaf && !function_frame_escapes(first, end_of_func)) {
            anchor = prologue_anchor(first, limit);
        }

        // Phase B: Execute Function-Wide Promotion (Leaf Functions Only)
        if (anchor) {
            FrameInfo fi;
            frame_info(&fi, anchor, first, limit);
            int count;
            int *w = loop_weights(first, limit, &count);
            RegionInfo ri;
            scan_region(first, limit, w, &ri);
            free(w);

            for (int idx = 0; idx < SLOT_COUNT; idx++) {
                int off = idx - SLOT_BIAS;
                if (!slot_promotable(off) || ri.refs[idx] < MIN_REFS_FUNC) continue;

                // TRIGGER CAP: gate this offset's whole promotion (every
                // rewritten reference plus its load) as one atomic unit.
                if (!trigger_allowed()) continue;

                bool fresh;
                int r = pick_register(fi.entry_end->next, &ri, &fi,
                                      ri.refs[idx] >= MIN_REFS_FUNC + FRESH_SAVE_EXTRA, &fresh);
                if (r < 0) continue;
                char reg_name[16], slot[24];
                snprintf(reg_name, sizeof(reg_name), "R%d", r);
                slot_text(off, slot, sizeof(slot));
                if (fresh) frame_save_reg(&fi, r, OPT_PROMOTE_LEAF);

                optimizations += rewrite_slot(first, limit, off, reg_name, OPT_PROMOTE_LEAF);

                char dbg[128];
                if (reg_dead_from(fi.entry_end->next, reg_name)) {
                    // every path writes the register before reading it: the
                    // slot's incoming value is never used, so there is nothing to load
                    snprintf(dbg, sizeof(dbg),
                             "PROMOTE-LEAF: %s lives in %s (always written before read: no load)",
                             slot, reg_name);
                    insert_debug_comment(fi.entry_end, OPT_PROMOTE_LEAF, dbg);
                } else {
                    AsmNode *load = make_mov(reg_name, slot);
                    link_after(fi.entry_end, load);
                    snprintf(dbg, sizeof(dbg),
                             "PROMOTE-LEAF: load of %s into %s after the prologue (whole-function, leaf-only)",
                             slot, reg_name);
                    insert_debug_comment(load, OPT_PROMOTE_LEAF, dbg);
                }
            }
        }

        curr = limit;
    }

    liveness_end();
    return optimizations;
}

// ===================================================================
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
// Enabling both is safe (harmless, idempotent overlap on leaf functions
// specifically) but redundant on them; promote-regs's actual value-add
// is entirely in non-leaf functions.
// ===================================================================
int pass_promote_regs(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;
    liveness_begin(head);

    while (curr) {
        if (!is_function_entry(curr)) { curr = curr->next; continue; }

        AsmNode *end_of_func = function_last(curr);
        AsmNode *func_first = curr->next;
        AsmNode *func_limit = end_of_func->next;

        // GUARD: function-wide frame-escape check -- see the file header.
        // CALL segmentation doesn't change this; it's unrelated to leafness.
        AsmNode *anchor = NULL;
        if (!function_frame_escapes(func_first, end_of_func)) {
            // The prologue must be recognizable for the function to be
            // trusted at all (a function with no "MOV BP, SP" -- naked, or
            // hand-written assembly -- has no frame of its own to promote
            // slots of); the first segment's loads go right after it.
            anchor = prologue_anchor(func_first, func_limit);
        }
        if (!anchor) { curr = func_limit; continue; }
        FrameInfo fi;
        frame_info(&fi, anchor, func_first, func_limit);

        // ----------------------------------------------------------------
        // Walk the function in CALL-free segments: [seg_start, seg_limit)
        // where seg_limit is either the next CALL or func_limit (the
        // function's end). Apply scalar replacement independently within
        // each segment -- nothing promoted in one segment is ever assumed
        // live in another.
        // ----------------------------------------------------------------
        AsmNode *seg_start = func_first;
        while (seg_start && seg_start != func_limit) {
            bool is_first_segment = (seg_start == func_first);

            AsmNode *seg_limit = seg_start;
            while (seg_limit && seg_limit != func_limit &&
                   !str_case_eq(seg_limit->mnemonic, "CALL")) {
                seg_limit = seg_limit->next;
            }
            AsmNode *next_seg_start = (seg_limit == func_limit) ? func_limit
                                       : (seg_limit ? seg_limit->next : NULL);
            // (BUG FIX kept from the original: advancing to seg_limit->next
            // unconditionally overran the function boundary on the last
            // segment and walked the REST OF THE PROGRAM as more segments of
            // this function -- -O3 + -fpromote-regs did not finish in over a
            // minute on a 300K-line program.)

            // ------------------------------------------------------------
            // SAFETY GUARD: a segment only behaves like the straight-line,
            // single-entry/single-exit region this pass's "load once at the
            // top, flush once at the natural end (the ending CALL)" model
            // assumes if NOTHING branches into or out of its middle:
            //   (a) a branch INSIDE the segment (JT/JF/JMP) jumps to a
            //       label OUTSIDE it -- an early exit that reaches code
            //       after the segment without ever running the flush this
            //       pass places before the ending CALL;
            //   (b) a branch OUTSIDE the segment jumps to a label INSIDE it
            //       -- e.g. a loop back-edge landing on a loop header that
            //       sits inside this CALL-free stretch -- reaching the
            //       segment's promoted code without ever running the load.
            // (Confirmed by reproduction: a guard-clause "JF R5, __after_call"
            // jumping past a promoted variable's flush-before-CALL left the
            // stack slot unwritten; code reached via that branch read stale
            // memory -- the real-game blank-screen-at-start failure.)
            // Computed jumps ("JMP R1") have no label target and so also
            // fail (a).
            // ------------------------------------------------------------
            bool segment_safe = true;
            for (AsmNode *m = seg_start; segment_safe && m != seg_limit; m = m->next) {
                if (!is_jump(m)) continue;
                char target[128];
                branch_target(m, target, sizeof(target));
                if (!label_in_range(target, seg_start, seg_limit)) segment_safe = false;
            }
            if (segment_safe &&
                branch_enters_region(func_first, end_of_func, seg_start, seg_limit, NULL)) {
                segment_safe = false;
            }
            if (!segment_safe) { seg_start = next_seg_start; continue; }

            // The first segment's loads go right after the prologue, so the
            // prologue has to be inside it (a CALL ahead of "MOV BP, SP"
            // would put the load in a later segment's territory).
            if (is_first_segment) {
                bool anchor_inside = false;
                for (AsmNode *m = seg_start; m && m != seg_limit; m = m->next) {
                    if (m == anchor) { anchor_inside = true; break; }
                }
                if (!anchor_inside) { seg_start = next_seg_start; continue; }
            }

            // ---- Analyze this segment: register usage + BP-off refs ----
            int count;
            int *w = loop_weights(seg_start, seg_limit, &count);
            RegionInfo ri;
            scan_region(seg_start, seg_limit, w, &ri);
            free(w);

            for (int idx = 0; idx < SLOT_COUNT; idx++) {
                int off = idx - SLOT_BIAS;
                if (!slot_promotable(off) || ri.refs[idx] < MIN_REFS_FUNC) continue;

                // The load goes right after the prologue in the first
                // segment, right before seg_start in a later one (after the
                // CALL; for the first segment that must not be before the
                // frame exists).
                AsmNode *load_at = is_first_segment ? fi.entry_end->next : seg_start;

                // TRIGGER CAP: gate the WHOLE promotion of this one offset
                // (its load, every rewritten reference, and its flush) as a
                // single atomic unit -- applying only part of a promotion
                // would produce code that corresponds to no real pass state.
                if (!trigger_allowed()) continue;

                bool fresh;
                int r = pick_register(load_at, &ri, &fi,
                                      ri.refs[idx] >= MIN_REFS_FUNC + FRESH_SAVE_EXTRA, &fresh);
                if (r < 0) continue;
                char reg_name[16], slot[24];
                snprintf(reg_name, sizeof(reg_name), "R%d", r);
                slot_text(off, slot, sizeof(slot));
                if (fresh) {
                    frame_save_reg(&fi, r, OPT_PROMOTE_REGS);
                    if (is_first_segment) load_at = fi.entry_end->next;   // the push went in ahead of it
                }

                optimizations += rewrite_slot(seg_start, seg_limit, off, reg_name, OPT_PROMOTE_REGS);

                // A write must reach the stack slot before the CALL that ends
                // the segment if anything reads the slot afterwards (the next
                // segment reloads it). The last segment ends in RET, where the
                // frame dies: no flush. A slot nobody else reads needs none.
                char dbg[160];
                if (ri.written[idx] && seg_limit && str_case_eq(seg_limit->mnemonic, "CALL") &&
                    slot_read_outside(func_first, end_of_func, seg_start, seg_limit, off)) {
                    AsmNode *store = make_mov(slot, reg_name);
                    link_before(seg_limit, store);
                    optimizations++;
                    snprintf(dbg, sizeof(dbg),
                             "PROMOTE-REGS: flush %s back to %s before the CALL that ends this segment (next segment reloads independently)",
                             reg_name, slot);
                    insert_debug_comment(store, OPT_PROMOTE_REGS, dbg);
                }

                // The load is needed only if some path reads the register
                // before writing it (asked AFTER the flush above, which is a
                // read of the register too).
                if (reg_dead_from(load_at, reg_name)) {
                    snprintf(dbg, sizeof(dbg),
                             "PROMOTE-REGS: %s lives in %s (always written before read: no load)",
                             slot, reg_name);
                    insert_debug_comment(load_at->prev, OPT_PROMOTE_REGS, dbg);
                } else {
                    AsmNode *load = make_mov(reg_name, slot);
                    if (is_first_segment) link_after(fi.entry_end, load);
                    else                  link_before(seg_start, load);
                    snprintf(dbg, sizeof(dbg),
                             "PROMOTE-REGS: %s-segment load of %s into %s (CALL-free segment, independent of any other segment)",
                             is_first_segment ? "first" : "later", slot, reg_name);
                    insert_debug_comment(load, OPT_PROMOTE_REGS, dbg);
                }
            }

            seg_start = next_seg_start;
        }

        curr = func_limit;
    }

    liveness_end();
    return optimizations;
}

// -------------------------------------------------------------------
// Loop header test. Matches common compiler patterns after stripping one
// trailing "_<digits>" disambiguation index.
//
// BUG FIX (kept): crisp-lib's (C-mode) labels carry the distinguishing
// number BEFORE the role suffix ("__do_101780_start:"), while the Lua-mode
// compiler appends it AFTER ("__Background_draw_clouds_for_gen_start_0:"),
// so a literal-suffix match never fired under Lua mode (38 of 38 real
// loop headers in nostalgick.asm missed). The strip only ever removes one
// "_<digits>" group preceded by an underscore.
// -------------------------------------------------------------------
static bool is_loop_header_name(const char *name) {
    char norm[128];
    safe_str_copy(norm, name, sizeof(norm));
    size_t len = strlen(norm);
    size_t i = len;
    while (i > 0 && isdigit((unsigned char)norm[i - 1])) i--;
    if (i > 0 && i < len && norm[i - 1] == '_') { norm[i - 1] = '\0'; len = i - 1; }

    return (len >= 6 && str_case_eq(norm + len - 6, "_start")) ||
           (len >= 5 && str_case_eq(norm + len - 5, "_for_")) ||
           (len >= 7 && str_case_eq(norm + len - 7, "_while_"));
}

// Weighted candidate list for the loop pass, heaviest slot first.
typedef struct { int idx; int weight; } SlotCand;

static int cmp_cand(const void *a, const void *b) {
    const SlotCand *x = a, *y = b;
    if (x->weight != y->weight) return y->weight - x->weight;
    return x->idx - y->idx;
}

// ===================================================================
// Pass: Loop-Invariant Register Promotion (CALL-Free Loops)
//
// Per loop ("header: ... jump back to header", then the loop's exit label):
//   - the whole function must be free of frame/stack-pointer escapes;
//   - nothing outside the loop may branch to a label inside it (including
//     the header: that would skip the preheader load) or to its exit
//     label (that would run the exit store with nothing loaded);
//   - no CALL / RET / HLT inside, every branch inside stays inside, goes
//     back to the header, or goes to the exit label.
// Slots are taken heaviest first (references weighted by how deeply
// nested they are) until the registers that are provably free run out.
// A loop that fails any check is skipped on its own: loops nested inside
// it are still considered.
// ===================================================================
int pass_promote_loop_registers(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;
    liveness_begin(head);

    // per-function caches: escape check, and the frame (saved registers)
    AsmNode *cached_entry = NULL;
    bool     cached_valid = false;
    bool     cached_escapes = false;
    AsmNode *fi_entry = NULL;
    bool     fi_valid = false, fi_ok = false;
    FrameInfo fi;

    while (curr) {
        // 1. Identify Loop Headers (e.g., __for_1_start:, __while_1_start:)
        if (curr->type != OP_LABEL) { curr = curr->next; continue; }
        char start_lbl[128];
        get_label_name(curr, start_lbl, sizeof(start_lbl));
        if (!is_loop_header_name(start_lbl)) { curr = curr->next; continue; }

        // The enclosing function [func_first, func_last]. Hand-written code
        // with no "__function_" labels counts as one big function.
        AsmNode *entry = curr;
        while (entry && !is_function_entry(entry)) entry = entry->prev;
        AsmNode *func_first, *func_last;
        if (entry) {
            func_first = entry->next;
            func_last  = function_last(entry);
        } else {
            func_first = head->next;
            func_last  = head;
            while (func_last->next) func_last = func_last->next;
        }
        bool in_function = false;
        for (AsmNode *n = func_first; n; n = n->next) {
            if (n == curr) { in_function = true; break; }
            if (n == func_last) break;
        }
        if (!in_function) { curr = curr->next; continue; }

        // GUARD: a frame / stack address that escapes ANYWHERE in the
        // function -- not just inside this loop -- can alias a slot.
        // BUG FIX: only the loop body used to be checked. A pointer made
        // before the loop ("int *p = &v;", or a struct local whose address
        // was taken) aliases the very slot the loop reads and writes
        // directly, and the exit store then overwrote what the loop had
        // written through the pointer. Confirmed by direct reproduction
        // (v += 1; *p += 10 inside the loop lost every *p update).
        if (!cached_valid || cached_entry != entry) {
            cached_entry = entry;
            cached_valid = true;
            cached_escapes = function_frame_escapes(func_first, func_last);
        }
        if (cached_escapes) { curr = curr->next; continue; }

        // The frame: which registers the function saves (see FrameInfo). In
        // every mode but -L lua a function without a recognizable prologue
        // can't have a register added to it, so its loops stay in memory.
        if (!fi_valid || fi_entry != entry) {
            fi_entry = entry;
            fi_valid = true;
            AsmNode *an = entry ? prologue_anchor(func_first, func_last->next) : NULL;
            if (an) {
                frame_info(&fi, an, func_first, func_last->next);
                fi_ok = true;
            } else {
                memset(&fi, 0, sizeof(fi));
                fi.lua = (config.lang_mode == LANG_LUA);
                fi_ok = fi.lua;
            }
        }
        if (!fi_ok) { curr = curr->next; continue; }

        // 2. Find the Loop Back-Edge (a JMP/JT/JF back to start_lbl)
        AsmNode *end_jmp = NULL;
        for (AsmNode *scan = curr->next; scan; scan = scan->next) {
            if (scan->type == OP_LABEL) {
                char l[128];
                get_label_name(scan, l, sizeof(l));
                if (strncmp(l, "__function_", 11) == 0) break;
            }
            if (is_jump(scan)) {
                char target[128];
                branch_target(scan, target, sizeof(target));
                if (str_case_eq(target, start_lbl)) { end_jmp = scan; break; }
            }
        }
        if (!end_jmp) { curr = curr->next; continue; }
        AsmNode *body_end = end_jmp->next;          // exclusive end of [curr, end_jmp]

        // GUARD: a preheader load (inserted just before the header, so it
        // runs once on fall-through entry and is never touched by the
        // back-edge, which jumps straight to the label) is only sound if
        // fall-through is the ONLY way execution first reaches the loop.
        // A jump from WITHIN the loop back to the header (a "continue")
        // is just another back-edge; a jump from OUTSIDE to the header --
        // or to any other label in the body (a "goto" into the loop) --
        // would skip the preheader and read the promoted register
        // uninitialized.
        if (branch_enters_region(func_first, func_last, curr, body_end, NULL)) {
            curr = curr->next;
            continue;
        }

        // 3. Find the designated Loop Exit Label
        AsmNode *exit_label_node = end_jmp->next;
        while (exit_label_node && exit_label_node->type == OP_OTHER &&
              (exit_label_node->raw[0] == '\0' || exit_label_node->raw[0] == ';')) {
            exit_label_node = exit_label_node->next;
        }
        if (!exit_label_node || exit_label_node->type != OP_LABEL) { curr = curr->next; continue; }
        char exit_lbl[128];
        get_label_name(exit_label_node, exit_lbl, sizeof(exit_lbl));

        // GUARD: only the loop itself may branch to the exit label -- the
        // exit store runs with the register loaded by the preheader, which a
        // jump from elsewhere (e.g. a guard before the header) skips.
        bool exit_shared = false;
        for (AsmNode *n = func_first; n; n = n->next) {
            if (n == curr) { n = end_jmp; }           // skip the loop itself
            else if (is_jump(n)) {
                char target[128];
                branch_target(n, target, sizeof(target));
                if (str_case_eq(target, exit_lbl)) { exit_shared = true; break; }
            }
            if (n == func_last) break;
        }
        if (exit_shared) { curr = curr->next; continue; }

        // Phase A: Safety Scan across the Loop Body
        bool loop_safe = true;
        for (AsmNode *n = curr->next; n && n != end_jmp; n = n->next) {
            // Rule 1: No CALLs, RETs, or HLTs inside the loop
            if (str_case_eq(n->mnemonic, "CALL") || str_case_eq(n->mnemonic, "RET") ||
                str_case_eq(n->mnemonic, "HLT")) { loop_safe = false; break; }
            // Rule 2 (the whole function was checked above): no frame writes
            // by name either
            // Rule 3: Verify all branches jump inside loop, to start, or to exit
            if (is_jump(n)) {
                char target[128];
                branch_target(n, target, sizeof(target));
                if (!str_case_eq(target, start_lbl) && !str_case_eq(target, exit_lbl) &&
                    !label_in_range(target, curr->next, end_jmp)) { loop_safe = false; break; }
            }
        }
        if (!loop_safe) { curr = curr->next; continue; }

        // Phase B: Execute Loop Promotion
        int count;
        int *w = loop_weights(curr->next, body_end, &count);
        RegionInfo ri;
        scan_region(curr->next, body_end, w, &ri);
        free(w);

        SlotCand cand[SLOT_COUNT];
        int ncand = 0;
        for (int idx = 0; idx < SLOT_COUNT; idx++) {
            int off = idx - SLOT_BIAS;
            if (!slot_promotable(off) || ri.refs[idx] < MIN_REFS_LOOP) continue;
            cand[ncand].idx = idx;
            cand[ncand].weight = ri.refs[idx];
            ncand++;
        }
        qsort(cand, (size_t)ncand, sizeof(SlotCand), cmp_cand);

        bool enclosed = ncand > 0 && region_reentered(func_first, func_last, curr, body_end);
        int promoted = 0;
        for (int c = 0; c < ncand; c++) {
            int idx = cand[c].idx;
            int off = idx - SLOT_BIAS;

            // TRIGGER CAP: gate this offset's whole promotion (preheader
            // load, exit store, every rewritten reference) as one unit.
            if (!trigger_allowed()) continue;

            bool fresh;
            int r = pick_register(curr, &ri, &fi, ri.refs[idx] >= MIN_REFS_LOOP + FRESH_SAVE_EXTRA_LOOP, &fresh);
            if (r < 0) continue;
            char reg_name[16], slot[24];
            snprintf(reg_name, sizeof(reg_name), "R%d", r);
            slot_text(off, slot, sizeof(slot));
            if (fresh) frame_save_reg(&fi, r, OPT_PROMOTE_LOOPS);

            // Rewrite all references inside the loop body to the register
            optimizations += rewrite_slot(curr->next, body_end, off, reg_name, OPT_PROMOTE_LOOPS);

            // Exit store right after the exit label, if the loop writes the
            // slot. It is only a candidate at first: it also counts as a
            // read of the register when asking whether the preheader load is
            // needed, and may be dropped afterwards (below).
            char dbg[160];
            AsmNode *store = NULL;
            if (ri.written[idx]) {
                store = make_mov(slot, reg_name);
                link_after(exit_label_node, store);
            }

            // The preheader load goes BEFORE the loop label -- unless every
            // path through the loop writes the register before reading it.
            bool need_load = !reg_dead_from(curr, reg_name);

            // The store can go if nothing outside the loop reads the slot and
            // the loop is not re-entered with its preheader load re-reading
            // memory (an enclosing loop): the value would be unobservable.
            if (store && !slot_read_outside(func_first, func_last, curr, body_end, off) &&
                !(need_load && enclosed)) {
                remove_node(store);
                store = NULL;
                need_load = !reg_dead_from(curr, reg_name);
            }
            if (store) {
                snprintf(dbg, sizeof(dbg),
                         "PROMOTE-LOOPS: flush %s back to %s at the loop exit (%s)",
                         reg_name, slot, exit_lbl);
                insert_debug_comment(store, OPT_PROMOTE_LOOPS, dbg);
            }

            // BUG FIX (kept): the load must not go AFTER the label -- that
            // put it INSIDE the loop body, where the back-edge re-ran it every
            // iteration and discarded whatever the body had just computed into
            // the register (and, being inside the rewrite range, it was itself
            // corrupted into "MOV R, R").
            if (!need_load) {
                snprintf(dbg, sizeof(dbg),
                         "PROMOTE-LOOPS: %s lives in %s inside %s (always written before read: no preheader load)",
                         slot, reg_name, start_lbl);
                insert_debug_comment(curr->prev, OPT_PROMOTE_LOOPS, dbg);
            } else {
                AsmNode *load = make_mov(reg_name, slot);
                link_before(curr, load);
                snprintf(dbg, sizeof(dbg),
                         "PROMOTE-LOOPS: preheader load of %s into %s (runs once, before %s)",
                         slot, reg_name, start_lbl);
                insert_debug_comment(load, OPT_PROMOTE_LOOPS, dbg);
            }
            promoted++;
        }

        // A promoted loop's body is fully rewritten (inner loops included);
        // otherwise look for loops nested inside this one.
        curr = promoted ? end_jmp->next : curr->next;
    }

    liveness_end();
    return optimizations;
}

// -------------------------------------------------------------------
// For other passes (loop-unroll-partial): is the function containing `n` one
// whose frame slots can only be reached through [BP+N]/[BP-N] operands? True
// when the function has a recognizable prologue and no BP/SP value escapes
// anywhere in it -- the same test the promote passes apply.
// -------------------------------------------------------------------
bool promote_frame_unaliased(AsmNode *n) {
    AsmNode *entry = n;
    while (entry && !is_function_entry(entry)) entry = entry->prev;
    if (!entry) return false;
    AsmNode *last = function_last(entry);
    return !function_frame_escapes(entry->next, last) &&
           prologue_anchor(entry->next, last->next) != NULL;
}
