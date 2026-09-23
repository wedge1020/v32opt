#ifndef __V32OPT_PROMOTE_H
#define __V32OPT_PROMOTE_H

// ===========================================================================
// promote.h -- experimental memory-to-register promotion passes
//              (src/promote.c)
// ---------------------------------------------------------------------------
// Opt-in only (not part of any -O level): stack-slot-to-register promotion
// for leaf functions, for CALL-free segments of any function, and for
// single CALL-free loops.
// ===========================================================================

void  promote_operand_to_reg       (Operand    *, const char *);
bool  is_inside_loop               (const char *, AsmNode *, AsmNode *);
int   pass_promote_stack_slots     (AsmNode *);   // -fpromote-leaf
int   pass_promote_regs            (AsmNode *);   // -fpromote-regs
int   pass_promote_loop_registers  (AsmNode *);   // -fpromote-loops

#endif
