#ifndef __V32OPT_PEEPHOLE_H
#define __V32OPT_PEEPHOLE_H

// ===========================================================================
// peephole.h -- local window peephole passes (src/peephole/*.c)
// ---------------------------------------------------------------------------
// Stateless, linear-time passes that rewrite small local instruction
// sequences. Enabled as a group by -O1; individually via -f<name>.
// ===========================================================================

// Forward scans inside the peephole passes are bounded by this many
// instructions so a pathological input can't make them quadratic.
#define  PEEPHOLE_MAX_SCAN_DISTANCE   256

int  peephole_pairs           (AsmNode *);
int  peephole_algebra         (AsmNode *);
int  peephole_compiler_myopia (AsmNode *);
int  peephole_forwarding      (AsmNode *);
int  peephole_jumps           (AsmNode *);
int  peephole_movs            (AsmNode *);
int  peephole_immediates      (AsmNode *);
int  peephole_reduce          (AsmNode *);
int  peephole_shifts          (AsmNode *);
int  peephole_dead_stores     (AsmNode *);
int  peephole_loads           (AsmNode *);
int  peephole_immediate_prop  (AsmNode *);
int  peephole_jmp_chain       (AsmNode *);
int  peephole_zero_test       (AsmNode *);
int  peephole_dead_regs       (AsmNode *);
int  peephole_copies          (AsmNode *);
int  peephole_loop_invert     (AsmNode *);   // -floop-invert (-O3), src/peephole/loop_invert.c
int  peephole_loop_index      (AsmNode *);   // -floop-index (-O3), src/peephole/loop_index.c
int  peephole_loop_unroll     (AsmNode *);   // -floop-unroll (-O3), src/peephole/loop_unroll.c

#endif
