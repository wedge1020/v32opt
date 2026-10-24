#include "v32opt.h"

// ===================================================================
// LOOP INVERSION  (-floop-invert)
//
// The compilers emit a top-tested loop, so every iteration executes two
// branches: the test's conditional jump and the back edge.
//
//   start:   <test>                       start:   <test>
//            JF  Rc, end                           JF  Rc, end
//            <body>                       body:    <body>
//            JMP start             ->              <test>
//   end:                                           JT  Rc, body   (JF if the
//                                                  test exits on JT)
//                                         end:
//
// The back edge "JMP start" becomes a copy of the test followed by a
// conditional jump straight back to the body: one jump fewer per
// iteration (-1 cycle), at the price of the test's size once. The top
// copy stays where it is, so the loop entry and every other jump to
// "start" (a "continue" in a while loop) still see the test first.
//
// SOUNDNESS. The rewrite is tail duplication of the code at the jump
// target: "JMP start" executes the test, then JF Rc,end either goes to
// "end" (Rc == 0) or continues with the first body instruction. The
// replacement executes the same test and takes JT Rc,body (Rc != 0) or
// falls through into whatever follows the old jump. (A test that exits on
// JT Rc,end gets the mirror image: JF Rc,body.) That is only the
// same thing if
//   * the test is straight-line code that ends in the conditional jump
//     (no label inside, so nothing else enters it midway; no CALL, no
//     other branch, no directive), at most LOOP_INVERT_MAX_TEST
//     instructions;
//   * the jump is a backward jump (its target label was seen earlier),
//     which is what makes it pay;
//   * the code right after the jump begins with the test's exit label
//     ("end"), possibly after further labels and comments, so the
//     fall-through is the loop exit;
//   * both labels are uniquely defined (the compilers' default .local
//     labels repeat per function; with those the site is skipped), and
//     the target is not a function entry.
// No register is read or written that the original did not: the test
// runs with the same register state either way, and the exit (Rc == 0)
// leaves the same values behind.
//
// The new body label is a plain label, or a .local one when the loop
// start is (so enhanced-syntax local scopes are not disturbed).
//
// Run after promote-*: those passes recognise a loop by its back edge
// "JMP start", which this pass removes.
// ===================================================================


typedef struct {
    char     name[128];
    AsmNode *node;
    bool     duplicate;
    bool     seen;       // already passed in the scan => a jump to it goes backward
} LiLabel;

static LiLabel *g_li;
static int      g_li_n;

static int cmp_li(const void *a, const void *b)
{
    return strcmp(((const LiLabel *)a)->name, ((const LiLabel *)b)->name);
}

static LiLabel *li_find(const char *name)
{
    LiLabel key;
    safe_str_copy(key.name, name, sizeof(key.name));
    return bsearch(&key, g_li, (size_t)g_li_n, sizeof(LiLabel), cmp_li);
}

static void li_build(AsmNode *head)
{
    int n = 0;
    for (AsmNode *p = head->next; p; p = p->next) if (p->type == OP_LABEL) n++;
    g_li   = calloc((size_t)n + 1, sizeof(LiLabel));
    g_li_n = 0;
    for (AsmNode *p = head->next; p; p = p->next) {
        if (p->type != OP_LABEL) continue;
        get_label_name(p, g_li[g_li_n].name, sizeof(g_li[0].name));
        g_li[g_li_n].node = p;
        g_li_n++;
    }
    qsort(g_li, (size_t)g_li_n, sizeof(LiLabel), cmp_li);
    for (int i = 1; i < g_li_n; i++) {
        if (strcmp(g_li[i].name, g_li[i - 1].name) == 0) {
            g_li[i].duplicate = g_li[i - 1].duplicate = true;
        }
    }
}

// The label a JMP/JT/JF names (exact text), or NULL.
static const char *target_text(const AsmNode *n, char *buf, size_t sz)
{
    const Operand *op = NULL;
    if (n->type == OP_JMP) op = n->has_dst ? &n->dst_op : &n->src_op;
    else if (n->type == OP_JT || n->type == OP_JF) op = n->has_src ? &n->src_op : NULL;
    if (!op || op->mode == MODE_REG || op->mode == MODE_INDIRECT) return NULL;
    safe_str_copy(buf, op->raw, sz);
    return trim(buf);
}

static bool is_comment_node(const AsmNode *n)
{
    return n->type == OP_OTHER && !is_directive_node(n);
}

// The straight-line test at 'label': fills first/jf (first test instruction,
// terminating conditional jump) and returns the instruction count, or 0.
static int find_test(const AsmNode *label, AsmNode **first, AsmNode **jf)
{
    AsmNode *p = label->next;
    while (p && (p->type == OP_LABEL || is_comment_node(p))) p = p->next;
    *first = p;
    int count = 0;
    while (p) {
        if (p->type == OP_LABEL || p->type == OP_OTHER) {
            if (is_comment_node(p)) { p = p->next; continue; }
            return 0;
        }
        if (p->type == OP_JT || p->type == OP_JF) {
            if (count == 0 || !p->has_dst || p->dst_op.mode != MODE_REG) return 0;
            *jf = p;
            return count;
        }
        if (is_branch_or_call(p) || is_control_flow_boundary(p)) return 0;
        if (++count > g_loop_tune.invert_max_test) return 0;
        p = p->next;
    }
    return 0;
}

