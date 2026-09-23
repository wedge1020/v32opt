#ifndef __V32OPT_H
#define __V32OPT_H

// ===========================================================================
// v32opt -- a modular, multi-pass assembly optimizer for the Vircon32
//           fantasy console.  https://github.com/wedge1020/v32opt
// ===========================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include <getopt.h>

// ---------------------------------------------------------------------------
// Project metadata
// ---------------------------------------------------------------------------
#define  VERSION                      "20260923-dev"
#define  AUTHOR                       "Matthew Haas"
#define  URL                          "https://github.com/wedge1020/v32opt"

#define  MAX_OPTIMIZATION_ALGORITHMS  21

// ---------------------------------------------------------------------------
// Optimization pass registry -- indices into opt_type_names[] and the
// per-pass tallies in main.c. NOTE: this OptType (pass id) is distinct
// from asm.h's OpType (instruction opcode).
// ---------------------------------------------------------------------------
typedef enum
{
    OPT_PEEPHOLE_ALGEBRA,
    OPT_PEEPHOLE_COMPILER_MYOPIA,
    OPT_PEEPHOLE_DEAD_STORES,
    OPT_PEEPHOLE_FORWARDING,
    OPT_PEEPHOLE_IMMEDIATE_PROP,
    OPT_PEEPHOLE_IMMEDIATES,
    OPT_PEEPHOLE_JMP_CHAIN,
    OPT_PEEPHOLE_JUMPS,
    OPT_PEEPHOLE_LOADS,
    OPT_PEEPHOLE_MOVS,
    OPT_PEEPHOLE_PAIRS,
    OPT_PEEPHOLE_REDUCE,
    OPT_PEEPHOLE_SHIFTS,
    OPT_CONSTANT_FOLDING,
    OPT_CSE,
    OPT_DCE,
    OPT_OMIT_FRAME_POINTERS,
    OPT_INLINE,
    OPT_PROMOTE_LEAF,
    OPT_PROMOTE_LOOPS,
    OPT_PROMOTE_REGS
} OptType;

// ---------------------------------------------------------------------------
// Section headers (project headers live in inc/, peers of src/)
// ---------------------------------------------------------------------------
#include "asm.h"        // instruction/operand model, parser & writer
#include "peephole.h"   // local window passes (src/peephole/)
#include "dataflow.h"   // CFG, constant folding, CSE, DCE
#include "inline.h"     // trivial leaf-function inlining
#include "stack.h"      // frame-pointer elimination
#include "promote.h"    // experimental stack-slot promotion

// ---------------------------------------------------------------------------
// Language mode -- C (default) or Lua. Lua mode makes the value-tracking
// passes aware of v32lua's NaN-boxed type system (BOXED_* immediates).
// Cross-cutting: set from the command line, consulted by several passes.
// ---------------------------------------------------------------------------
typedef enum {
    LANG_C,      // Default: plain Vircon32 assembly semantics
    LANG_LUA,    // Lua mode: aware of boxed type system
    LANG_MAX
} LangMode;

bool  is_lua_mode          (void);
bool  is_boxed_type_operand(const Operand *);
bool  is_boxed_tagging     (AsmNode *);

// ---------------------------------------------------------------------------
// Optimization configuration (set by process_args, read by main.c and
// the individual passes)
// ---------------------------------------------------------------------------
typedef struct {
    bool     verbose;
    bool     testing;
    bool     debug;
    bool     opt_peephole_algebra;
    bool     opt_peephole_compiler_myopia;
    bool     opt_peephole_dead_stores;
    bool     opt_peephole_forwarding;
    bool     opt_peephole_immediate_prop;
    bool     opt_peephole_immediates;
    bool     opt_peephole_jmp_chain;
    bool     opt_peephole_jumps;
    bool     opt_peephole_loads;
    bool     opt_peephole_movs;
    bool     opt_peephole_pairs;
    bool     opt_peephole_reduce;
    bool     opt_peephole_shifts;
    bool     opt_constant_folding;
    bool     opt_cse;
    bool     opt_dce;
    bool     opt_omit_frame_pointers;
    bool     opt_inline;
    int      opt_inline_call_limit;
    int      opt_inline_max_body_ins;
    bool     opt_promote_leaf;
    bool     opt_promote_loops;
    bool     opt_promote_regs;
    LangMode lang_mode;
} OptConfig;

extern OptConfig  config;

// ---------------------------------------------------------------------------
// Pass-name table (defined in tools.c, used for -v/-t reporting and for
// -d debug comments)
// ---------------------------------------------------------------------------
extern const char *opt_type_names[];

// ---------------------------------------------------------------------------
// Trigger cap (--trigger-max=N): a single global choke point gating whether
// any pass may COMMIT a transformation (deleted node, in-place rewrite, or
// spliced load/store). Exists so a miscompile can be bisected: re-run with
// increasing N until the output breaks; the Nth transform applied, in
// program order across all passes, is the culprit.
// ---------------------------------------------------------------------------
extern long  g_trigger_max;
extern long  g_trigger_count;

bool  trigger_allowed    (void);
void  insert_debug_comment (AsmNode *, OptType, const char *);
bool  remove_with_debug  (AsmNode **, AsmNode *nodes[], int, OptType);

// ---------------------------------------------------------------------------
// Command-line processing (args.c)
// ---------------------------------------------------------------------------
void  print_usage  (const char *);
void  process_args (int,         char **,
                    OptConfig *, char  *,
                    size_t,      char  *,
                    size_t,      char  *,
                    size_t,      int   *);

#endif
