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
bool     promote_function_complete (AsmNode *entry);            // ... and its frame ends there (no nested definition, no fall-through)
void     promote_bounds_begin   (AsmNode *head);                // collect the CALL targets the boundaries need (each pass, before the above)
void     promote_bounds_end     (void);
bool     promote_labels_private (AsmNode *first, AsmNode *last, const AsmNode *except);   // no label of the range but `except` named outside it
bool     promote_bp_value_escapes (AsmNode *n);   // n captures or destroys BP's value ("MOV R1, BP", "LEA R1, [BP-2]")
bool     promote_sp_value_escapes (AsmNode *n);   // n copies SP's value out ("MOV R1, SP", "LEA R1, [SP+1]")


// ---- the register rules of the promote passes, for loop-strength (thin wrappers, promote.c) ----
#define MAX_EPILOGUES 8
typedef struct {
    AsmNode *entry_end;             // last node of the prologue: loads go right after it
    bool     saved[16];             // registers the function saves/restores
    bool     lua;                   // caller-save ABI (v32lua, v32cc): nothing needs saving
    bool     can_save;              // a push/pop can be added (every exit is a recognised epilogue, no [SP+k] reaches it)
    int      n_epi;
    AsmNode *epi_first[MAX_EPILOGUES];   // our pops go right before this node, per epilogue
} FrameInfo;

bool promote_loop_frame         (FrameInfo *fi, AsmNode *n);          // frame_info() of n's function; false: no register usable
bool promote_frame_save_reg     (FrameInfo *fi, int r, OptType t);    // frame_save_reg(): the function saves Rr from now on
int  promote_pick_loop_register (AsmNode *from, AsmNode *last, const FrameInfo *fi, unsigned exclude, bool *fresh);

#endif
