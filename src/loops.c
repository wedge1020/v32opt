#include "v32opt.h"

// Shared helpers of the loop passes (see inc/loops.h).

LoopLabel *g_loop_labels;
int      g_loop_labels_n;

static int cmp_loop_label(const void *a, const void *b)
{
    return strcmp(((const LoopLabel *)a)->name, ((const LoopLabel *)b)->name);
}

LoopLabel *loop_label_find(const char *name)
{
    LoopLabel key;
    safe_str_copy(key.name, name, sizeof(key.name));
    return bsearch(&key, g_loop_labels, (size_t)g_loop_labels_n, sizeof(LoopLabel), cmp_loop_label);
}

bool loop_ident_char(int c) { return isalnum(c) || c == '_' || c == '.' || c == '$'; }

// Calls fn(label) for every identifier token of the line that names a label.
void loop_label_tokens(const AsmNode *n, void (*fn)(LoopLabel *, void *), void *arg)
{
    const char *s = n->raw;
    while (*s && *s != ';') {
        if (loop_ident_char((unsigned char)*s)) {
            const char *b = s;
            while (loop_ident_char((unsigned char)*s)) s++;
            size_t len = (size_t)(s - b);
            if (len < 128) {
                char tok[128];
                memcpy(tok, b, len);
                tok[len] = '\0';
                LoopLabel *l = loop_label_find(tok);
                if (l) fn(l, arg);
            }
        } else {
            s++;
        }
    }
}

static void loop_count_ref(LoopLabel *l, void *arg) { (void)arg; l->refs++; }

void loop_labels_build(AsmNode *head)
{
    int n = 0;
    for (AsmNode *p = head->next; p; p = p->next) if (p->type == OP_LABEL) n++;
    g_loop_labels   = calloc((size_t)n + 1, sizeof(LoopLabel));
    g_loop_labels_n = 0;
    for (AsmNode *p = head->next; p; p = p->next) {
        if (p->type != OP_LABEL) continue;
        get_label_name(p, g_loop_labels[g_loop_labels_n].name, sizeof(g_loop_labels[0].name));
        g_loop_labels[g_loop_labels_n].node = p;
        g_loop_labels_n++;
    }
    qsort(g_loop_labels, (size_t)g_loop_labels_n, sizeof(LoopLabel), cmp_loop_label);
    for (int i = 1; i < g_loop_labels_n; i++) {
        if (strcmp(g_loop_labels[i].name, g_loop_labels[i - 1].name) == 0) {
            g_loop_labels[i].duplicate = g_loop_labels[i - 1].duplicate = true;
        }
    }
    for (AsmNode *p = head->next; p; p = p->next) {
        if (p->type == OP_LABEL || p->type == OP_OTHER) continue;
        loop_label_tokens(p, loop_count_ref, NULL);
    }
}

bool loop_is_comment(const AsmNode *n)
{
    return n->type == OP_OTHER && !is_directive_node(n);
}

AsmNode *loop_next_code(AsmNode *n)      // skips comments only
{
    for (n = n->next; n && loop_is_comment(n); n = n->next) ;
    return n;
}

const char *loop_jump_target(const AsmNode *n, char *buf, size_t sz)
{
    const Operand *op = NULL;
    if (n->type == OP_JMP) op = n->has_dst ? &n->dst_op : &n->src_op;
    else if (n->type == OP_JT || n->type == OP_JF) op = n->has_src ? &n->src_op : NULL;
    if (!op || op->mode == MODE_REG || op->mode == MODE_INDIRECT) return NULL;
    safe_str_copy(buf, op->raw, sz);
    return trim(buf);
}

bool loop_imm_value(const Operand *op, long *v)
{
    if (!op || op->mode != MODE_IMMEDIATE || !is_numeric_immediate(op) || op->is_float) return false;
    *v = (long)op->immediate;
    return true;
}

void loop_link_before(AsmNode *pos, AsmNode *nn)
{
    nn->prev = pos->prev;
    nn->next = pos;
    if (pos->prev) pos->prev->next = nn;
    pos->prev = nn;
}


void loop_labels_free(void)
{
    free(g_loop_labels);
    g_loop_labels   = NULL;
    g_loop_labels_n = 0;
}

