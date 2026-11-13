#include "v32opt.h"
#include <stdint.h>

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

// A call whose callee is known (';@fn F clobbers=...') to leave R1..R10 alone needs no save around it.
static bool cold_call_may_clobber(const AsmNode *call) {
    const FnHints *h = hints_fn_of_call(call);
    return !(h && h->clobbers_known && (h->clobbers & 0x7FEu) == 0);
}

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
    return n && (n->mnemonic[0] == 'J' || n->mnemonic[0] == 'j') &&
           (str_case_eq(n->mnemonic, "JMP") || str_case_eq(n->mnemonic, "JT") ||
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
        if (name_eq(lbl, name)) return true;
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
            if ((!except || !name_eq(target, except)) &&
                label_in_range(target, from, to)) return true;
        }
        if (n == scope_last) break;
    }
    return false;
}

// -------------------------------------------------------------------
// Helper: function boundaries
// -------------------------------------------------------------------
// A function's entry label: "__function_<name>" (not "..._return"), or the
// Lua compiler's "__global_scope_initialization".
static bool is_function_entry(const AsmNode *n) {
    if (!n || n->type != OP_LABEL) return false;
    char lbl[128];
    get_label_name(n, lbl, sizeof(lbl));
    if (str_case_eq(lbl, "__global_scope_initialization")) return true;
    return strncmp(lbl, "__function_", 11) == 0 && !is_fn_return_label(n);
}

// -------------------------------------------------------------------
// Where a function ends. BUG FIX (peephole-lua-checks miscompile on v32lua output, and the same
// mistake in every promote pass): a function used to run from its entry label to the next
// "__function_" / "__literal_" / "__global_" label, with "__global_scope_initialization" itself
// NOT a boundary. v32lua output broke that in three ways:
//   - every top-level control-flow label is "__global_<something>" ("__global_for_start_0",
//     "__global_num_3", ...; see is_global_scope_internal_label in dce.c), so the top-level
//     code's function stopped at its first loop or coercion guard;
//   - the function placed right before "__global_scope_initialization" (main, defined at the
//     top) ran on into it, up to that first "__global_" label: main's frame and the top-level
//     frame were one "function" -- both number their slots [BP-1], [BP-2], ... -- and
//     peephole-lua-checks deleted the top-level loop's step and counter stores because main's
//     loop no longer read those offsets;
//   - a function does not end at its own "__<stem>_skip:" label, where the code that defined it
//     (v32lua's skip-around layout, dce.c) resumes in the ENCLOSING frame.
// Now a function ends before: another function entry (including "__global_scope_initialization"),
// a "__literal_" label, its own "__<stem>_skip" label, or a label that some CALL targets (a
// subroutine with its own return, such as v32lua's runtime library after the top-level code).
// "__global_*" labels other than the scope initialization are ordinary labels of their function.
//
// A range that stops while the frame goes on is INCOMPLETE (function_complete): a nested function
// definition inside it ("JMP __inner_skip / __function_inner: ..."), or code falling into the next
// label. The passes that reason about a whole frame (promotion, the slot analyses of
// peephole-lua-checks, frame cleanup, loop-strength's register choice) leave such a function alone.
// -------------------------------------------------------------------
static char    **g_call_tgt   = NULL;      // sorted names of direct CALL targets
static int       g_call_tgt_n = 0;
static AsmNode  *g_bounds_head = NULL;     // the list they were collected from

static int cmp_cstr(const void *a, const void *b) { return strcmp(*(char * const *)a, *(char * const *)b); }

// Every label with the number of times its name is mentioned anywhere (instructions, data and
// preprocessor directives; not inside a string, not in a comment): see promote_inner_labels_private().
static void bounds_for(const AsmNode *n);

typedef struct { char *name; int refs; int inside; } LabelCount;
static LabelCount *g_lc   = NULL;
static int         g_lc_n = 0;

static int cmp_lc(const void *a, const void *b) { return strcmp(((const LabelCount *)a)->name, ((const LabelCount *)b)->name); }

static LabelCount *lc_find(const char *name) {
    if (!g_lc) return NULL;
    LabelCount key = { (char *)name, 0, 0 };
    return bsearch(&key, g_lc, (size_t)g_lc_n, sizeof(LabelCount), cmp_lc);
}

static bool lc_char(int c) { return isalnum(c) || c == '_' || c == '.' || c == '$' || c == '~'; }

// Adds `delta` to field (0: refs, 1: inside) of every label named in n's text.
static void lc_count(const AsmNode *n, int field) {
    const char *p = n->raw;
    bool in_str = false;
    while (*p) {
        if (in_str) { if (*p == '\\' && p[1]) p++; else if (*p == '"') in_str = false; p++; continue; }
        if (*p == ';') break;
        if (*p == '"') { in_str = true; p++; continue; }
        if (lc_char((unsigned char)*p) && !isdigit((unsigned char)*p)) {
            const char *b = p;
            while (lc_char((unsigned char)*p)) p++;
            size_t len = (size_t)(p - b);
            if (len < 128) {
                char tok[128];
                memcpy(tok, b, len);
                tok[len] = '\0';
                LabelCount *l = lc_find(tok);
                if (l) { if (field) l->inside++; else l->refs++; }
            }
            continue;
        }
        p++;
    }
}

void promote_bounds_end(void) {
    for (int i = 0; i < g_call_tgt_n; i++) free(g_call_tgt[i]);
    free(g_call_tgt);
    g_call_tgt = NULL;
    g_call_tgt_n = 0;
    for (int i = 0; i < g_lc_n; i++) free(g_lc[i].name);
    free(g_lc);
    g_lc = NULL;
    g_lc_n = 0;
    g_bounds_head = NULL;
}

void promote_bounds_begin(AsmNode *head) {
    promote_bounds_end();
    int cap = 0;
    for (AsmNode *n = head; n; n = n->next) if (n->type == OP_CALL) cap++;
    g_call_tgt = malloc((size_t)(cap + 1) * sizeof(char *));
    for (AsmNode *n = head; n && g_call_tgt; n = n->next) {
        if (n->type != OP_CALL || !n->has_dst || n->dst_op.mode != MODE_IMMEDIATE) continue;
        char buf[128];
        safe_str_copy(buf, n->dst_op.raw, sizeof(buf));
        g_call_tgt[g_call_tgt_n++] = strdup(trim(buf));
    }
    if (g_call_tgt) qsort(g_call_tgt, (size_t)g_call_tgt_n, sizeof(char *), cmp_cstr);
    g_bounds_head = head;
}

