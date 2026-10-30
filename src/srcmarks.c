#include "v32opt.h"

// ============================================================================
// srcmarks.c -- ';@src FILE:LINE' source markers (see inc/srcmarks.h, doc/srcmarks.md)
//
// Life of a marker:
//   srcmarks_attach()  right after the file is read: every ';@src' line is taken out of the
//                      node list and its location is stored on each instruction that follows it
//                      (AsmNode.loc_file/loc_line/loc_col).
//   the passes         never see a marker line. clone_node() copies the location, an in-place
//                      rewrite leaves it, deleting an instruction deletes its location with it.
//                      An instruction a pass made up has none (LOC_UNSET) and, in the output,
//                      belongs to whatever precedes it -- exactly what it would mean as text.
//   srcmarks_emit()    just before the file is written: a marker before every instruction whose
//                      location differs from the one before it, and before the first instruction
//                      of each function, so that no function starts out attributed to the code
//                      that precedes it textually.
// ============================================================================

static char **g_files    = NULL;       // interned file names; loc_file n refers to g_files[n-1]
static int    g_nfiles   = 0;
static int    g_cap      = 0;
static bool   g_present  = false;

bool srcmarks_present(void) { return g_present; }

const char *srcmarks_file(int f) {
    return (f >= 1 && f <= g_nfiles) ? g_files[f - 1] : NULL;
}

void srcmarks_free(void) {
    for (int i = 0; i < g_nfiles; i++) free(g_files[i]);
    free(g_files);
    g_files = NULL; g_nfiles = g_cap = 0;
    g_present = false;
}

static int intern_file(const char *name, size_t len) {
    for (int i = 0; i < g_nfiles; i++)
        if (strlen(g_files[i]) == len && memcmp(g_files[i], name, len) == 0) return i + 1;
    if (g_nfiles == g_cap) {
        int ncap = g_cap ? g_cap * 2 : 8;
        char **p = realloc(g_files, (size_t)ncap * sizeof(char *));
        if (!p) return LOC_NONE;
        g_files = p; g_cap = ncap;
    }
    char *copy = malloc(len + 1);
    if (!copy) return LOC_NONE;
    memcpy(copy, name, len);
    copy[len] = '\0';
    g_files[g_nfiles] = copy;
    return ++g_nfiles;
}

// Digits only, at least one, up to the end of [s, e). *v gets the value.
static bool all_digits(const char *s, const char *e, int *v) {
    if (s >= e) return false;
    long x = 0;
    for (const char *p = s; p < e; p++) {
        if (*p < '0' || *p > '9') return false;
        x = x * 10 + (*p - '0');
        if (x > 100000000L) return false;
    }
    *v = (int)x;
    return true;
}

typedef struct { int file, line, col; } Loc;

// The text after ';@src'. 'FILE:LINE', 'FILE:LINE:COL' or '-'. FILE may hold ':' (a drive letter);
// the LAST ':' (or the last two) separate it from the numbers. false: not understood.
static bool parse_loc(const char *t, Loc *out) {
    while (*t == ' ' || *t == '\t') t++;
    const char *e = t + strlen(t);
    while (e > t && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) e--;
    if (e == t) return false;
    if (e - t == 1 && *t == '-') { out->file = LOC_NONE; out->line = out->col = 0; return true; }

    const char *c1 = e;
    while (c1 > t && c1[-1] != ':') c1--;                 // c1: just after the last ':'
    if (c1 == t) return false;                             // no ':' at all
    int a;
    if (!all_digits(c1, e, &a)) return false;
    const char *name_end = c1 - 1;                         // the ':' itself
    int line = a, col = 0;
    const char *c2 = name_end;
    while (c2 > t && c2[-1] != ':') c2--;                  // c2: just after the previous ':'
    int b;
    // FILE:LINE:COL -- only if the part in front is a plausible file name (not a lone drive letter)
    if (c2 > t && all_digits(c2, name_end, &b) && (c2 - 1) - t >= 2) {
        line = b; col = a;
        name_end = c2 - 1;
    }
    if (name_end <= t) return false;
    int f = intern_file(t, (size_t)(name_end - t));
    out->file = f; out->line = line; out->col = col;
    return true;
}

void srcmarks_inherit(AsmNode *dst, const AsmNode *src) {
    if (!dst || !src) return;
    dst->loc_file = src->loc_file; dst->loc_line = src->loc_line; dst->loc_col = src->loc_col;
}

static bool is_marker_line(const AsmNode *n, const char **text) {
    if (n->type != OP_OTHER || n->debug_note) return false;
    const char *p = n->raw;
    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, ";@src", 5) != 0) return false;
    if (p[5] != '\0' && p[5] != ' ' && p[5] != '\t') return false;
    *text = p + 5;
    return true;
}

static bool is_instruction(const AsmNode *n) {
    return n->type != OP_LABEL && n->type != OP_OTHER && n->type <= OP_POW;
}

bool srcmarks_attach(AsmNode *head) {
    Loc cur = {LOC_UNSET, 0, 0};
    bool any = false;
    AsmNode *n = head ? head->next : NULL;
    while (n) {
        AsmNode *next = n->next;
        const char *txt;
        if (is_marker_line(n, &txt)) {
            Loc l;
            if (parse_loc(txt, &l)) {
                cur = l;
                any = true;
                n->prev->next = n->next;                   // out of the list
                if (n->next) n->next->prev = n->prev;
                free(n->raw);
                free(n);
            } else {
                fprintf(stderr, "v32opt: warning: ignoring a ';@src' line it cannot read (input line %d: %s)\n",
                        n->src_line, n->raw);
            }
        } else if (is_instruction(n)) {
            n->loc_file = cur.file; n->loc_line = cur.line; n->loc_col = cur.col;
        }
        n = next;
    }
    g_present = any;
    return any;
}

