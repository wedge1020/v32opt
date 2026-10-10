#include "v32opt.h"

// ===================================================================
// PEEPHOLE: Jump Chain Elimination
//
// Every JMP, JT or JF whose target label leads straight to another
// unconditional "JMP <label>" is retargeted to the END of the chain,
// wherever in the file the jump sits:
//
//       JT  R0, mid                   JT  R0, final
//       ...                           ...
//       JMP mid             ->        JMP final
//       ...                           ...
//     mid:                          mid:
//       JMP final                     (removed -- see below)
//
// A multi-hop chain (mid -> mid2 -> final) is followed to its end in
// one step. "Leads straight to" means: the first thing after the label
// is the JMP, give or take further labels, comments and blank lines
// (any directive ends the search: data means the label marks data, and
// a %include could hide code). A chain whose last hop is a computed jump
// ("JMP R0", "JMP 0x1000") stops at the last label hop; a chain that
// loops back on itself ("L: JMP L", or L1 -> L2 -> L1) is left alone,
// and so is a target label that is defined more than once.
//
// Retargeting a conditional branch is exact: the taken path used to
// reach the final label through the intermediate JMP, and the
// fall-through path is unchanged.
//
// INTERMEDIATE JMP REMOVAL: once the last user of a hop's label has
// been retargeted, the intermediate "JMP <next>" is deleted -- but only
// if NOTHING can still reach it:
//   (a) no label in the run of labels directly above it is referenced
//       anywhere in the program text (any operand -- MOV R0, mid takes
//       its address -- and any directive, e.g. "pointer mid"), and
//   (b) nothing can FALL INTO it: the preceding instruction is an
//       unconditional transfer (JMP/RET/HLT). The start of the file is
//       the boot entry point, so a JMP there is always reachable.
// Removal cascades along the chain (mid's JMP going away can make the
// next hop's label unreferenced in turn).
//
// Before this rewrite only the shape "JMP L1 / L1: JMP L2" (the jump
// directly above its own target) was handled, one hop per fixed-point
// iteration; jumps from anywhere else were never retargeted.
//
// TRIGGER CAP: one retargeted jump (together with whatever intermediate
// JMPs that retarget makes removable) is ONE transform = one slot.
// ===================================================================

// -------------------------------------------------------------------
// Label table: every label in the program, with a count of how many
// places in the program text reference it. Rebuilt on each call to the
// pass (labels never move while it runs), sorted for bsearch.
// -------------------------------------------------------------------
typedef struct {
    char     name[128];
    AsmNode *node;        // the label's own node
    int      refs;        // references anywhere in the program text
    bool     duplicate;   // defined more than once: never resolve
} ChainLabel;

static ChainLabel *g_lbl       = NULL;
static int         g_lbl_count = 0;

static int cmp_chain_label(const void *a, const void *b)
{
    return strcasecmp(((const ChainLabel *)a)->name, ((const ChainLabel *)b)->name);
}

static ChainLabel *find_label(const char *name)
{
    if (!name || !name[0] || g_lbl_count == 0) return NULL;
    ChainLabel key;
    safe_str_copy(key.name, name, sizeof(key.name));
    return bsearch(&key, g_lbl, (size_t)g_lbl_count, sizeof(ChainLabel), cmp_chain_label);
}