// The label reference counts, built on first use (promote_labels_private) for the list of the
// last promote_bounds_begin().
static void lc_build(AsmNode *head) {
    int nl = 0;
    for (AsmNode *n = head; n; n = n->next) if (n->type == OP_LABEL) nl++;
    g_lc = calloc((size_t)nl + 1, sizeof(LabelCount));
    for (AsmNode *n = head; n && g_lc; n = n->next) {
        if (n->type != OP_LABEL) continue;
        char nm[128];
        get_label_name(n, nm, sizeof(nm));
        if (nm[0]) g_lc[g_lc_n++].name = strdup(nm);
    }
    if (g_lc) qsort(g_lc, (size_t)g_lc_n, sizeof(LabelCount), cmp_lc);
    for (AsmNode *n = head; n && g_lc; n = n->next)
        if (n->type != OP_LABEL && !n->debug_note) lc_count(n, 0);
}

// Is every label of [first, last] other than `except` referenced only from inside [first, last]?
// A label that code elsewhere jumps to (a shared epilogue: "JT R0, __b_tail" from function a), calls,
// or names in data is a second way into the range, with a stack the range's own prologue did not
// build: whatever assumes the prologue ran (a removed or added save, a removed frame) breaks it.
// The counts are those of promote_bounds_begin() (references are not edited by the passes that ask).
bool promote_labels_private(AsmNode *first, AsmNode *last, const AsmNode *except) {
    bounds_for(first);
    if (!g_lc) lc_build(g_bounds_head);
    if (!g_lc) return false;
    for (int i = 0; i < g_lc_n; i++) g_lc[i].inside = 0;
    for (AsmNode *n = first; n; n = n->next) {
        if (n->type != OP_LABEL && !n->debug_note) lc_count(n, 1);
        if (n == last) break;
    }
    for (AsmNode *n = first; n; n = n->next) {
        if (n->type == OP_LABEL && n != except) {
            char nm[128];
            get_label_name(n, nm, sizeof(nm));
            LabelCount *l = lc_find(nm);
            if (!l || l->inside != l->refs) return false;
        }
        if (n == last) break;
    }
    return true;
}

// The table must describe the list `n` is in: a caller that did not call promote_bounds_begin()
// for it gets it built here (slow -- a walk to the head -- but never wrong).
static void bounds_for(const AsmNode *n) {
    const AsmNode *h = n;
    if (g_bounds_head) {
        // cheap check: the caller's begin() normally covers it
        return;
    }
    while (h->prev) h = h->prev;
    promote_bounds_begin((AsmNode *)h);
}

static bool is_call_target(const char *lbl) {
    if (!g_call_tgt || g_call_tgt_n == 0) return false;
    const char *key = lbl;
    return bsearch(&key, g_call_tgt, (size_t)g_call_tgt_n, sizeof(char *), cmp_cstr) != NULL;
}

// The last node belonging to the function whose entry label is `entry`.
static AsmNode *function_last(AsmNode *entry) {
    bounds_for(entry);
    char ename[128], skip[160];
    get_label_name(entry, ename, sizeof(ename));
    skip[0] = '\0';
    if (strncmp(ename, "__function_", 11) == 0) snprintf(skip, sizeof(skip), "__%s_skip", ename + 11);
    AsmNode *last = entry;
    for (AsmNode *s = entry->next; s; s = s->next) {
        if (s->type == OP_LABEL) {
            char lbl[128];
            get_label_name(s, lbl, sizeof(lbl));
            if (is_function_entry(s) || strncmp(lbl, "__literal_", 10) == 0 ||
                (skip[0] && strcmp(lbl, skip) == 0) || is_call_target(lbl)) {
                break;
            }
        }
        last = s;
    }
    return last;
}

// Does the frame of the function [entry, last] end there? (See above.) The last instruction must
// be a RET or HLT, or a JMP back into the range; and no jump may leave the range for a label
// further on in the same code (a nested definition's "JMP __inner_skip" does).
static bool function_complete(AsmNode *entry, AsmNode *last) {
    AsmNode *lastins = NULL;
    for (AsmNode *n = entry->next; n; n = n->next) {
        if (n->type != OP_LABEL && n->type != OP_OTHER) lastins = n;
        if (n == last) break;
    }
    if (!lastins) return true;
    for (AsmNode *n = entry->next; n; n = n->next) {
        if (n->type == OP_JMP || n->type == OP_JT || n->type == OP_JF) {
            const Operand *op = (n->type == OP_JMP) ? &n->dst_op : &n->src_op;
            if (op->mode == MODE_IMMEDIATE) {
                char t[128];
                safe_str_copy(t, op->raw, sizeof(t));
                bool inside = false;
                for (AsmNode *q = entry->next; q; q = q->next) {
                    if (q->type == OP_LABEL) {
                        char l[128];
                        get_label_name(q, l, sizeof(l));
                        if (name_eq(l, trim(t))) { inside = true; break; }
                    }
                    if (q == last) break;
                }
                if (!inside) {
                    // a jump to a label AFTER the range: the frame continues there
                    for (AsmNode *q = last->next; q; q = q->next) {
                        if (q->type == OP_LABEL) {
                            char l[128];
                            get_label_name(q, l, sizeof(l));
                            if (name_eq(l, t)) return false;
                        }
                        if (is_function_entry(q) && q != last->next) break;   // the next function on is someone else's
                    }
                }
            }
        }
        if (n == last) break;
    }
    return lastins->type == OP_RET || lastins->type == OP_HLT || lastins->type == OP_JMP;
}

