#include "v32opt.h"

extern OptConfig config;

// ============================================================================
// layout.c -- v32lua's cartridge layout labels stay where the compiler put them.
//
// Since v32lua 20261120-dev every cart checks two labels once at boot:
//
//     __v32lua_code_end:   after all code (user functions, runtime, __globals_to_nil)
//     __v32lua_rom_end:    after the string pool, at the very end of the file
//
// and panics when code passes 2M ROM words (bit 21 of a function value is the closure flag) or data passes 4M.
// The check is only right if nothing is moved past either label and nothing is appended after the string
// pool. No pass does that; this file makes it a checked guarantee rather than an accident of the passes:
// layout_snapshot() records every label, instruction and directive from __v32lua_code_end to the end of the
// file (comments and blank lines aside, comments stripped off the lines) before the passes run, and
// layout_verify() compares the list just before the output is written. Any difference (an instruction or
// routine after the label, data moved, the label gone or no longer followed by the same text, something after
// __v32lua_rom_end) is an internal error: v32opt says what differs and writes no output.
//
// V32OPT_LAYOUT_SELFTEST=1 appends a RET at the end of the program before the comparison (the test of the check).
//
// A file without __v32lua_code_end (C, hand-written assembly, an older v32lua) is not checked, nor one that already
// has instructions after it (v32lua never writes that; the passes may legitimately tidy such code).
// ============================================================================

typedef struct {
    bool    active;
    char  **lines;
    int     n;
} LayoutSnap;

static LayoutSnap g_snap;

static bool is_label_named(const AsmNode *n, const char *name) {
    if (!n || n->type != OP_LABEL) return false;
    char lbl[128];
    get_label_name(n, lbl, sizeof(lbl));
    return name_eq(lbl, name);
}

// Instructions, labels and directives count; comments and blank lines do not.
static bool layout_significant(const AsmNode *n) {
    if (n->type != OP_OTHER) return true;
    return is_directive_node(n) || is_preprocessor_node(n);
}

static char *layout_text(const AsmNode *n) {
    char stripped[1024], norm[1024];
    strip_comment_from_line(stripped, n->raw ? n->raw : "", sizeof(stripped));
    normalize_whitespace(norm, stripped, sizeof(norm));
    return strdup(trim(norm));
}

static AsmNode *find_code_end(AsmNode *head) {
    for (AsmNode *n = head; n; n = n->next)
        if (is_label_named(n, "__v32lua_code_end")) return n;
    return NULL;
}

static int collect(AsmNode *from, char ***out) {
    int cap = 64, n = 0;
    char **v = malloc((size_t)cap * sizeof(char *));
    for (AsmNode *x = from; x && v; x = x->next) {
        if (!layout_significant(x)) continue;
        if (n == cap) {
            cap *= 2;
            char **nv = realloc(v, (size_t)cap * sizeof(char *));
            if (!nv) break;
            v = nv;
        }
        v[n++] = layout_text(x);
    }
    *out = v;
    return n;
}

static void free_lines(char **v, int n) {
    for (int i = 0; i < n; i++) free(v[i]);
    free(v);
}

void layout_snapshot(AsmNode *head) {
    memset(&g_snap, 0, sizeof(g_snap));
    AsmNode *ce = find_code_end(head);
    if (!ce) return;
    // v32lua puts no instruction after the label. Input that has some (hand-edited) is optimized like any other
    // code, which would change the list: it is not checked.
    for (AsmNode *x = ce->next; x; x = x->next) {
        if (x->type != OP_OTHER && x->type != OP_LABEL) {
            if (config.verbose)
                printf("Layout check: off (instructions after __v32lua_code_end in the input).\n");
            return;
        }
    }
    g_snap.n = collect(ce, &g_snap.lines);
    g_snap.active = g_snap.lines != NULL;
}

// 0: fine (or nothing to check); otherwise the number of problems, reported on stderr.
int layout_verify(AsmNode *head) {
    if (!g_snap.active) return 0;
    // Test hook (testing/lua-layout): play a pass that appends a routine after the string pool.
    const char *st = getenv("V32OPT_LAYOUT_SELFTEST");
    if (st && *st && *st != '0') {
        AsmNode *last = head;
        while (last->next) last = last->next;
        AsmNode *r = create_node("    RET", OP_RET, "RET", NULL, NULL);
        if (r) { last->next = r; r->prev = last; }
    }
    int problems = 0;
    AsmNode *ce = find_code_end(head);
    if (!ce) {
        fprintf(stderr, "v32opt: internal error: the label __v32lua_code_end was removed\n");
        problems++;
    } else {
        char **now = NULL;
        int n = collect(ce, &now);
        int i = 0;
        while (i < n && i < g_snap.n && strcmp(now[i], g_snap.lines[i]) == 0) i++;
        if (i < n || i < g_snap.n) {
            fprintf(stderr, "v32opt: internal error: the cartridge layout after __v32lua_code_end changed "
                            "(item %d: was \"%s\", now \"%s\")\n", i + 1,
                    i < g_snap.n ? g_snap.lines[i] : "<end of file>", i < n ? now[i] : "<end of file>");
            problems++;
        }
        free_lines(now, n);
    }
    free_lines(g_snap.lines, g_snap.n);
    memset(&g_snap, 0, sizeof(g_snap));
    if (problems)
        fprintf(stderr, "v32opt: code or data may not move past __v32lua_code_end / __v32lua_rom_end "
                        "(v32lua's boot-time layout check); no output written\n");
    return problems;
}
