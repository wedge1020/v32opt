#ifndef __V32OPT_SRCMARKS_H
#define __V32OPT_SRCMARKS_H

// ===========================================================================
// srcmarks.h -- source locations carried through the passes (src/srcmarks.c)
// ---------------------------------------------------------------------------
// A compiler run with -g writes a comment line before the instructions it
// describes (spec: V32DBG-SPEC.md, doc/srcmarks.md):
//
//   ;@src FILE:LINE          all following instructions come from FILE line LINE
//   ;@src FILE:LINE:COL      (optional) with a column
//   ;@src -                  the following instructions have no source
//
// Kept as separate lines in the node list, such markers would be left behind
// by every pass that moves, copies or deletes code. So they are taken out of
// the list when the file is read and stored on the instructions themselves
// (AsmNode.loc_*); clone_node copies them (unrolling, inlining and loop
// inversion therefore copy the location of the original, and an inlined body
// keeps the CALLEE's file:line), a rewrite in place keeps them, a deleted
// instruction takes its location with it. Just before the file is written the
// markers are put back: one wherever the location changes from one
// instruction to the next in the output order, and one at the first
// instruction of every function.
// ===========================================================================

#define  LOC_UNSET   0            // AsmNode.loc_file: no marker applies (yet)
#define  LOC_NONE   -1            //   ';@src -'

bool        srcmarks_attach (AsmNode *head);                 // take the markers out of the list; true if any
void        srcmarks_emit   (AsmNode *head);                 // put them back, as the output needs them
int         srcmarks_check  (AsmNode *head);                 // --check-src: report problems on stderr, return their number
const char *srcmarks_file   (int loc_file);                  // name of file number n (1-based), or NULL
bool        srcmarks_present(void);
void        srcmarks_inherit(AsmNode *dst, const AsmNode *src);   // a node a pass made up gets the location of `src`                          // the input had markers
void        srcmarks_free   (void);

#endif
