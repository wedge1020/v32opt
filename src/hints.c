#include "v32opt.h"

// ============================================================================
// hints.c -- reading the compiler's ';@' hints (see inc/hints.h, doc/hints.md)
//
// Everything here is advisory and defensive: an unknown token is ignored, a
// malformed attribute is ignored (or, for 'escapes=', degrades to "all slots
// escape"), and a hint line for a function that is not in the file does
// nothing. The one thing a hint can never do is make the program do something
// the unhinted optimizer would not have been willing to prove for itself in
// the same situation -- the caller of this module decides what a hint is
// worth (promote.c: per-slot escape facts; inline.c: noinline; the load/store
// peepholes: volatile; promote.c: the calling convention).
// ============================================================================

#define HINTS_VERSION 1

static FnHints *g_fn      = NULL;
static int      g_fn_n    = 0;
static int      g_fn_cap  = 0;
static AbiMode  g_hdr_abi = ABI_AUTO;      // what the file's header declared
static LoopFacts *g_lf    = NULL;
static int      g_lf_n    = 0;
static int      g_lf_cap  = 0;

bool node_in_cold_block(const AsmNode *n) {
    return n && (n->hints & NH_COLD_BLK) != 0;
}

const LoopFacts *hints_loop_facts(const AsmNode *label) {
    if (!label || !(label->hints & NH_LOOP_FACTS)) return NULL;
    for (int i = 0; i < g_lf_n; i++) if (g_lf[i].label == label) return &g_lf[i];
    return NULL;
}

bool node_is_volatile(const AsmNode *n) {
    return n && (n->hints & NH_VOLATILE) != 0;
}

bool abi_caller_save(void) {
    if (config.lang_mode == LANG_LUA) return true;          // v32lua is caller-save
    AbiMode m = config.abi;
    if (m == ABI_AUTO) m = g_hdr_abi;
    return m == ABI_CALLER_SAVE;
}

void hints_free(void) {
    free(g_lf); g_lf = NULL; g_lf_n = g_lf_cap = 0;
    free(g_fn);
    g_fn = NULL; g_fn_n = g_fn_cap = 0;
    g_hdr_abi = ABI_AUTO;
}

static int cmp_fn(const void *a, const void *b) {
    return strcmp(((const FnHints *)a)->name, ((const FnHints *)b)->name);
}

// Valid once hints_parse has returned (the table is sorted then).
const FnHints *hints_fn_of_call(const AsmNode *call) {
    if (!call || call->type != OP_CALL || !call->has_dst || call->dst_op.mode != MODE_IMMEDIATE) return NULL;
    char t[128];
    safe_str_copy(t, call->dst_op.raw, sizeof(t));
    return hints_fn(trim(t));
}

const FnHints *hints_fn(const char *label) {
    if (!label || g_fn_n == 0) return NULL;
    FnHints key;
    safe_str_copy(key.name, label, sizeof(key.name));
    return bsearch(&key, g_fn, (size_t)g_fn_n, sizeof(FnHints), cmp_fn);
}

bool hints_fn_slot_escapes(const FnHints *h, int off) {
    if (!h || !h->escapes_known) return false;
    if (h->escapes_all) return true;
    if (off < -128 || off > 127) return true;               // outside the table: unknown, so assume yes
    return h->escaped[off + 128] != 0;
}

static FnHints *fn_slot(const char *name) {
    for (int i = 0; i < g_fn_n; i++)
        if (strcmp(g_fn[i].name, name) == 0) return &g_fn[i];
    if (g_fn_n == g_fn_cap) {
        int ncap = g_fn_cap ? g_fn_cap * 2 : 64;
        FnHints *p = realloc(g_fn, (size_t)ncap * sizeof(FnHints));
        if (!p) return NULL;
        g_fn = p; g_fn_cap = ncap;
    }
    FnHints *h = &g_fn[g_fn_n++];
    memset(h, 0, sizeof(*h));
    safe_str_copy(h->name, name, sizeof(h->name));
    return h;
}

