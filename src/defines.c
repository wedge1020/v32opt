#include "v32opt.h"

// ===================================================================
// %define SYMBOL TABLE
//
// Minimal model of the assembler's "%define NAME VALUE" directive:
// name -> defining node, value text, and (when the value is a plain
// numeric literal) its parsed int/float value.
//
// Today this is used for SAFETY: Vircon32's %define is a textual,
// in-order substitution, so a symbol must be defined above its first
// use. Any pass that moves code upward (inline, in particular) must not
// carry a use above its definition -- define_visible_at() answers that.
//
// It is also the groundwork for VALUE resolution (letting constant
// folding / strength reduction see "IMUL R0, PICO8_MAP_WIDTH" as
// "IMUL R0, 128"); define_numeric_value() exposes that, but no pass
// consumes it yet. A symbol defined more than once with differing
// values is "poisoned" and never resolved.
// ===================================================================

typedef struct {
    char     name[128];
    char     value[128];
    AsmNode *node;          // first defining node
    bool     poisoned;      // redefined with a different value
} DefineEntry;

static DefineEntry *g_defs      = NULL;
static int          g_def_count = 0;

static int cmp_def(const void *a, const void *b) {
    return strcasecmp(((const DefineEntry *)a)->name, ((const DefineEntry *)b)->name);
}

// Parse "%define NAME VALUE [; comment]" from node->raw. Returns false
// if the node isn't a %define.
static bool parse_define(const AsmNode *node, char *name, size_t nsz, char *value, size_t vsz) {
    if (!is_preprocessor_node(node)) return false;
    char buf[1024];
    strip_comment_from_line(buf, node->raw, sizeof(buf));
    char *p = trim(buf);
    if (strncasecmp(p, "%define", 7) != 0 || !isspace((unsigned char)p[7])) return false;
    p += 7;
    while (isspace((unsigned char)*p)) p++;
    char *n = p;
    while (*p && !isspace((unsigned char)*p)) p++;
    if (*p) *p++ = '\0';
    safe_str_copy(name, n, nsz);
    safe_str_copy(value, trim(p), vsz);
    return name[0] != '\0';
}

void defines_free(void) {
    free(g_defs);
    g_defs = NULL;
    g_def_count = 0;
}

int defines_build(AsmNode *head) {
    defines_free();
    int cap = 0;
    for (AsmNode *n = head; n; n = n->next)
        if (is_preprocessor_node(n)) cap++;
    if (cap == 0) return 0;
    g_defs = calloc((size_t)cap, sizeof(DefineEntry));
    if (!g_defs) return 0;

    for (AsmNode *n = head; n; n = n->next) {
        char name[128], value[128];
        if (!parse_define(n, name, sizeof(name), value, sizeof(value))) continue;
        // linear dedupe is fine: defines number in the hundreds
        DefineEntry *e = NULL;
        for (int i = 0; i < g_def_count; i++)
            if (str_case_eq(g_defs[i].name, name)) { e = &g_defs[i]; break; }
        if (e) {
            if (!str_case_eq(e->value, value)) e->poisoned = true;
            continue;
        }
        e = &g_defs[g_def_count++];
        safe_str_copy(e->name, name, sizeof(e->name));
        safe_str_copy(e->value, value, sizeof(e->value));
        e->node = n;
    }
    qsort(g_defs, (size_t)g_def_count, sizeof(DefineEntry), cmp_def);
    return g_def_count;
}

static const DefineEntry *lookup(const char *sym) {
    if (!g_defs || !sym || !sym[0]) return NULL;
    DefineEntry key;
    safe_str_copy(key.name, sym, sizeof(key.name));
    return bsearch(&key, g_defs, (size_t)g_def_count, sizeof(DefineEntry), cmp_def);
}

bool define_exists(const char *sym) { return lookup(sym) != NULL; }

// Is the definition of 'sym' textually above 'site'? (true for symbols
// that aren't %defines at all -- labels and assembler built-ins are not
// order-sensitive). Walks backward from site; only called for operands
// that ARE defines, and only by code-moving passes, so cost is bounded.
bool define_visible_at(const char *sym, const AsmNode *site) {
    const DefineEntry *e = lookup(sym);
    if (!e) return true;
    for (const AsmNode *n = site; n; n = n->prev)
        if (n == e->node) return true;
    return false;
}

// Every %define symbol referenced by 'node' (either operand, including
// the base of an indirect "[SYM]" / "[SYM+N]") is defined above 'site'.
bool node_defines_visible_at(const AsmNode *node, const AsmNode *site) {
    const Operand *ops[2] = { &node->dst_op, &node->src_op };
    for (int i = 0; i < 2; i++) {
        const Operand *op = ops[i];
        const char *sym = (op->mode == MODE_INDIRECT) ? op->reg : op->raw;
        char buf[128];
        safe_str_copy(buf, sym, sizeof(buf));
        if (!define_visible_at(trim(buf), site)) return false;
    }
    return true;
}

// Numeric value of a %define whose value is a plain literal. Not yet
// used by any pass (see header comment).
bool define_numeric_value(const char *sym, long *ival, float *fval, bool *is_float) {
    const DefineEntry *e = lookup(sym);
    if (!e || e->poisoned || !is_immediate_string(e->value)) return false;
    char *end = NULL;
    if (strchr(e->value, '.')) {
        float f = strtof(e->value, &end);
        if (end && *end) return false;
        if (fval) *fval = f;
        if (is_float) *is_float = true;
    } else {
        long v = (long)strtoul(e->value, &end, 0);
        if (e->value[0] == '-') v = strtol(e->value, &end, 0);
        if (end && *end) return false;
        if (ival) *ival = v;
        if (is_float) *is_float = false;
    }
    return true;
}
