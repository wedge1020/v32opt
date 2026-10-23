#ifndef __V32OPT_PROMOTE_H
#define __V32OPT_PROMOTE_H

// ===========================================================================
// promote.h -- memory-to-register promotion passes (src/promote.c)
// ---------------------------------------------------------------------------
// stack-slot-to-register promotion for leaf functions and for CALL-free
// segments of any function (-fpromote-leaf, -fpromote-regs: part of -O2
// and up), and for single CALL-free loops (-fpromote-loops: opt-in).
// ===========================================================================

void  promote_operand_to_reg       (Operand    *, const char *);
int   pass_promote_stack_slots     (AsmNode *);   // -fpromote-leaf
int   pass_promote_regs            (AsmNode *);   // -fpromote-regs
int   pass_promote_loop_registers  (AsmNode *);   // -fpromote-loops

#endif