// "-5..-3,-1,+2" -> marks. Returns false if anything in the list is unusable.
static bool parse_escape_list(FnHints *h, const char *s) {
    while (*s) {
        char *end;
        long a = strtol(s, &end, 10);
        if (end == s) return false;
        long b = a;
        s = end;
        if (s[0] == '.' && s[1] == '.') {
            s += 2;
            b = strtol(s, &end, 10);
            if (end == s) return false;
            s = end;
        }
        if (a > b || a < -128 || b > 127) return false;
        for (long o = a; o <= b; o++) { h->escaped[o + 128] = 1; h->any_escaped = true; }
        if (*s == ',') s++;
        else if (*s) return false;
    }
    return true;
}

// "R0", "R0-R3", "R0,R2,R5-R7" -> bit mask (bit n = Rn). false: anything unreadable, or an empty set
// is fine ("none"), but a register above R15 or a reversed range is not.
static bool parse_reg_set(const char *s, unsigned *mask) {
    unsigned m = 0;
    if (strcmp(s, "none") == 0) { *mask = 0; return true; }
    while (*s) {
        if (*s != 'R' && *s != 'r') return false;
        char *end;
        long a = strtol(s + 1, &end, 10);
        if (end == s + 1 || a < 0 || a > 15) return false;
        long b = a;
        s = end;
        if (*s == '-') {
            if (*(s + 1) != 'R' && *(s + 1) != 'r') return false;
            b = strtol(s + 2, &end, 10);
            if (end == s + 2 || b < a || b > 15) return false;
            s = end;
        }
        for (long r = a; r <= b; r++) m |= 1u << r;
        if (*s == ',') s++;
        else if (*s) return false;
    }
    *mask = m;
    return true;
}

// 'key=N' with an integer N (optionally signed); false if tok is not that key / not a number
static bool tok_int(const char *tok, const char *key, long *out) {
    size_t kl = strlen(key);
    if (strncmp(tok, key, kl) != 0 || tok[kl] != '=') return false;
    char *end;
    errno = 0;
    long v = strtol(tok + kl + 1, &end, 10);
    if (end == tok + kl + 1 || *end != '\0' || errno) return false;
    *out = v;
    return true;
}

static bool parse_loop_fact(LoopFacts *lf, const char *tok) {
    long v;
    if (tok_int(tok, "counter", &v) && v >= -128 && v <= 127) { lf->has_counter = true; lf->counter = (int)v; return true; }
    if (tok_int(tok, "limit",   &v) && v >= -128 && v <= 127) { lf->has_limit   = true; lf->limit   = (int)v; return true; }
    if (tok_int(tok, "step",    &v) && v >= -128 && v <= 127) { lf->has_step    = true; lf->step    = (int)v; return true; }
    if (tok_int(tok, "stepval", &v)) { lf->has_stepval = true; lf->stepval = v; return true; }
    if (tok_int(tok, "from",    &v)) { lf->has_from    = true; lf->from    = v; return true; }
    if (tok_int(tok, "to",      &v)) { lf->has_to      = true; lf->to      = v; return true; }
    return false;
}

// Does a JMP/JT/JF between the start of the function and `before` (exclusive) target the label `name`?
static bool label_jumped_to_before(const AsmNode *before, const char *name) {
    for (const AsmNode *x = before->prev; x; x = x->prev) {
        if (x->type == OP_LABEL) {
            char nm[128];
            get_label_name(x, nm, sizeof(nm));
            if (strncmp(nm, "__function_", 11) == 0 && !is_fn_return_label(x)) return false;
        }
        if (x->type != OP_JMP && x->type != OP_JT && x->type != OP_JF) continue;
        const Operand *t = x->type == OP_JMP ? &x->dst_op : &x->src_op;
        if ((x->type == OP_JMP ? x->has_dst : x->has_src) && t->mode == MODE_IMMEDIATE && name_eq(t->raw, name)) return true;
    }
    return false;
}

