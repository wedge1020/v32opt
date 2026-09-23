#ifndef __V32OPT_INLINE_H
#define __V32OPT_INLINE_H

// ===========================================================================
// inline.h -- interprocedural trivial-leaf-function inlining (src/inline.c)
// ===========================================================================

#define  MAX_INLINE_CANDIDATES        64
#define  MAX_BODY_INS                 32
#define  MAX_FUNCTIONS                4096

typedef struct {
    char name[128];
    AsmNode *body_nodes[MAX_BODY_INS];
    int body_count;
} InlineCandidate;

// ---------------------------------------------------------------------------
// Inlining control globals (defined in main.c, set via -finline-call-limit
// and -finline-exclude, consumed by inline.c)
// ---------------------------------------------------------------------------
extern int   g_inline_call_limit;
extern int   g_inline_calls_so_far;
extern char  g_inline_exclude_name[1024];

int  inline_trivial_functions (AsmNode *);

#endif