// Does the code after 'jmp' begin with a label called 'name'?
static bool falls_into(const AsmNode *jmp, const char *name)
{
    for (AsmNode *p = jmp->next; p; p = p->next) {
        if (p->type == OP_LABEL) {
            char buf[128];
            get_label_name(p, buf, sizeof(buf));
            if (strcmp(buf, name) == 0) return true;
        } else if (!is_comment_node(p)) {
            return false;
        }
    }
    return false;
}

int peephole_loop_invert(AsmNode *head)
{
    if (!head) return 0;
    li_build(head);
    int optimizations = 0, serial = 0;

    for (AsmNode *p = head->next; p; ) {
        AsmNode *nextp = p->next;
        if (p->type == OP_LABEL) {
            char nm[128];
            get_label_name(p, nm, sizeof(nm));
            LiLabel *l = li_find(nm);
            if (l && !l->duplicate) l->seen = true;
            p = nextp;
            continue;
        }
        char tbuf[128];
        const char *tgt = (p->type == OP_JMP) ? target_text(p, tbuf, sizeof(tbuf)) : NULL;
        LiLabel *start = tgt ? li_find(tgt) : NULL;
        if (!start || start->duplicate || !start->seen || strncmp(tgt, "__function_", 11) == 0) {
            p = nextp;
            continue;
        }

        if (loop_heat(start->node, p) < 0) { p = nextp; continue; }      // cold per the profile

        AsmNode *first = NULL, *jf = NULL;
        int n = find_test(start->node, &first, &jf);
        if (!n || !jf) { p = nextp; continue; }

        char ebuf[128];
        const char *exit_label = target_text(jf, ebuf, sizeof(ebuf));
        LiLabel *ex = exit_label ? li_find(exit_label) : NULL;
        if (!ex || ex->duplicate || !falls_into(p, exit_label) || !jf->next) {
            p = nextp;
            continue;
        }

        // The body label: reuse a label already sitting right after the JF,
        // else make one (a .local name if the loop's labels are .local).
        AsmNode *after = jf->next;
        while (after->next && is_comment_node(after)) after = after->next;
        char body_label[128];
        if (after->type == OP_LABEL) {
            get_label_name(after, body_label, sizeof(body_label));
            LiLabel *bl = li_find(body_label);
            if (!bl || bl->duplicate) { p = nextp; continue; }
        } else {
            char raw[160];
            snprintf(body_label, sizeof(body_label), "%sv32opt_loop_%d",
                     tgt[0] == '.' ? "." : "__", ++serial);
            while (li_find(body_label)) {
                snprintf(body_label, sizeof(body_label), "%sv32opt_loop_%d",
                         tgt[0] == '.' ? "." : "__", ++serial);
            }
            snprintf(raw, sizeof(raw), "%s:", body_label);
            AsmNode *lab = create_node(raw, OP_LABEL, NULL, NULL, NULL);
            lab->prev = after->prev;
            lab->next = after;
            after->prev->next = lab;
            after->prev = lab;
            // keep the table honest for the labels created here
            g_li = realloc(g_li, ((size_t)g_li_n + 1) * sizeof(LiLabel));
            memset(&g_li[g_li_n], 0, sizeof(LiLabel));
            safe_str_copy(g_li[g_li_n].name, body_label, sizeof(g_li[0].name));
            g_li[g_li_n].node = lab;
            g_li_n++;
            qsort(g_li, (size_t)g_li_n, sizeof(LiLabel), cmp_li);
            // realloc/qsort moved the entries: 'start' and 'ex' are stale now,
            // and neither is used again below
        }

        // Copy the test in front of the jump, then replace the jump by JT.
        for (AsmNode *t = first; t != jf; t = t->next) {
            if (t->type == OP_LABEL || is_comment_node(t)) continue;
            AsmNode *c = clone_node(t);
            c->prev = p->prev;
            c->next = p;
            p->prev->next = c;
            p->prev = c;
        }
        // The test leaves the loop when its jump is taken (JF: Rc == 0, JT:
        // Rc != 0); the back edge must jump to the body on the opposite
        // condition and fall through on the exit condition.
        bool exit_on_zero = (jf->type == OP_JF);
        char raw[200];
        snprintf(raw, sizeof(raw), "    %s %s, %s", exit_on_zero ? "JT" : "JF",
                 jf->dst_op.reg, body_label);
        AsmNode *jt = create_node(raw, exit_on_zero ? OP_JT : OP_JF,
                                  exit_on_zero ? "JT" : "JF", jf->dst_op.reg, body_label);
        jt->prev = p->prev;
        jt->next = p;
        p->prev->next = jt;
        p->prev = jt;
        remove_node(p);

        optimizations++;
        p = jt->next;
    }

    free(g_li);
    g_li = NULL;
    g_li_n = 0;
    return optimizations;
}