static bool is_ident_char(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

// Count every identifier-shaped token in a node's text (outside its
// trailing comment) that names a label. Deliberately broad: a jump
// target, a CALL, "MOV R0, label" (address taken), "pointer label", a
// "%define X label" -- anything that could let control reach the label
// keeps it referenced. A label inside a string literal is counted too
// (harmlessly conservative).
static void count_refs_in(const AsmNode *n, int delta)
{
    const char *p = n->raw;
    bool in_string = false;
    while (*p) {
        if (in_string) {
            if (*p == '\\' && p[1]) p++;
            else if (*p == '"') in_string = false;
            p++;
            continue;
        }
        if (*p == ';') break;
        if (*p == '"') { in_string = true; p++; continue; }
        if (is_ident_char(*p) && !isdigit((unsigned char)*p)) {
            const char *start = p;
            while (is_ident_char(*p)) p++;
            size_t len = (size_t)(p - start);
            if (len < sizeof(((ChainLabel *)0)->name)) {
                char tok[128];
                memcpy(tok, start, len);
                tok[len] = '\0';
                ChainLabel *l = find_label(tok);
                if (l) l->refs += delta;
            }
            continue;
        }
        p++;
    }
}

static void build_label_table(AsmNode *head)
{
    g_lbl_count = 0;
    int cap = 0;
    for (AsmNode *n = head->next; n; n = n->next)
        if (n->type == OP_LABEL) cap++;
    free(g_lbl);
    g_lbl = cap ? calloc((size_t)cap, sizeof(ChainLabel)) : NULL;
    if (!g_lbl) return;

    for (AsmNode *n = head->next; n; n = n->next) {
        if (n->type != OP_LABEL) continue;
        get_label_name(n, g_lbl[g_lbl_count].name, sizeof(g_lbl[0].name));
        if (!g_lbl[g_lbl_count].name[0]) continue;
        g_lbl[g_lbl_count].node = n;
        g_lbl_count++;
    }
    qsort(g_lbl, (size_t)g_lbl_count, sizeof(ChainLabel), cmp_chain_label);

    // Mark duplicates (adjacent after sorting), then count references
    // from every non-label, non-comment line.
    for (int i = 1; i < g_lbl_count; i++) {
        if (strcasecmp(g_lbl[i - 1].name, g_lbl[i].name) == 0) {
            g_lbl[i - 1].duplicate = true;
            g_lbl[i].duplicate     = true;
        }
    }
    for (AsmNode *n = head->next; n; n = n->next) {
        if (n->type == OP_LABEL || n->debug_note) continue;
        count_refs_in(n, +1);
    }
}

// The operand of a jump that names its target (JMP: the only operand;
// JT/JF: the second).
static Operand *jump_target_op(AsmNode *n)
{
    if (n->type == OP_JMP) return n->has_dst ? &n->dst_op : &n->src_op;
    if (n->type == OP_JT || n->type == OP_JF) return n->has_src ? &n->src_op : NULL;
    return NULL;
}

// The label a jump targets, if its operand is exactly a known,
// uniquely-defined label.
static ChainLabel *jump_target_label(AsmNode *n)
{
    Operand *op = jump_target_op(n);
    if (!op || op->mode == MODE_REG || op->mode == MODE_INDIRECT) return NULL;
    char buf[128];
    safe_str_copy(buf, op->raw, sizeof(buf));
    ChainLabel *l = find_label(trim(buf));
    return (l && !l->duplicate) ? l : NULL;
}

// The first real instruction control reaches when entering at 'label':
// further labels, comments and blank lines are skipped. Any directive
// (or the end of the file) means "unknown" -- a data directive marks
// data rather than code, and a %include could splice in code of its
// own -- and so does a non-instruction line.
static AsmNode *first_instruction_at(AsmNode *label)
{
    AsmNode *n = label->next;
    while (n && (n->type == OP_LABEL ||
                 (n->type == OP_OTHER && !is_directive_node(n)))) {
        n = n->next;
    }
    return (n && n->type != OP_OTHER) ? n : NULL;
}

// Can control still reach 'jmp' other than through a retargeted jump?
// Yes if any label in the run directly above it is still referenced, or
// if the code before that run can fall into it.
static bool node_still_reachable(AsmNode *jmp)
{
    AsmNode *p = jmp->prev;
    while (p) {
        if (p->type == OP_LABEL) {
            char name[128];
            get_label_name(p, name, sizeof(name));
            ChainLabel *l = find_label(name);
            if (!l || l->duplicate || l->refs > 0) return true;
        } else if (p->type == OP_OTHER && !is_directive_node(p)) {
            // comment or blank line -- or the dummy head, which means the
            // JMP is at the very start of the program: the boot entry point
            if (p->prev == NULL) return true;
        } else if (p->type == OP_OTHER) {
            return true;   // a directive (%include could hide code): assume so
        } else {
            break;   // a real instruction, or data
        }
        p = p->prev;
    }
    if (!p) return true;
    if (p->type == OP_JMP || p->type == OP_RET || p->type == OP_HLT) return false;
    return true;    // an instruction that falls through
}

// Follow the chain from 'start' (a label). Returns the label at the end
// of it -- or NULL when 'start' doesn't lead to another label hop at all,
// or the chain loops (including back through 'origin', the jump being
// retargeted).
#define MAX_CHAIN_HOPS 64
static ChainLabel *resolve_chain(ChainLabel *start, AsmNode *origin)
{
    ChainLabel *cur = start;
    AsmNode *seen[MAX_CHAIN_HOPS];
    int hops = 0;
    for (;;) {
        AsmNode *ins = first_instruction_at(cur->node);
        if (!ins || ins->type != OP_JMP) break;
        ChainLabel *next = jump_target_label(ins);
        if (!next) break;                        // computed or unknown target
        if (ins == origin) return NULL;          // loops back through origin
        for (int i = 0; i < hops; i++)
            if (seen[i] == ins) return NULL;     // cycle
        if (hops == MAX_CHAIN_HOPS) return NULL; // pathological; leave it
        seen[hops++] = ins;
        cur = next;
    }
    return (cur == start) ? NULL : cur;
}

// Point jump 'n' at label 'to' (operand, raw text and reference counts).
static void retarget(AsmNode *n, ChainLabel *from, ChainLabel *to)
{
    Operand *op = jump_target_op(n);
    safe_str_copy(op->raw, to->name, sizeof(op->raw));
    if (n->type == OP_JMP) {
        snprintf(n->raw, sizeof(n->raw), "    JMP %s", to->name);
    } else {
        snprintf(n->raw, sizeof(n->raw), "    %s %s, %s",
                 n->type == OP_JT ? "JT" : "JF", n->dst_op.raw, to->name);
    }
    from->refs--;
    to->refs++;
}

int peephole_jmp_chain(AsmNode *head)
{
    int optimizations = 0;
    if (!head) return 0;

    build_label_table(head);
    if (g_lbl_count == 0) return 0;

    for (AsmNode *curr = head->next; curr; curr = curr->next)
    {
        if (curr->type != OP_JMP && curr->type != OP_JT && curr->type != OP_JF) continue;

        ChainLabel *target = jump_target_label(curr);
        if (!target) continue;

        ChainLabel *final = resolve_chain(target, curr);
        if (!final) continue;

        // TRIGGER CAP: one atomic transform = one slot. If the budget is
        // exhausted, leave everything exactly as found.
        if (!trigger_allowed()) continue;

        insert_debug_comment(curr->prev, OPT_PEEPHOLE_JMP_CHAIN, curr->raw);
        retarget(curr, target, final);
        optimizations++;

        // Cascade: walk the chain that was just bypassed, deleting each
        // intermediate JMP nothing can reach any more. Stop at the first
        // one that must stay -- it still routes its own users, so every
        // later hop stays reachable through it.
        ChainLabel *hop = target;
        while (hop != final) {
            AsmNode *ins = first_instruction_at(hop->node);
            ChainLabel *next = ins ? jump_target_label(ins) : NULL;
            if (!ins || !next || ins == curr) break;   // (cannot happen)
            if (node_still_reachable(ins)) break;
            if (config.debug) {
                insert_debug_comment(ins->prev, OPT_PEEPHOLE_JMP_CHAIN, ins->raw);
            }
            count_refs_in(ins, -1);
            remove_node(ins);
            hop = next;
        }
    }

    free(g_lbl);
    g_lbl = NULL;
    g_lbl_count = 0;
    return optimizations;
}