// ---------------------------------------------------------------------------
// Where a function starts: the labels v32opt itself treats as function entries.
// ---------------------------------------------------------------------------
static bool starts_function(const AsmNode *n) {
    if (n->type != OP_LABEL) return false;
    char lbl[128];
    get_label_name(n, lbl, sizeof(lbl));
    if (strcmp(lbl, "__global_scope_initialization") == 0) return true;
    if (strncmp(lbl, "__function_", 11) == 0 && !is_fn_return_label(n)) return true;
    return hints_fn(lbl) != NULL;
}

static void format_marker(char *buf, size_t sz, int file, int line, int col) {
    if (file == LOC_NONE || file == LOC_UNSET) { snprintf(buf, sz, ";@src -"); return; }
    const char *f = srcmarks_file(file);
    if (col > 0) snprintf(buf, sz, ";@src %s:%d:%d", f ? f : "?", line, col);
    else         snprintf(buf, sz, ";@src %s:%d", f ? f : "?", line);
}

void srcmarks_emit(AsmNode *head) {
    if (!g_present || config.strip_src || !head) return;

    Loc state = {LOC_UNSET, 0, 0};          // what the text written so far says
    bool force = false;                      // at a function entry: say it again
    for (AsmNode *n = head->next; n; n = n->next) {
        if (n->type == OP_LABEL) {
            if (starts_function(n)) force = true;
            continue;
        }
        if (!is_instruction(n)) continue;

        if (n->loc_file == LOC_UNSET) {
            // made by a pass: it continues the code before it
            n->loc_file = state.file; n->loc_line = state.line; n->loc_col = state.col;
        }
        bool differs = n->loc_file != state.file || n->loc_line != state.line || n->loc_col != state.col;
        if (n->loc_file == LOC_UNSET) { force = false; continue; }     // nothing to say before the first marker
        if (differs || force) {
            char buf[512];
            format_marker(buf, sizeof(buf), n->loc_file, n->loc_line, n->loc_col);
            AsmNode *m = create_node(buf, OP_OTHER, NULL, NULL, NULL);
            m->prev = n->prev; m->next = n;
            n->prev->next = m; n->prev = m;
            state.file = n->loc_file; state.line = n->loc_line; state.col = n->loc_col;
        }
        force = false;
    }
}

// ---------------------------------------------------------------------------
// --check-src: what a debugger relies on.
//   1. in a file that has markers, an instruction before the first marker has no source at
//      all (only a ';@src -' region may say so on purpose);
//   2. an instruction in a function with ';@fn F lines=A..B' lies inside A..B, or inside the
//      range of another function (an inlined body keeps its callee's lines).
// Returns the number of problems; the first few are described on stderr.
// ---------------------------------------------------------------------------
int srcmarks_check(AsmNode *head) {
    if (!head) return 0;
    int problems = 0, shown = 0;
    const int MAX_SHOWN = 20;
    bool seen_marker = false;
    const FnHints *fn = NULL;
    char fname[128] = "";
    Loc state = {LOC_UNSET, 0, 0};

    for (AsmNode *n = head->next; n; n = n->next) {
        if (n->type == OP_LABEL) {
            if (starts_function(n)) {
                get_label_name(n, fname, sizeof(fname));
                fn = hints_fn(fname);
            }
            continue;
        }
        if (!is_instruction(n)) continue;
        int f = n->loc_file ? n->loc_file : state.file;
        int l = n->loc_file ? n->loc_line : state.line;
        if (n->loc_file) { state.file = n->loc_file; state.line = n->loc_line; state.col = n->loc_col; seen_marker = true; }

        if (g_present && !seen_marker && f == LOC_UNSET) {
            problems++;
            if (shown++ < MAX_SHOWN)
                fprintf(stderr, "v32opt: --check-src: instruction without a source marker before the first marker: %s\n", n->raw);
            continue;
        }
        if (f > 0 && fn && fn->line_lo > 0 && (l < fn->line_lo || l > fn->line_hi)) {
            // maybe an inlined body: inside another function's range?
            bool elsewhere = false;
            for (AsmNode *m = head->next; m && !elsewhere; m = m->next) {
                if (m->type != OP_LABEL || !starts_function(m)) continue;
                char nm[128];
                get_label_name(m, nm, sizeof(nm));
                const FnHints *g = hints_fn(nm);
                if (g && g != fn && g->line_lo > 0 && l >= g->line_lo && l <= g->line_hi) elsewhere = true;
            }
            if (!elsewhere) {
                problems++;
                if (shown++ < MAX_SHOWN)
                    fprintf(stderr, "v32opt: --check-src: in %s (lines %d..%d) an instruction is attributed to %s:%d: %s\n",
                            fname, fn->line_lo, fn->line_hi, srcmarks_file(f), l, n->raw);
            }
        }
    }
    if (problems > shown)
        fprintf(stderr, "v32opt: --check-src: ... and %d more\n", problems - shown);
    return problems;
}
