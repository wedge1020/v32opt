#ifndef __V32OPT_LOOPS_H
#define __V32OPT_LOOPS_H

// ===========================================================================
// loops.h -- helpers shared by the loop passes (loop-invert is self-contained;
// loop-unroll and loop-index use these): a label table with reference counts
// and a few instruction-pattern utilities. Implemented in src/loops.c.
// ===========================================================================

typedef struct {
    char     name[128];
    AsmNode *node;
    bool     duplicate;
    int      refs;           // occurrences as a token in non-label, non-directive lines
} LoopLabel;

extern LoopLabel *g_loop_labels;
extern int        g_loop_labels_n;

void        loop_labels_build   (AsmNode *head);
void        loop_labels_free    (void);
LoopLabel  *loop_label_find     (const char *name);
bool        loop_ident_char     (int c);
bool        loop_is_comment     (const AsmNode *n);
AsmNode    *loop_next_code      (AsmNode *n);                    // next node that is not a comment
const char *loop_jump_target    (const AsmNode *n, char *buf, size_t sz);  // JMP/JT/JF label operand
bool        loop_imm_value      (const Operand *op, long *v);    // plain integer immediate
void        loop_link_before    (AsmNode *pos, AsmNode *nn);
bool        loop_mentions_reg   (const AsmNode *n, const char *reg);  // register name appears as a token

// A top-tested counted loop, as left by the compilers and promote-*:
//
//   start:  MOV Rc, Ri / [loads of the bound ...] / I<cmp> Rc, <bound> / JF Rc, end
//           body
//           IADD/ISUB Ri, <imm>          (the step)
//           JMP start
//   end:                                  (right after the JMP)
//
// 'start' is referenced only by the JMP. Nothing else is promised: the passes
// check the body and the init themselves.
typedef struct {
    AsmNode *jmp, *start, *test_first, *cmp, *jf, *first, *step;
    int      test_len;               // instructions from test_first to jf
    char     ri[32], rc[32];
    char     end_name[128];
    OpType   cmp_type;
    long     stepv;
    bool     bound_imm;              // test is exactly MOV / compare-with-immediate / JF
    long     bound;                  // valid when bound_imm
} CountedLoop;

bool        loop_parse_counted  (AsmNode *jmp, CountedLoop *L);   // needs loop_labels_build()

typedef struct { char from[128]; char to[160]; } LoopRename;
// A copy of 'src' with jump targets (and a label's own name) renamed per the table.
AsmNode    *loop_clone_renamed  (const AsmNode *src, const LoopRename *r, int n);

#endif