// The function a node belongs to: walk back to the nearest entry, stepping over the body of a
// nested function whose "__<stem>_skip" label is passed on the way (the code after it is the
// enclosing function's). NULL when there is none.
static AsmNode *enclosing_entry(AsmNode *n) {
    for (AsmNode *p = n; p; p = p->prev) {
        if (is_function_entry(p)) return p;
        if (p->type != OP_LABEL) continue;
        char lbl[128];
        get_label_name(p, lbl, sizeof(lbl));
        size_t len = strlen(lbl);
        if (len > 7 && strncmp(lbl, "__", 2) == 0 && strcmp(lbl + len - 5, "_skip") == 0) {
            char want[160];
            snprintf(want, sizeof(want), "__function_%.*s", (int)(len - 7), lbl + 2);
            AsmNode *q = p->prev;
            for (; q; q = q->prev) {
                if (q->type != OP_LABEL) continue;
                char l2[128];
                get_label_name(q, l2, sizeof(l2));
                if (strcmp(l2, want) == 0) break;
            }
            if (q) p = q;                      // continue before the nested function's entry
        }
    }
    return NULL;
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
// Compiler hints (hints.h): ';@fn <label> escapes=<list>' says exactly which
// frame words have their address taken. Without it a single "LEA R, [BP-N]"
// anywhere keeps EVERY slot of the function in memory (function_frame_escapes);
// with it only the listed slots stay, and the rest are promoted as usual.
//
//   g_fh   the hints of the function being processed (set by each pass as it
//          enters a function; NULL when there are none or no 'escapes=')
//
// The hint is only trusted as far as it can be checked: a frame address that
// is captured any way other than "LEA R, [BP+off]" of a LISTED word ("MOV R1,
// BP", an LEA of an unlisted word, anything with SP) still disables the whole
// function, exactly as without hints.
// -------------------------------------------------------------------
static const FnHints *g_fh = NULL;

static void set_fn_hints(AsmNode *first) {
    g_fh = NULL;
    AsmNode *e = first ? first->prev : NULL;
    while (e && e->type != OP_LABEL) e = e->prev;
    if (!e) return;
    char lbl[128];
    get_label_name(e, lbl, sizeof(lbl));
    const FnHints *h = hints_fn(lbl);
    if (h && h->escapes_known) g_fh = h;
}

// Does anything in [first, last] make slot promotion in this function unsafe?
static bool function_frame_blocks_promotion(AsmNode *first, AsmNode *last) {
    if (!g_fh) return function_frame_escapes(first, last);
    if (g_fh->escapes_all) return true;
    for (AsmNode *n = first; n; n = n->next) {
        if (sp_value_escapes(n)) return true;
        if (bp_value_escapes(n)) {
            bool listed_lea = n->type == OP_LEA && n->has_src && n->src_op.mode == MODE_INDIRECT &&
                              str_case_eq(n->src_op.reg, "BP") &&
                              hints_fn_slot_escapes(g_fh, n->src_op.offset);
            if (!listed_lea) return true;
        }
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
    int  cold_calls;             // ';@cold' CALLs in the region
    int  cold_cost;              // sum of the loop-nesting weights of those that can clobber a promoted register (each costs a store and a reload per register)
} RegionInfo;

// A cold block runs about one time in COLD_DIV of the pass through the code around it. Saving a register
// around a cold CALL costs 2 instructions there; that is charged against what the promotion saves.
#define COLD_DIV 16

// References of a slot net of what saving its register around the region's cold CALLs would cost.
static int eff_refs(const RegionInfo *ri, int idx) {
    int r = ri->refs[idx];
    if (r < 0 || ri->cold_calls == 0) return r;          // (negative: a volatile slot)
    return r - (2 * ri->cold_cost) / COLD_DIV;
}

// Only locals ([BP-N]). Parameters ([BP+N], N >= 2) are deliberately left
// alone: promoting one costs a load and, with the cleanup passes there are
// today, saves nothing (the register copy that replaces each read still
// executes). [BP+0] / [BP+1] are the saved BP and the return address.
static bool slot_promotable(int off) {
    return off <= -1 && off >= -SLOT_MAX_ABS && !hints_fn_slot_escapes(g_fh, off);
}

// The slots worth promoting in a region, in the order to try them. Without cold CALLs that is the old order
// (by offset); with them registers are scarcer than they were (a cold CALL no longer ends the stretch) and the
// heaviest slot goes first.
static int order_candidates(const RegionInfo *ri, int min_refs, int *out) {
    int n = 0;
    for (int idx = 0; idx < SLOT_COUNT; idx++)
        if (slot_promotable(idx - SLOT_BIAS) && eff_refs(ri, idx) >= min_refs) out[n++] = idx;
    if (ri->cold_calls > 0) {
        for (int i = 1; i < n; i++) {                                  // insertion sort, stable
            int v = out[i], j = i - 1;
            while (j >= 0 && eff_refs(ri, out[j]) < eff_refs(ri, v)) { out[j + 1] = out[j]; j--; }
            out[j + 1] = v;
        }
    }
    return n;
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
        if (n->type == OP_CALL && node_in_cold_block(n)) {
            ri->cold_calls++;
            if (cold_call_may_clobber(n)) ri->cold_cost += w;
        }
        if (node_in_cold_block(n)) w = 1;      // a rarely taken block does not run as often as its loop
        // (a PUSH/POP of a register is bookkeeping -- the prologue and
        // epilogue of a callee-save function, or a spill: liveness decides
        // whether the register is free, so they don't count as "used")
        if (n->type != OP_PUSH && n->type != OP_POP) {
            if (n->has_dst) note_reg(ri, &n->dst_op);
            if (n->has_src) note_reg(ri, &n->src_op);
        }
        // a volatile slot ('volatile int x;' local, hint ';@v') stays in memory: pushing its
        // count far below any threshold takes it out of every pass's candidates
        const int VOL_PENALTY = 1 << 24;
        // BUG FIX (non-convergence, -O2 -L lua): the save/reload a promotion put around a cold CALL is
        // not a use of the slot. Counted as two references inside a loop, promote-loops took the slot
        // into a fresh register -- whose own cold-CALL save the copy cleanup then folded straight back
        // into the original pair, so every fixed-point iteration redid and undid the same 8 changes.
        if (n->hints & NH_PROMOTE_SPILL) w = 0;
        if (n->has_dst && n->dst_op.mode == MODE_INDIRECT && str_case_eq(n->dst_op.reg, "BP") &&
            slot_promotable(n->dst_op.offset)) {
            ri->refs[n->dst_op.offset + SLOT_BIAS] += node_is_volatile(n) ? -VOL_PENALTY : w;
            ri->written[n->dst_op.offset + SLOT_BIAS] = true;
        }
        if (n->has_src && n->src_op.mode == MODE_INDIRECT && str_case_eq(n->src_op.reg, "BP") &&
            slot_promotable(n->src_op.offset)) {
            ri->refs[n->src_op.offset + SLOT_BIAS] += node_is_volatile(n) ? -VOL_PENALTY : w;
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
    int c = strcmp(x->name, y->name);
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
            int c = strcmp(labels[mid].name, key.name);
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
                int c = strcmp(labels[mid].name, key.name);
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
// A fresh push/pop costs 2 instructions per call: ask for this many more
// (weighted) references before spending one. Tuned on the stock-compiler
// corpus (any value from 1 to 6 landed within 0.15 points).
#define FRESH_SAVE_EXTRA      3
#define FRESH_SAVE_EXTRA_LOOP 1

// FrameInfo (and MAX_EPILOGUES) live in inc/promote.h: loop-strength uses them too.

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

// -------------------------------------------------------------------
// A fresh PUSH goes right after fi->entry_end, so it takes the word just
// below everything pushed so far, and moves everything the body pushes
// later one word down. "[SP+k]" is addressed from the top of the stack: it
// still means the same word as long as it stays among the words pushed
// after that point (0 <= k < d, d = words pushed since entry_end). A k >= d
// reaches the fresh save or above it -- v32cc's single "ISUB SP, N" for
// locals and arguments with "MOV [SP+k], x" stores: the store would
// overwrite the saved word, and the epilogue's POP would hand the caller the
// argument instead of its register. The stock compiler reserves its argument
// area with a second "ISUB SP, A" after its saves (k < A = d): safe.
//
// Walk the function from entry_end, following d through PUSH/POP and
// "ISUB/IADD SP, <integer>". Anything that makes d uncertain refuses:
//   - d differing between labels and jumps: every label and every jump must
//     see the same depth D0 (whatever is reached by a jump then arrives with
//     D0 as well; the stock compiler's argument area keeps D0 = A through the
//     whole body), and code after a JMP, HLT or epilogue starts again at D0;
//   - d != 0 at the start of an epilogue (fi->epi_first), where the fresh POP
//     goes;
//   - a computed jump; a RET that is not an epilogue's;
//   - POP BP/SP, d going negative, a non-integer or negative SP adjustment,
//     any other write of SP (CALL excepted: net zero);
//   - an SP value escape ("MOV R1, SP", "LEA R1, [SP+2]");
//   - a directive.
// A fresh save added later (entry_end moves onto it) sees the same walk: the
// earlier fresh PUSH sits above it, among the saves, not in d.
// -------------------------------------------------------------------
static bool sp_operands_clear_of_saves(const FrameInfo *fi, AsmNode *limit) {
    long d = 0, d0 = -1;                 // d0: the depth every label and jump sees (-1: none yet)
    bool lost = false;                   // after JMP/HLT/epilogue: only a label resumes the walk
    for (AsmNode *n = fi->entry_end->next; n && n != limit; n = n->next) {
        if (n->type == OP_OTHER) {
            if (is_directive_node(n)) return false;
            continue;
        }
        if (n->type == OP_LABEL) {
            if (lost) {
                if (d0 < 0) return false;
                d = d0;
                lost = false;
            }
            if (d0 < 0) d0 = d;
            else if (d != d0) return false;
            continue;
        }
        if (lost) continue;              // unreachable except through a label
        int e;
        for (e = 0; e < fi->n_epi; e++) if (n == fi->epi_first[e]) break;
        if (e < fi->n_epi) {
            if (d != 0) return false;
            while (n && n != limit && n->type != OP_RET) n = n->next;
            if (!n || n == limit) return false;
            lost = true;
            continue;
        }
        if (n->type == OP_RET) return false;
        if (sp_value_escapes(n)) return false;
        if (is_jump(n)) {
            char target[128];
            branch_target(n, target, sizeof(target));
            if (!target[0] || get_reg_index(target) >= 0 || strchr(target, '[')) return false;   // computed
            if (d0 < 0) d0 = d;
            else if (d != d0) return false;
            if (n->type == OP_JMP) { lost = true; continue; }
        }
        if (n->type == OP_HLT) { lost = true; continue; }
        const Operand *imm = n->has_src ? &n->src_op : NULL;
        bool sp_dst = n->has_dst && n->dst_op.mode == MODE_REG && str_case_eq(n->dst_op.reg, "SP");
        if (n->type == OP_PUSH) {
            d++;
        } else if (n->type == OP_POP) {
            if (sp_dst || (n->has_dst && n->dst_op.mode == MODE_REG && str_case_eq(n->dst_op.reg, "BP"))) return false;
            if (--d < 0) return false;
        } else if ((n->type == OP_ISUB || n->type == OP_IADD) && sp_dst) {
            if (!imm || imm->mode != MODE_IMMEDIATE || imm->is_float || !is_numeric_immediate(imm) ||
                imm->immediate < 0) return false;
            d += (n->type == OP_ISUB) ? imm->immediate : -(long)imm->immediate;
            if (d < 0) return false;
        } else if (n->type != OP_CALL && modifies_register(n, "SP")) {
            return false;
        }
        const Operand *ops[2] = { n->has_dst ? &n->dst_op : NULL, n->has_src ? &n->src_op : NULL };
        for (int i = 0; i < 2; i++) {
            if (!ops[i] || ops[i]->mode != MODE_INDIRECT || !str_case_eq(ops[i]->reg, "SP")) continue;
            if (ops[i]->offset < 0 || ops[i]->offset >= d) return false;
        }
    }
    return true;
}

static void frame_info(FrameInfo *fi, AsmNode *anchor, AsmNode *first, AsmNode *limit) {
    memset(fi, 0, sizeof(*fi));
    fi->entry_end = anchor;
    fi->lua = abi_caller_save();        // caller-save ABI (v32lua, v32cc): no register needs saving
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
    if (fi->can_save && !sp_operands_clear_of_saves(fi, limit)) fi->can_save = false;
    // BUG FIX (same kind as the frame-cleanup shared-tail one): a label of the function that is
    // reached from elsewhere -- another function jumping into this one's epilogue -- would run the
    // added POP on a stack that never got the PUSH.
    // (Decided on first use, by fi_can_save(): it needs the label reference table, which most
    // functions -- no fresh save wanted -- never have to pay for.)
    if (fi->can_save) {
        fi->lbl_pending = true;
        fi->lbl_first = first;
        fi->lbl_limit = limit;
    }
}

static bool fi_can_save(const FrameInfo *cfi) {
    FrameInfo *fi = (FrameInfo *)cfi;
    if (fi->can_save && fi->lbl_pending) {
        fi->lbl_pending = false;
        AsmNode *last = fi->lbl_first;
        while (last->next && last->next != fi->lbl_limit) last = last->next;
        if (!promote_labels_private(fi->lbl_first, last, NULL)) fi->can_save = false;
    }
    return fi->can_save;
}

// Make sure the function saves and restores Rr; false if it can't.
static bool frame_save_reg(FrameInfo *fi, int r, OptType t) {
    if (fi->lua || fi->saved[r]) return true;
    if (!fi_can_save(fi)) return false;
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
                if (is_saved || !allow_fresh || !fi_can_save(fi) || r < 4) continue;
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

// -------------------------------------------------------------------
// Debug records (SO-1, inc/srcmarks.h). A slot promoted to a register over [from, to): the ';@var' lines inside the region say
// the register; a variable declared earlier is moved by a ';@home' where the register takes over (after the load) and one where
// the slot is the truth again (after the flush; "optimized-out" when the region wrote it and nothing keeps it). All of them are
// comment lines, written only when the input had ';@var' lines and the function has a variable in that slot.
// -------------------------------------------------------------------
static void home_inside(AsmNode *from, AsmNode *to, int off, const char *reg_name) {
    varloc_rewrite(from, to, off, reg_name);
}

static bool home_wanted(AsmNode *func_first, AsmNode *func_limit, int off) {
    return varloc_present() && varloc_has(func_first, func_limit, off);
}

// the node that ends a region's use of the register: back to the slot, or gone
static AsmNode *home_leave(int off, const char *reg_name, bool slot_is_true) {
    return slot_is_true ? varloc_home(off, reg_name, false) : varloc_gone(reg_name);
}

// -------------------------------------------------------------------
// Cold CALLs (';@cold' blocks, hints.h). A loop or segment whose only CALLs lie in
// blocks the compiler marked as rarely taken is promoted anyway: every promoted
// register is stored to its home slot right before such a CALL and reloaded right
// after it, so the cost lands on the cold path only. This needs no help from the
// hint to be CORRECT -- the register holds the slot's value again as soon as the
// reload has run, whatever the callee did (callee-save or caller-save) -- the hint
// only says that it is worth it. What keeps it sound:
//   - the callee cannot reach the slot: the escape checks made for the whole
//     function already ruled out a pointer into the frame;
//   - the callee may leave anything in the promoted register (it was chosen to be
//     dead from the region's start on, which includes the callee's own reads and
//     writes, see pick_register) so the reload cannot destroy a returned value;
//   - the store before and the reload after the CALL are adjacent to it, so no
//     path can see the register without the slot or the other way round.
// A callee with `;@fn F clobbers=...` that does not list the register needs neither.
// -------------------------------------------------------------------
static bool is_cold_call(const AsmNode *n) {
    return n && n->type == OP_CALL && node_in_cold_block(n);
}

static bool callee_keeps_reg(const AsmNode *call, int reg) {
    const FnHints *h = hints_fn_of_call(call);
    return h && h->clobbers_known && !((h->clobbers >> reg) & 1u);
}

// Is `n` a CALL that ends a CALL-free stretch? (A cold one does not.)
static bool is_barrier_call(const AsmNode *n) {
    return n && str_case_eq(n->mnemonic, "CALL") && !is_cold_call(n);
}

// Around every cold CALL in [from, to): store `reg` to `slot` before it, reload after it.
// Run AFTER rewrite_slot (which must not see these two). Returns the number of nodes added.
static int wrap_cold_calls(AsmNode *from, AsmNode *to, const char *slot, const char *reg_name, int reg, OptType t) {
    int added = 0;
    AsmNode *next;
    for (AsmNode *n = from; n && n != to; n = next) {
        next = n->next;
        if (!is_cold_call(n) || callee_keeps_reg(n, reg)) continue;
        // a register nobody reads after the call (it is rewritten first, on every path) needs no saving
        if (n->next && reg_dead_from(n->next, reg_name)) continue;
        AsmNode *store = make_mov(slot, reg_name);
        link_before(n, store);
        AsmNode *load = make_mov(reg_name, slot);
        link_after(n, load);
        srcmarks_inherit(store, n);
        srcmarks_inherit(load, n);
        store->hints |= NH_COLD_BLK | NH_PROMOTE_SPILL;
        load->hints  |= NH_COLD_BLK | NH_PROMOTE_SPILL;
        added += 2;
        char dbg[160];
        snprintf(dbg, sizeof(dbg), "PROMOTE: %s is saved to %s around this cold CALL (only the slow path pays)", reg_name, slot);
        insert_debug_comment(store, t, dbg);
    }
    return added;
}


// ===================================================================
// Pass: Stack Slot to Register Promotion (Scalar Replacement)
// ===================================================================
int pass_promote_stack_slots(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;
    liveness_begin(head);
    if (head) promote_bounds_begin(head);

    while (curr) {
        if (!is_function_entry(curr)) { curr = curr->next; continue; }

        AsmNode *end_of_func = function_last(curr);
        AsmNode *first = curr->next;
        AsmNode *limit = end_of_func->next;
        if (!function_complete(curr, end_of_func)) { curr = limit; continue; }
        set_fn_hints(first);

        // Phase A: Analyze Function Safety & Register Liveness
        bool is_leaf = true;
        for (AsmNode *n = first; n && n != limit; n = n->next) {
            if (is_barrier_call(n)) { is_leaf = false; break; }
        }
        AsmNode *anchor = NULL;
        if (is_leaf && !function_frame_blocks_promotion(first, end_of_func)) {
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

            int order[SLOT_COUNT];
            int norder = order_candidates(&ri, MIN_REFS_FUNC, order);
            for (int oi = 0; oi < norder; oi++) {
                int idx = order[oi];
                int off = idx - SLOT_BIAS;

                // TRIGGER CAP: gate this offset's whole promotion (every
                // rewritten reference plus its load) as one atomic unit.
                if (!trigger_allowed()) continue;

                bool fresh;
                int r = pick_register(fi.entry_end->next, &ri, &fi,
                                      eff_refs(&ri, idx) >= MIN_REFS_FUNC + FRESH_SAVE_EXTRA, &fresh);
                if (r < 0) continue;
                char reg_name[16], slot[24];
                snprintf(reg_name, sizeof(reg_name), "R%d", r);
                slot_text(off, slot, sizeof(slot));
                if (fresh) frame_save_reg(&fi, r, OPT_PROMOTE_LEAF);

                optimizations += rewrite_slot(first, limit, off, reg_name, OPT_PROMOTE_LEAF);
                home_inside(first, limit, off, reg_name);        // the whole function: every record of the slot says the register
                optimizations += wrap_cold_calls(first, limit, slot, reg_name, r, OPT_PROMOTE_LEAF);

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
    if (head) promote_bounds_begin(head);

    while (curr) {
        if (!is_function_entry(curr)) { curr = curr->next; continue; }

        AsmNode *end_of_func = function_last(curr);
        AsmNode *func_first = curr->next;
        AsmNode *func_limit = end_of_func->next;
        if (!function_complete(curr, end_of_func)) { curr = func_limit; continue; }
        set_fn_hints(func_first);

        // GUARD: function-wide frame-escape check -- see the file header.
        // CALL segmentation doesn't change this; it's unrelated to leafness.
        AsmNode *anchor = NULL;
        if (!function_frame_blocks_promotion(func_first, end_of_func)) {
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
                   !is_barrier_call(seg_limit)) {
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

            int order[SLOT_COUNT];
            int norder = order_candidates(&ri, MIN_REFS_FUNC, order);
            for (int oi = 0; oi < norder; oi++) {
                int idx = order[oi];
                int off = idx - SLOT_BIAS;

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
                                      eff_refs(&ri, idx) >= MIN_REFS_FUNC + FRESH_SAVE_EXTRA, &fresh);
                if (r < 0) continue;
                char reg_name[16], slot[24];
                snprintf(reg_name, sizeof(reg_name), "R%d", r);
                slot_text(off, slot, sizeof(slot));
                if (fresh) {
                    frame_save_reg(&fi, r, OPT_PROMOTE_REGS);
                    if (is_first_segment) load_at = fi.entry_end->next;   // the push went in ahead of it
                }

                bool want_home = home_wanted(func_first, func_limit, off);
                home_inside(seg_start, seg_limit, off, reg_name);
                optimizations += rewrite_slot(seg_start, seg_limit, off, reg_name, OPT_PROMOTE_REGS);

                // A write must reach the stack slot before the CALL that ends
                // the segment if anything reads the slot afterwards (the next
                // segment reloads it). The last segment ends in RET, where the
                // frame dies: no flush. A slot nobody else reads needs none.
                char dbg[160];
                bool flushed = false;
                if (ri.written[idx] && seg_limit && str_case_eq(seg_limit->mnemonic, "CALL") &&
                    slot_read_outside(func_first, end_of_func, seg_start, seg_limit, off)) {
                    AsmNode *store = make_mov(slot, reg_name);
                    link_before(seg_limit, store);
                    flushed = true;
                    optimizations++;
                    snprintf(dbg, sizeof(dbg),
                             "PROMOTE-REGS: flush %s back to %s before the CALL that ends this segment (next segment reloads independently)",
                             reg_name, slot);
                    insert_debug_comment(store, OPT_PROMOTE_REGS, dbg);
                }

                // cold CALLs inside the segment (after the flush above, which keeps the register live)
                optimizations += wrap_cold_calls(seg_start, seg_limit, slot, reg_name, r, OPT_PROMOTE_REGS);

                // the slot is the truth again after the segment: a record for the CALL that ends it (written before the load's
                // is placed: it goes right before the CALL, after the flush)
                if (want_home && seg_limit && seg_limit != func_limit && str_case_eq(seg_limit->mnemonic, "CALL")) {
                    AsmNode *leave = home_leave(off, reg_name, flushed || !ri.written[idx]);
                    if (leave) link_before(seg_limit, leave);
                }

                // The load is needed only if some path reads the register
                // before writing it (asked AFTER the flush above, which is a
                // read of the register too).
                if (reg_dead_from(load_at, reg_name)) {
                    snprintf(dbg, sizeof(dbg),
                             "PROMOTE-REGS: %s lives in %s (always written before read: no load)",
                             slot, reg_name);
                    insert_debug_comment(load_at->prev, OPT_PROMOTE_REGS, dbg);
                    if (want_home) {
                        AsmNode *enter = varloc_home(off, reg_name, true);
                        if (enter) { if (is_first_segment) link_after(fi.entry_end, enter); else link_before(seg_start, enter); }
                    }
                } else {
                    AsmNode *load = make_mov(reg_name, slot);
                    if (is_first_segment) link_after(fi.entry_end, load);
                    else                  link_before(seg_start, load);
                    if (want_home) {
                        AsmNode *enter = varloc_home(off, reg_name, true);
                        if (enter) { if (is_first_segment) link_after(load, enter); else link_before(seg_start, enter); }
                    }
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

// -------------------------------------------------------------------
// promote-loops across CALLs (-fpromote-loops-calls, L4 of doc/lua-loops.md)
//
// A hot CALL used to end every loop. It does not have to when the callee says, with ';@fn F clobbers=...',
// which registers it can change: v32lua computes that list (tools/check_clobbers.py) for its runtime
// routines over everything they can reach -- a CALL contributes its callee's set, an indirect call or
// jump, an unbalanced stack or a callee it cannot analyse leave the routine without a list -- and leaves
// out the registers a routine saves and restores. So a register in none of the lists of a loop's CALLs
// keeps its value across them. A loop qualifies when every hot CALL in it is a DIRECT call to a function
// with such a list; the registers in those lists are not candidates. A CALL through a register, to
// __builtin_exec (user functions: no list) or to any routine without a list keeps blocking the loop.
// The function's own list, if it has one, is a promise to ITS callers: a register it says it keeps is
// not used either (a caller-save promotion adds no save, so the function would start to clobber it).
//
// V32OPT_PROMOTE_CALLS_DEBUG=1: one line per loop with hot CALLs (stderr): promoted, or why not.
// V32OPT_PROMOTE_CALLS_CHECK=1: for every promotion across hot CALLs, walk each callee's code (direct
// callees included) and report any write of the register outside a PUSH/POP save -- a list that
// disagrees with the code it describes.
// -------------------------------------------------------------------
static AsmNode *find_label_global(AsmNode *head, const char *name) {
    char lbl[128];
    for (AsmNode *n = head; n; n = n->next) {
        if (n->type != OP_LABEL) continue;
        get_label_name(n, lbl, sizeof(lbl));
        if (name_eq(lbl, name)) return n;
    }
    return NULL;
}

typedef struct { AsmNode *n; int saved; } XWalk;     // saved: how many PUSHes of the register are still open

// 0: no write of `reg` outside a save on any path from `callee`; 1: a write (*where = the node); 2: not followed
static int xcheck_callee(AsmNode *head, const char *callee, const char *reg, const AsmNode **where) {
    AsmNode *entry = find_label_global(head, callee);
    if (!entry) return 2;
    int cap = 1 << 16, sp = 0, nseen = 0, seen_cap = 1 << 19;
    XWalk *stack = malloc((size_t)cap * sizeof(XWalk));
    XWalk *seen  = calloc((size_t)seen_cap, sizeof(XWalk));
    if (!stack || !seen) { free(stack); free(seen); return 2; }
    int result = 0, steps = 0;
    stack[sp++] = (XWalk){ entry->next, 0 };
    while (sp > 0 && result == 0) {
        XWalk w = stack[--sp];
        for (AsmNode *n = w.n; n; n = n->next) {
            if (++steps > 4000000) { result = 2; break; }
            if (n->type == OP_OTHER || n->type == OP_LABEL) continue;
            // visited (node, saved)?
            unsigned hsh = (unsigned)(((uintptr_t)n >> 4) * 2654435761u + (unsigned)w.saved) & (unsigned)(seen_cap - 1);
            bool dup = false;
            while (seen[hsh].n) {
                if (seen[hsh].n == n && seen[hsh].saved == w.saved) { dup = true; break; }
                hsh = (hsh + 1) & (unsigned)(seen_cap - 1);
            }
            if (dup) break;
            if (++nseen > seen_cap / 2) { result = 2; break; }
            seen[hsh] = (XWalk){ n, w.saved };

            bool on_reg = n->has_dst && n->dst_op.mode == MODE_REG && str_case_eq(n->dst_op.reg, reg);
            if (n->type == OP_PUSH) { if (on_reg && w.saved < 64) w.saved++; continue; }
            if (n->type == OP_POP && on_reg && w.saved > 0) { w.saved--; continue; }
            if (n->type == OP_RET || n->type == OP_HLT) break;
            if (n->type == OP_JMP || n->type == OP_JT || n->type == OP_JF || n->type == OP_CALL) {
                const Operand *t = (n->type == OP_JT || n->type == OP_JF) ? &n->src_op : &n->dst_op;
                if (t->mode != MODE_IMMEDIATE) {
                    if (w.saved > 0) break;                  // inside a save of the register: whatever runs there gets it back
                    result = 2; *where = n; break;           // (when control comes back through the POP -- the stack discipline
                }                                            //  tools/check_clobbers.py assumes as well)
                AsmNode *tl = find_label_global(head, t->raw);
                if (!tl) { result = 2; *where = n; break; }
                if (sp >= cap) { result = 2; break; }
                stack[sp++] = (XWalk){ tl->next, w.saved };
                if (n->type == OP_JMP) break;
                continue;                                    // JT/JF fall through; CALL returns here
            }
            if (w.saved == 0 && modifies_register(n, reg)) { result = 1; *where = n; break; }
        }
    }
    free(stack); free(seen);
    return result;
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
    if (head) promote_bounds_begin(head);

    // per-function caches: boundaries, escape check, and the frame (saved registers)
    AsmNode *bounds_entry = NULL, *bounds_last = NULL;
    bool     bounds_complete = false;
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
        AsmNode *entry = enclosing_entry(curr);
        AsmNode *func_first, *func_last;
        if (entry) {
            // (per function, not per loop: what this pass adds -- loads, stores, saves -- goes inside the
            // range and never adds a label or a jump, so neither answer changes while it works)
            if (entry != bounds_entry) {
                bounds_entry    = entry;
                bounds_last     = function_last(entry);
                bounds_complete = function_complete(entry, bounds_last);
            }
            func_first = entry->next;
            func_last  = bounds_last;
            if (!bounds_complete) { curr = curr->next; continue; }
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
            set_fn_hints(func_first);
            cached_escapes = function_frame_blocks_promotion(func_first, func_last);
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
                if (name_eq(target, start_lbl)) { end_jmp = scan; break; }
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
                if (name_eq(target, exit_lbl)) { exit_shared = true; break; }
            }
            if (n == func_last) break;
        }
        if (exit_shared) { curr = curr->next; continue; }

        // Phase A: Safety Scan across the Loop Body
        bool loop_safe = true;
        unsigned hot_clob = 0;                       // registers the loop's hot CALLs may change (promote-loops-calls)
        int hot_calls = 0;
        const char *refuse_why = NULL;
        char refuse_callee[128] = "";
        for (AsmNode *n = curr->next; n && n != end_jmp; n = n->next) {
            // Rule 1: No CALLs, RETs, or HLTs inside the loop -- except CALLs that lie in a
            // ';@cold' block: those are wrapped in a store/reload of each promoted register --
            // and (promote-loops-calls) direct CALLs to a function whose ';@fn clobbers=' is known
            if (is_barrier_call(n)) {
                const FnHints *h = config.opt_promote_loops_calls ? hints_fn_of_call(n) : NULL;
                if (h && h->clobbers_known) { hot_clob |= h->clobbers; hot_calls++; continue; }
                loop_safe = false;
                if (!n->has_dst || n->dst_op.mode != MODE_IMMEDIATE) refuse_why = "call-through-register";
                else {
                    safe_str_copy(refuse_callee, n->dst_op.raw, sizeof(refuse_callee));
                    refuse_why = name_eq(trim(refuse_callee), "__builtin_exec") ? "exec" : "unhinted";
                }
                break;
            }
            if (str_case_eq(n->mnemonic, "RET") ||
                str_case_eq(n->mnemonic, "HLT")) { loop_safe = false; refuse_why = "ret-hlt"; break; }
            // Rule 2 (the whole function was checked above): no frame writes
            // by name either
            // Rule 3: Verify all branches jump inside loop, to start, or to exit
            if (is_jump(n)) {
                char target[128];
                branch_target(n, target, sizeof(target));
                if (!name_eq(target, start_lbl) && !name_eq(target, exit_lbl) &&
                    !label_in_range(target, curr->next, end_jmp)) { loop_safe = false; refuse_why = "branch"; break; }
            }
        }
        bool calls_dbg = getenv("V32OPT_PROMOTE_CALLS_DEBUG") != NULL;
        if (!loop_safe) {
            if (calls_dbg && refuse_why && strncmp(refuse_why, "ret", 3) && strcmp(refuse_why, "branch"))
                fprintf(stderr, "promote-loops-calls: %s refused %s %s\n", start_lbl, refuse_why, refuse_callee);
            curr = curr->next;
            continue;
        }

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
            if (!slot_promotable(off) || eff_refs(&ri, idx) < MIN_REFS_LOOP) continue;
            cand[ncand].idx = idx;
            cand[ncand].weight = eff_refs(&ri, idx);
            ncand++;
        }
        qsort(cand, (size_t)ncand, sizeof(SlotCand), cmp_cand);

        bool enclosed = ncand > 0 && region_reentered(func_first, func_last, curr, body_end);
        int promoted = 0;
        // registers the hot CALLs may change are not candidates; nor, in a function that has its own
        // ';@fn clobbers=' list, a register that list says the function keeps (a caller-save promotion adds
        // no save: the function would start to change it behind its callers' backs)
        for (int r = 0; r < 16; r++) if ((hot_clob >> r) & 1u) ri.reg_used[r] = true;
        if (entry) {
            char elbl[128];
            get_label_name(entry, elbl, sizeof(elbl));
            const FnHints *own = hints_fn(elbl);
            if (own && own->clobbers_known && fi.lua)
                for (int r = 0; r < 16; r++) if (!((own->clobbers >> r) & 1u)) ri.reg_used[r] = true;
        }
        int promoted_calls_regs[SLOT_COUNT], n_pcr = 0;
        int free_regs = 0;                           // (debug: candidates left before liveness has its say)
        for (int r = PROMOTE_FIRST_REG; r <= PROMOTE_LAST_REG; r++) if (!ri.reg_used[r]) free_regs++;
        for (int c = 0; c < ncand; c++) {
            int idx = cand[c].idx;
            int off = idx - SLOT_BIAS;

            // TRIGGER CAP: gate this offset's whole promotion (preheader
            // load, exit store, every rewritten reference) as one unit.
            if (!trigger_allowed()) continue;

            bool fresh;
            int r = pick_register(curr, &ri, &fi, eff_refs(&ri, idx) >= MIN_REFS_LOOP + FRESH_SAVE_EXTRA_LOOP, &fresh);
            if (r < 0) continue;
            char reg_name[16], slot[24];
            snprintf(reg_name, sizeof(reg_name), "R%d", r);
            slot_text(off, slot, sizeof(slot));
            if (fresh) frame_save_reg(&fi, r, OPT_PROMOTE_LOOPS);

            // Rewrite all references inside the loop body to the register
            bool want_home = home_wanted(func_first, func_last->next, off);
            home_inside(curr->next, body_end, off, reg_name);
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

            // Cold CALLs in the loop: save the register around them (after the exit store above
            // exists: it is a read of the register that keeps it live out of the loop)
            optimizations += wrap_cold_calls(curr->next, body_end, slot, reg_name, r, OPT_PROMOTE_LOOPS);

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
            if (want_home) {
                // after the loop the slot is the truth again (the exit store), unless the loop wrote it and nothing keeps it
                AsmNode *leave = home_leave(off, reg_name, store != NULL || !ri.written[idx]);
                if (leave) link_after(store ? store : exit_label_node, leave);
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
                if (want_home) { AsmNode *enter = varloc_home(off, reg_name, true); if (enter) link_before(curr, enter); }
            } else {
                AsmNode *load = make_mov(reg_name, slot);
                link_before(curr, load);
                if (want_home) { AsmNode *enter = varloc_home(off, reg_name, true); if (enter) link_before(curr, enter); }
                snprintf(dbg, sizeof(dbg),
                         "PROMOTE-LOOPS: preheader load of %s into %s (runs once, before %s)",
                         slot, reg_name, start_lbl);
                insert_debug_comment(load, OPT_PROMOTE_LOOPS, dbg);
            }
            promoted++;
            if (hot_calls && n_pcr < SLOT_COUNT) promoted_calls_regs[n_pcr++] = r;
        }

        if (hot_calls && calls_dbg)
            fprintf(stderr, "promote-loops-calls: %s %s %d slot(s) across %d hot CALL(s), %d register(s) not excluded\n", start_lbl,
                    promoted ? "promoted" : (ncand ? (free_regs ? "refused no-register(liveness)" : "refused no-register(excluded)") : "refused no-candidate"),
                    promoted, hot_calls, free_regs);
        if (hot_calls && n_pcr && getenv("V32OPT_PROMOTE_CALLS_CHECK")) {
            for (AsmNode *n = curr->next; n && n != end_jmp; n = n->next) {
                if (!is_barrier_call(n)) continue;
                char callee[128];
                safe_str_copy(callee, n->dst_op.raw, sizeof(callee));
                for (int k = 0; k < n_pcr; k++) {
                    char rn[8];
                    snprintf(rn, sizeof(rn), "R%d", promoted_calls_regs[k]);
                    const AsmNode *where = NULL;
                    int x = xcheck_callee(head, trim(callee), rn, &where);
                    fprintf(stderr, "promote-loops-calls-check: %s %s %s %s%s%s\n", start_lbl, trim(callee), rn,
                            x == 0 ? "ok" : x == 1 ? "WRITES" : "not-followed", where ? " at: " : "", where ? where->raw : "");
                }
            }
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
    AsmNode *entry = enclosing_entry(n);
    if (!entry) return false;
    AsmNode *last = function_last(entry);
    bool inside = false;
    for (AsmNode *q = entry->next; q; q = q->next) { if (q == n) { inside = true; break; } if (q == last) break; }
    if (!inside || !function_complete(entry, last)) return false;
    return !function_frame_escapes(entry->next, last) &&
           prologue_anchor(entry->next, last->next) != NULL;
}

// -------------------------------------------------------------------
// For other passes (peephole-lua-checks): the function a node belongs to.
// -------------------------------------------------------------------
bool promote_function_entry(const AsmNode *n) { return is_function_entry(n); }
AsmNode *promote_function_last(AsmNode *entry) { return function_last(entry); }
bool promote_function_complete(AsmNode *entry) { return function_complete(entry, function_last(entry)); }

bool promote_bp_value_escapes(AsmNode *n) { return bp_value_escapes(n); }

// ---- for loop-strength (src/peephole/loop_strength.c): the register rules of promote-loops ----

// The frame of the function `n` sits in, as promote-loops sees it. False when no register may
// be used there at all (no recognisable prologue outside -L lua).
bool promote_loop_frame(FrameInfo *fi, AsmNode *n) {
    AsmNode *entry = enclosing_entry(n);
    AsmNode *an = NULL, *last = NULL;
    if (entry) {
        last = function_last(entry);
        bool inside = false;
        for (AsmNode *q = entry->next; q; q = q->next) { if (q == n) { inside = true; break; } if (q == last) break; }
        if (!inside || !function_complete(entry, last)) return false;
        an = prologue_anchor(entry->next, last->next);
    }
    if (an) {
        frame_info(fi, an, entry->next, last->next);
        return true;
    }
    memset(fi, 0, sizeof(*fi));
    fi->lua = (config.lang_mode == LANG_LUA);
    return fi->lua;
}

bool promote_frame_save_reg(FrameInfo *fi, int r, OptType t) { return frame_save_reg(fi, r, t); }

// pick_register() for the region [from, last]: a register R1..R10 not mentioned there (PUSH/POP
// included) nor in `exclude` (bit n = Rn), dead at `from`, saved by the function already or (with
// *fresh set) one that frame_save_reg() can still add. -1 if none.
int promote_pick_loop_register(AsmNode *from, AsmNode *last, const FrameInfo *fi, unsigned exclude, bool *fresh) {
    RegionInfo ri;
    scan_region(from, last->next, NULL, &ri);
    for (AsmNode *n = from; n && n != last->next; n = n->next) {
        if (n->type != OP_PUSH && n->type != OP_POP) continue;
        if (n->has_dst) note_reg(&ri, &n->dst_op);
        if (n->has_src) note_reg(&ri, &n->src_op);
    }
    for (int r = 0; r < 16; r++) if (exclude & (1u << r)) ri.reg_used[r] = true;
    return pick_register(from, &ri, fi, true, fresh);
}
bool promote_sp_value_escapes(AsmNode *n) { return sp_value_escapes(n); }