bool loop_mentions_reg(const AsmNode *n, const char *reg)
{
    const char *s = n->raw;
    size_t len = strlen(reg);
    while (*s && *s != ';') {
        if (loop_ident_char((unsigned char)*s)) {
            const char *b = s;
            while (loop_ident_char((unsigned char)*s)) s++;
            if ((size_t)(s - b) == len && strncasecmp(b, reg, len) == 0) return true;
        } else {
            s++;
        }
    }
    return false;
}

static bool is_scratch(const char *r)
{
    int i = get_reg_index(r);
    return i >= 1 && i <= 10;
}

bool loop_parse_counted(AsmNode *jmp, CountedLoop *L)
{
    char sbuf[128];
    const char *start_name = (jmp->type == OP_JMP) ? loop_jump_target(jmp, sbuf, sizeof(sbuf)) : NULL;
    LoopLabel *start = start_name ? loop_label_find(start_name) : NULL;
    if (!start || start->duplicate || start->refs != 1 ||
        strncmp(start_name, "__function_", 11) == 0) return false;

    // the test: MOV Rc,Ri / ... / I<cmp> Rc,X / JF Rc,end  (at most 4 instructions)
    AsmNode *mv = loop_next_code(start->node);
    if (!mv || mv->type == OP_LABEL || mv->type != OP_MOV ||
        !mv->has_dst || mv->dst_op.mode != MODE_REG ||
        !mv->has_src || mv->src_op.mode != MODE_REG) return false;
    const char *rc = mv->dst_op.reg, *ri = mv->src_op.reg;
    if (!is_scratch(ri) || str_case_eq(rc, ri)) return false;

    AsmNode *cmp = NULL, *jf = NULL;
    int len = 1;
    for (AsmNode *n = loop_next_code(mv); n && len < 6; n = loop_next_code(n)) {
        len++;
        if (n->type == OP_JF) { jf = n; break; }
        if (n->type == OP_LABEL || n->type == OP_OTHER || is_branch_or_call(n) ||
            is_control_flow_boundary(n)) return false;
        cmp = n;
    }
    if (!jf || !cmp || !jf->has_dst || jf->dst_op.mode != MODE_REG || !str_case_eq(jf->dst_op.reg, rc)) return false;
    if (!(cmp->type == OP_ILT || cmp->type == OP_ILE || cmp->type == OP_IGT ||
          cmp->type == OP_IGE || cmp->type == OP_INE) ||
        !cmp->has_dst || cmp->dst_op.mode != MODE_REG || !str_case_eq(cmp->dst_op.reg, rc)) return false;
    // anything between MOV and the compare must leave Rc and Ri alone
    for (AsmNode *n = loop_next_code(mv); n && n != cmp; n = loop_next_code(n)) {
        if (modifies_register(n, rc) || modifies_register(n, ri) ||
            loop_mentions_reg(n, rc)) return false;
    }

    char ebuf[128];
    const char *end_name = loop_jump_target(jf, ebuf, sizeof(ebuf));
    LoopLabel *end = end_name ? loop_label_find(end_name) : NULL;
    if (!end || end->duplicate) return false;
    bool falls = false;
    for (AsmNode *p = jmp->next; p; p = p->next) {
        if (p->type == OP_LABEL) {
            char b[128];
            get_label_name(p, b, sizeof(b));
            if (strcmp(b, end_name) == 0) { falls = true; break; }
        } else if (!loop_is_comment(p)) break;
    }
    if (!falls) return false;

    AsmNode *step = jmp->prev;
    while (step && (loop_is_comment(step) || step->type == OP_LABEL)) step = step->prev;
    long sv;
    if (!step || (step->type != OP_IADD && step->type != OP_ISUB) ||
        !step->has_dst || step->dst_op.mode != MODE_REG ||
        !str_case_eq(step->dst_op.reg, ri) || !loop_imm_value(&step->src_op, &sv)) return false;

    long bound = 0;
    L->jmp = jmp; L->start = start->node; L->test_first = mv; L->cmp = cmp; L->jf = jf;
    L->first = jf->next; L->step = step; L->test_len = len;
    safe_str_copy(L->ri, ri, sizeof(L->ri));
    safe_str_copy(L->rc, rc, sizeof(L->rc));
    safe_str_copy(L->end_name, end_name, sizeof(L->end_name));
    L->cmp_type = cmp->type;
    L->stepv = (step->type == OP_IADD) ? sv : -sv;
    L->bound_imm = (loop_next_code(mv) == cmp) && loop_imm_value(&cmp->src_op, &bound);
    L->bound = bound;
    return L->first != NULL;
}