// ';@cold' before a label: the block that starts at the next label and runs to its first unconditional
// JMP / RET / HLT (inclusive) is cold. `from` is the node after the ';@cold' line.
static bool mark_cold_block(AsmNode *from) {
    AsmNode *l = from;
    while (l && l->type == OP_OTHER) {                           // comments and blank lines between
        const char *q = l->raw;
        while (*q == ' ' || *q == '\t') q++;
        if (*q && *q != ';') return false;                       // a directive: not a block
        l = l->next;
    }
    if (!l || l->type != OP_LABEL) return false;
    // the terminator must come before another function starts; otherwise the hint means nothing
    AsmNode *end = NULL;
    int steps = 0;
    for (AsmNode *n = l->next; n && steps < 4096; n = n->next, steps++) {
        if (n->type == OP_LABEL) {
            char nm[128];
            get_label_name(n, nm, sizeof(nm));
            if (strncmp(nm, "__function_", 11) == 0 && !is_fn_return_label(n)) return false;
            // The slow path usually falls into the join label that the hot path jumped over it to reach. The
            // block ends there: a label that a jump BEFORE the cold label already targets is where the hot
            // path continues, not part of the cold block.
            if (label_jumped_to_before(l, nm)) { end = n->prev; break; }
        }
        if (n->type == OP_JMP || n->type == OP_RET || n->type == OP_HLT) { end = n; break; }
        if (n->type == OP_OTHER && is_data_directive_node(n)) return false;
    }
    if (!end) return false;
    for (AsmNode *n = l; n; n = n->next) {
        n->hints |= NH_COLD_BLK;
        if (n == end) break;
    }
    return true;
}

static void parse_fn_line(char *rest) {
    char *save = NULL;
    char *name = strtok_r(rest, " \t", &save);
    if (!name) return;
    FnHints *h = fn_slot(name);
    if (!h) return;
    for (char *tok = strtok_r(NULL, " \t", &save); tok; tok = strtok_r(NULL, " \t", &save)) {
        if      (strcmp(tok, "noinline") == 0) h->noinline = true;
        else if (strcmp(tok, "inline")   == 0) h->inline_kw    = true;
        else if (strcmp(tok, "alwaysinline") == 0) h->alwaysinline = true;
        else if (strcmp(tok, "hot")      == 0) h->hot  = true;
        else if (strcmp(tok, "cold")     == 0) h->cold = true;
        else if (strcmp(tok, "noreturn") == 0) h->noreturn = true;
        else if (strcmp(tok, "leaf")     == 0) h->leaf     = true;
        else if (strcmp(tok, "naked")    == 0) h->naked    = true;
        else if (strncmp(tok, "escapes=", 8) == 0) {
            const char *v = tok + 8;
            h->escapes_known = true;
            h->escapes_all = false;
            h->any_escaped = false;
            memset(h->escaped, 0, sizeof(h->escaped));
            if (strcmp(v, "none") == 0) {
                // nothing's address is taken
            } else if (strcmp(v, "all") == 0 || !parse_escape_list(h, v)) {
                h->escapes_all = true;                       // 'all', or a list we can't trust
                h->any_escaped = true;
            }
        }
        else if (strncmp(tok, "lines=", 6) == 0) {
            int a = 0, b = 0;
            if (sscanf(tok + 6, "%d..%d", &a, &b) == 2 && a > 0 && b >= a) { h->line_lo = a; h->line_hi = b; }
        }
        else if (strncmp(tok, "clobbers=", 9) == 0) {
            unsigned m;
            if (parse_reg_set(tok + 9, &m)) { h->clobbers_known = true; h->clobbers = m; }
        }
        // anything else (future attributes): ignored
    }
}

// The text after ';@' on a line, or NULL. For an instruction line only a
// trailing comment counts; a ';' inside a data directive's string is not one.
static const char *hint_text(const char *raw) {
    const char *p = strstr(raw, ";@");
    return p ? p + 2 : NULL;
}

