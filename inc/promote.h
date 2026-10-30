#ifndef __V32OPT_PROMOTE_H
#define __V32OPT_PROMOTE_H

// ===========================================================================
// promote.h -- memory-to-register promotion passes (src/promote.c)
// ---------------------------------------------------------------------------
// Part of -O2 and up (-fno-promote-<leaf|regs|loops> to leave one out):
// stack-slot-to-register promotion for leaf functions, for CALL-free
// segments of any function, and for single CALL-free loops.
// ===========================================================================

void  promote_operand_to_reg       (Operand    *, const char *);
int   pass_promote_stack_slots     (AsmNode *);   // -fpromote-leaf
int   pass_promote_regs            (AsmNode *);   // -fpromote-regs
int   pass_promote_loop_registers  (AsmNode *);   // -fpromote-loops

bool promote_frame_unaliased(AsmNode *n);   // frame slots of n's function reachable only via [BP+/-N]

bool     promote_function_entry (const AsmNode *n);     // a function's entry label ("__function_X", "__global_scope_initialization")
AsmNode *promote_function_last  (AsmNode *entry);               // the last node of the function that starts at `entry`

#endif
