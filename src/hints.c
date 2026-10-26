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
    free(g_fn);
    g_fn = NULL; g_fn_n = g_fn_cap = 0;
    g_hdr_abi = ABI_AUTO;
}

static int cmp_fn(const void *a, const void *b) {
    return strcmp(((const FnHints *)a)->name, ((const FnHints *)b)->name);
}

// Valid once hints_parse has returned (the table is sorted then).
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

static void parse_fn_line(char *rest) {
    char *save = NULL;
    char *name = strtok_r(rest, " \t", &save);
    if (!name) return;
    FnHints *h = fn_slot(name);
    if (!h) return;
    for (char *tok = strtok_r(NULL, " \t", &save); tok; tok = strtok_r(NULL, " \t", &save)) {
        if      (strcmp(tok, "noinline") == 0) h->noinline = true;
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
        // anything else (clobbers=..., future attributes): ignored
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
    for (AsmNode *n = head ? head->next : NULL; n; n = n->next) {
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
    return any;
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