bool hints_parse(AsmNode *head) {
    bool any = false;
    bool in_cold_region = false;                       // between ';@cold {' and ';@cold }'
    for (AsmNode *n = head ? head->next : NULL; n; n = n->next) {
        if (in_cold_region && n->type != OP_OTHER) {
            // the region is straight-line code the hot path falls into and out of: a label or a jump ends it
            if (n->type == OP_LABEL || n->type == OP_JMP || n->type == OP_JT || n->type == OP_JF ||
                n->type == OP_RET || n->type == OP_HLT) in_cold_region = false;
            else n->hints |= NH_COLD_BLK;
        }
        if (n->type == OP_OTHER) {
            // a whole-line hint: only if the line STARTS with ';@' (after blanks)
            const char *p = n->raw;
            while (*p == ' ' || *p == '\t') p++;
            if (p[0] != ';' || p[1] != '@') continue;
            char buf[1024];
            safe_str_copy(buf, p + 2, sizeof(buf));
            char *save = NULL;
            char *kw = strtok_r(buf, " \t", &save);
            if (!kw) continue;
            if (strcmp(kw, "hints") == 0) {
                char *ver = strtok_r(NULL, " \t", &save);
                if (!ver || atoi(ver) != HINTS_VERSION) {
                    fprintf(stderr, "v32opt: ignoring compiler hints of an unknown version ('%s')\n", ver ? ver : "");
                    hints_free();
                    return false;                              // all or nothing
                }
                any = true;
                for (char *tok = strtok_r(NULL, " \t", &save); tok; tok = strtok_r(NULL, " \t", &save)) {
                    if (strncmp(tok, "abi=", 4) == 0) {
                        if      (strcmp(tok + 4, "caller-save") == 0) g_hdr_abi = ABI_CALLER_SAVE;
                        else if (strcmp(tok + 4, "callee-save") == 0) g_hdr_abi = ABI_CALLEE_SAVE;
                    }
                }
            } else if (strcmp(kw, "fn") == 0) {
                parse_fn_line(save ? save : (char *)"");
                any = true;
            } else if (strcmp(kw, "cold") == 0) {
                char *arg = strtok_r(NULL, " \t", &save);
                if (arg && strcmp(arg, "{") == 0) {
                    if (!in_cold_region) { in_cold_region = true; any = true; }       // a second '{' changes nothing
                } else if (arg && strcmp(arg, "}") == 0) {
                    in_cold_region = false;
                } else if (mark_cold_block(n->next)) any = true;
            } else if (strcmp(kw, "loop") == 0) {
                // the attributes belong to the next label (the loop's start), if that is what follows
                unsigned bits = 0;
                LoopFacts lf;
                memset(&lf, 0, sizeof(lf));
                bool is_for = false, any_fact = false;
                for (char *tok = strtok_r(NULL, " \t", &save); tok; tok = strtok_r(NULL, " \t", &save)) {
                    if      (strcmp(tok, "kind=for") == 0) is_for = true;
                    else if (parse_loop_fact(&lf, tok))    any_fact = true;
                    else if (strcmp(tok, "hot") == 0)      bits |= NH_LOOP_HOT;
                    else if (strcmp(tok, "cold") == 0)     bits |= NH_LOOP_COLD;
                    else if (strcmp(tok, "nounroll") == 0) bits = (bits & ~NH_UNROLL_MASK) | NH_LOOP_NOUNROLL;
                    else if (strncmp(tok, "unroll=", 7) == 0) {
                        const char *v = tok + 7;
                        long c = strcmp(v, "full") == 0 ? HINT_UNROLL_FULL : strtol(v, NULL, 10);
                        if (c >= 2 && c <= HINT_UNROLL_FULL)
                            bits = (bits & ~(NH_UNROLL_MASK | NH_LOOP_NOUNROLL)) | ((unsigned)c << NH_UNROLL_SHIFT);
                        else if (c == 1)
                            bits = (bits & ~NH_UNROLL_MASK) | NH_LOOP_NOUNROLL;
                    }                                               // anything else: ignored
                }
                AsmNode *l = n->next;
                while (l && l->type == OP_OTHER) {                  // blank lines and comments between
                    const char *q = l->raw;
                    while (*q == ' ' || *q == '\t') q++;
                    if (*q && *q != ';') { l = NULL; break; }       // a directive: not a loop label
                    l = l->next;
                }
                if (l && l->type == OP_LABEL && is_for && any_fact) {
                    // facts only mean something for the kind they were written for
                    if (g_lf_n == g_lf_cap) {
                        int ncap = g_lf_cap ? g_lf_cap * 2 : 16;
                        LoopFacts *p = realloc(g_lf, (size_t)ncap * sizeof(LoopFacts));
                        if (p) { g_lf = p; g_lf_cap = ncap; }
                    }
                    if (g_lf_n < g_lf_cap) {
                        lf.label = l;
                        g_lf[g_lf_n++] = lf;
                        bits |= NH_LOOP_FACTS;
                    }
                }
                if (l && l->type == OP_LABEL && bits) { l->hints |= bits; any = true; }
            }
            continue;
        }
        // instruction lines: a trailing ';@v' marks a volatile access
        if (n->type == OP_LABEL || n->type > OP_POW) continue;
        const char *t = hint_text(n->raw);
        if (!t) continue;
        const char *semi = strchr(n->raw, ';');
        if (semi != t - 2) continue;                            // the FIRST ';' must be the hint's
        for (const char *q = t; *q; ) {
            while (*q == ' ' || *q == '\t') q++;
            const char *e = q;
            while (*e && *e != ' ' && *e != '\t') e++;
            size_t len = (size_t)(e - q);
            if ((len == 1 && *q == 'v') || (len == 8 && strncmp(q, "volatile", 8) == 0)) {
                n->hints |= NH_VOLATILE;
                any = true;
            }
            q = e;
        }
    }
    if (g_fn_n > 1) qsort(g_fn, (size_t)g_fn_n, sizeof(FnHints), cmp_fn);

    // The heat of a function (';@fn ... hot|cold') is passed to every label that follows it, up to the next
    // function label: that is how a loop finds out which function it is in (hints_static_heat).
    if (g_fn_n > 0) {
        unsigned heat = 0;
        for (AsmNode *n = head ? head->next : NULL; n; n = n->next) {
            if (n->type != OP_LABEL) continue;
            char nm[128];
            get_label_name(n, nm, sizeof(nm));
            if (strncmp(nm, "__function_", 11) == 0) {
                const FnHints *f = hints_fn(nm);
                heat = f ? (f->cold ? NH_FN_COLD : f->hot ? NH_FN_HOT : 0) : 0;
            }
            n->hints |= heat;
        }
    }
    return any;
}

