#ifndef __V32OPT_HINTS_H
#define __V32OPT_HINTS_H

// ===========================================================================
// hints.h -- compiler hints (src/hints.c)
// ---------------------------------------------------------------------------
// A compiler (v32cc) may tell v32opt what it knows about the program in
// ';@' comment lines. They are ordinary comments to the assembler, so a file
// that carries them assembles exactly as before, and v32opt without them (or a
// v32opt that does not understand a line) loses nothing: every hint only ever
// LIFTS a conservative assumption, and an absent or unrecognized hint keeps it.
//
// See doc/hints.md for the format. In short:
//
//   ;@hints 1 abi=caller-save              file header (once, anywhere)
//   ;@fn __function_f noinline escapes=-5..-3,-1
//                                          one per function, named by its label
//   mov R0, [global_flag]   ;@v            this access is volatile
// ===========================================================================

#define  NH_VOLATILE   0x1u      // AsmNode.hints: a volatile memory access

typedef enum {
    ABI_AUTO,                    // default: the file's ';@hints' header decides, else stock
    ABI_CALLEE_SAVE,             // the stock Vircon32 compiler: callees preserve R1 and up
    ABI_CALLER_SAVE              // v32cc, v32lua: nothing survives a CALL, callers save
} AbiMode;

typedef struct {
    char            name[128];   // the function's label, without the colon
    bool            noinline;
    bool            noreturn;
    bool            leaf;        // informational: v32opt still checks
    bool            naked;
    bool            escapes_known;   // an 'escapes=' attribute was given
    bool            escapes_all;     // ... and it said 'all' (or the list was unusable)
    bool            any_escaped;     // at least one slot is listed
    unsigned char   escaped[256];    // BP-relative word offset + 128 -> 1 if its address is taken
} FnHints;

bool           hints_parse     (AsmNode *head);        // scan a freshly parsed program; true if any hint was seen
void           hints_free      (void);
bool           abi_caller_save (void);                 // the calling convention in force for this run
const FnHints *hints_fn        (const char *label);    // NULL: no ';@fn' line for it
bool           hints_fn_slot_escapes (const FnHints *, int bp_offset);
bool           node_is_volatile(const AsmNode *);
int            hints_verify    (AsmNode *head);        // safety net before output: warns about a volatile access a pass turned into a non-access

#endif
