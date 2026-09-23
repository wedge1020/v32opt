#ifndef __V32OPT_STACK_H
#define __V32OPT_STACK_H

// ===========================================================================
// stack.h -- stack-frame analysis & frame-pointer elimination (src/stack.c)
// ===========================================================================

bool  is_reg_op            (AsmNode *, const char *);
bool  references_bp_direct (AsmNode *);
int   omit_frame_pointers  (AsmNode *);

#endif