int hints_static_heat(const AsmNode *l) {
    if (!l) return 0;
    if (l->hints & NH_LOOP_COLD) return -1;
    if (l->hints & NH_LOOP_HOT)  return +1;
    if (l->hints & NH_FN_COLD)   return -1;
    if (l->hints & NH_FN_HOT)    return +1;
    return 0;
}

int hints_loop_unroll(const AsmNode *l) {
    if (!l) return 0;
    unsigned c = (l->hints & NH_UNROLL_MASK) >> NH_UNROLL_SHIFT;
    if (c) return (int)c;
    return (l->hints & NH_LOOP_NOUNROLL) ? 1 : 0;
}

// ---------------------------------------------------------------------------
// Safety net, run just before the output is written: every node still carrying the
// volatile mark must still be a memory access. A pass that rewrote a volatile load into
// a register copy (or otherwise lost the access) has ignored the hint; say so rather
// than silently shipping it. Returns the number of such nodes.
// ---------------------------------------------------------------------------
int hints_verify(AsmNode *head) {
    int bad = 0;
    for (AsmNode *n = head ? head->next : NULL; n; n = n->next) {
        if (!node_is_volatile(n)) continue;
        bool mem = (n->has_dst && n->dst_op.mode == MODE_INDIRECT) ||
                   (n->has_src && n->src_op.mode == MODE_INDIRECT);
        if (!mem) {
            bad++;
            fprintf(stderr, "v32opt: warning: a volatile access was optimized away or rewritten "
                            "(input line %d: %s) -- please report this\n", n->src_line, n->raw);
        }
    }
    return bad;
}
