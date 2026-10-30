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
//   ;@loop hot unroll=4                    before a loop's start label
//   mov R0, [global_flag]   ;@v            this access is volatile
// ===========================================================================

#define  NH_VOLATILE   0x1u      // AsmNode.hints: a volatile memory access
// On a label node (a loop's start label), from a ';@loop' line before it:
#define  NH_LOOP_HOT       0x02u    //   'hot'
#define  NH_LOOP_COLD      0x04u    //   'cold'
#define  NH_LOOP_NOUNROLL  0x08u    //   'nounroll'
// On every label node: the heat of the function it sits in (';@fn ... hot|cold'), inherited by its loops
#define  NH_FN_HOT         0x10u
#define  NH_FN_COLD        0x20u
// ';@cold' before a label (hints.c): every node from that label to the first unconditional JMP/RET/HLT
#define  NH_COLD_BLK       0x40u    //   is a rarely taken block (a hint about frequency, never about correctness)
// On a loop's start label: ';@loop kind=for ...' gave facts about the loop (hints_loop_facts)
#define  NH_LOOP_FACTS     0x80u
// 'unroll=N' / 'unroll=full': the count in the high half of AsmNode.hints (0: no request)
#define  NH_UNROLL_SHIFT   16
#define  NH_UNROLL_MASK    0xFFFF0000u
#define  HINT_UNROLL_FULL  0xFFFF

typedef enum {
    ABI_AUTO,                    // default: the file's ';@hints' header decides, else stock
    ABI_CALLEE_SAVE,             // the stock Vircon32 compiler: callees preserve R1 and up
    ABI_CALLER_SAVE              // v32cc, v32lua: nothing survives a CALL, callers save
} AbiMode;

typedef struct {
    char            name[128];   // the function's label, without the colon
    bool            noinline;
    bool            inline_kw;       // 'inline': the source asked for it (a soft request: a larger size limit)
    bool            alwaysinline;    // 'alwaysinline': inline it whatever its size (up to MAX_BODY_INS)
    bool            hot;             // 'hot': inline more readily, loops count as hot when no profile says otherwise
    bool            cold;            // 'cold': do not inline it; its loops are not grown (no unrolling, no inversion)
    bool            noreturn;
    bool            leaf;        // informational: v32opt still checks
    bool            naked;
    bool            escapes_known;   // an 'escapes=' attribute was given
    bool            escapes_all;     // ... and it said 'all' (or the list was unusable)
    bool            any_escaped;     // at least one slot is listed
    unsigned char   escaped[256];    // BP-relative word offset + 128 -> 1 if its address is taken
    bool            clobbers_known;  // a 'clobbers=' attribute was given (and could be read)
    unsigned        clobbers;        // ... bit n = Rn may be written by a call to this function (and what it calls)
    int             line_lo, line_hi;    // 'lines=A..B': the function's source range (0: not given)
} FnHints;

// Facts about a counted loop from ';@loop kind=for counter=-9 limit=-7 step=-8 stepval=1 from=1 to=200'.
// Frame words are BP-relative offsets (-9 = [BP-9]). All of them are promises of the compiler: the counter is
// written only by the loop's increment, limit and step only before the loop, none has its address taken, and the
// body never writes them. (v32opt checks what it cheaply can before using them: promote.c / lua_checks.c.)
typedef struct {
    const AsmNode *label;                       // the loop's start label
    bool  has_counter, has_limit, has_step;
    int   counter, limit, step;
    bool  has_stepval, has_from, has_to;
    long  stepval, from, to;
} LoopFacts;

const LoopFacts *hints_loop_facts (const AsmNode *loop_label);   // NULL: none (or the label was cloned)
bool           node_in_cold_block (const AsmNode *);             // the node lies in a ';@cold' block
const FnHints *hints_fn_of_call   (const AsmNode *call);         // the ';@fn' of a direct CALL's target, or NULL

bool           hints_parse     (AsmNode *head);        // scan a freshly parsed program; true if any hint was seen
void           hints_free      (void);
bool           abi_caller_save (void);                 // the calling convention in force for this run
const FnHints *hints_fn        (const char *label);    // NULL: no ';@fn' line for it
bool           hints_fn_slot_escapes (const FnHints *, int bp_offset);
bool           node_is_volatile(const AsmNode *);
int            hints_static_heat (const AsmNode *loop_label);    // -1 cold, 0 unknown, +1 hot: from ';@loop' / the function's ';@fn' (profile.c adds the measured one)
int            hints_loop_unroll (const AsmNode *loop_label);    // 0: no request, 1: nounroll, N >= 2: unroll N, HINT_UNROLL_FULL: as far as possible
int            hints_verify    (AsmNode *head);        // safety net before output: warns about a volatile access a pass turned into a non-access

#endif
