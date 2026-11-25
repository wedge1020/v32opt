#include "v32opt.h"

// ---------------------------------------------------------------
// Returns true if 'node' makes it unsafe to remove an in-flight
// "PUSH Rn ... POP Rn" span at this point: any control transfer, any
// OTHER push/pop (they move SP, and something later in the span --
// or after it -- may depend on exactly how many words are currently
// pushed), any direct read/write of SP or BP, or a write to the
// register the span is trying to protect. A plain READ of that
// register inside the span is fine: nothing here overwrites it, so a
// read sees the same value with or without the push/pop.
// ---------------------------------------------------------------
// Words of locals the function's frame reserves below BP: the N of the
// prologue "MOV BP, SP / ISUB SP, N" found walking back from `from`
// (labels included, up to the function's start). 0 when the prologue has
// no ISUB; -1 when no "MOV BP, SP" is found before a RET / HLT or the start
// of the file (BP is then not known to be this function's frame).
static int frame_locals_before(const AsmNode *from)
{
    int steps = 0;
    for (const AsmNode *q = from ? from->prev : NULL; q && steps < 20000; q = q->prev, steps++) {
        if (q->type == OP_RET || q->type == OP_HLT) return -1;
        if (q->type == OP_MOV && q->has_dst && q->has_src &&
            q->dst_op.mode == MODE_REG && q->src_op.mode == MODE_REG &&
            str_case_eq(q->dst_op.reg, "BP") && str_case_eq(q->src_op.reg, "SP")) {
            const AsmNode *n = q->next;
            while (n && n->type == OP_OTHER) n = n->next;
            if (n && n->type == OP_ISUB && n->has_dst && n->dst_op.mode == MODE_REG &&
                str_case_eq(n->dst_op.reg, "SP") && n->has_src &&
                n->src_op.mode == MODE_IMMEDIATE && !n->src_op.is_float && n->src_op.immediate >= 0)
                return n->src_op.immediate;
            return 0;
        }
    }
    return -1;
}

// Can `node` reach the word a PUSH put below the frame's locals through BP:
// a [BP-k] below the `locals` words the prologue reserved (any [BP-k] when
// that is unknown, locals < 0), or BP copied to another register? A callee
// that keeps its register arguments in its frame ("PUSH R1 ; [BP-1]", then
// "MOV R7, [BP-1]") reads the pushed word as data: removing the PUSH / POP
// loses the argument. (v32lua's __tic80_circ_run; found as broken TIC-80
// circ() in an optimized build of witchem_up.)
static bool reads_pushed_word_via_bp(const AsmNode *node, int locals)
{
    for (int k = 0; k < 2; k++) {
        const Operand *op = k == 0 ? &node->dst_op : &node->src_op;
        if (!(k == 0 ? node->has_dst : node->has_src)) continue;
        if (!str_case_eq(op->reg, "BP")) continue;
        if (op->mode == MODE_REG && k == 1) return true;          // BP copied: could reach anything
        if (node->type == OP_LEA && k == 1) return true;          // a pointer from BP: likewise
        if (op->mode == MODE_INDIRECT && op->offset < 0 && (locals < 0 || -op->offset > locals)) return true;
    }
    return false;
}

static bool blocks_push_pop_removal(AsmNode *node, const char *reg_name)
{
    if (!node) return true;
    if (node->type == OP_LABEL) return true;
    if (str_case_eq(node->mnemonic, "JMP") || str_case_eq(node->mnemonic, "JT") ||
        str_case_eq(node->mnemonic, "JF") || str_case_eq(node->mnemonic, "CALL") ||
        str_case_eq(node->mnemonic, "RET") || str_case_eq(node->mnemonic, "HLT")) return true;
    if (node->type == OP_PUSH || node->type == OP_POP) return true;
    if (modifies_register(node, "SP") || modifies_register(node, "BP")) return true;
    if (node->has_dst && (node->dst_op.mode == MODE_REG || node->dst_op.mode == MODE_INDIRECT) &&
        str_case_eq(node->dst_op.reg, "SP")) return true;
    if (node->has_src && (node->src_op.mode == MODE_REG || node->src_op.mode == MODE_INDIRECT) &&
        str_case_eq(node->src_op.reg, "SP")) return true;
    if (modifies_register(node, reg_name)) return true;
    return false;
}

// ===================================================================
// PEEPHOLE: Adjacent Instruction Pair Elimination
//
// Scans a sliding window of two consecutive instructions and removes
// redundant pairs that cancel each other out.
//
// Patterns handled:
//   - IEQ/INE followed by CIB on same register (CIB redundant)
//   - BNOT x; BNOT x → remove both (double negation cancels)
//   - ISGN x; ISGN x → remove both (sign of a sign is a fixed point)
//   - XOR r1, r2; XOR r1, r2 → remove both (XOR twice = identity)
//   - PUSH r; POP r → remove both (no net stack effect)
//
// Example:
//   Input:  IEQ R1, R2
//           CIB R1
//   Output: (IEQ only, CIB removed)
//
//   Input:  BNOT R4
//           BNOT R4
//   Output: (both removed)
//
// Returns: Number of optimizations applied
// ===================================================================
int peephole_pairs(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr && curr->next)
    {
        AsmNode *n1 = curr;
        AsmNode *n2 = curr->next;

        // --- IEQ/INE + CIB Redundancy ---
        // IEQ/INE already produces a canonical 0/1 result by definition --
        // that's what "equal"/"not-equal" means. CIB (Convert Integer to
        // Boolean) just normalizes an arbitrary integer down to canonical
        // 0/1; on a value that's already exactly 0 or 1, it's a no-op.
        // If they target the same register, the CIB is redundant.
        //
        // BUG FIX: this used to keep the CIB whenever the very next
        // instruction was JT/JF, on the apparent assumption that JT/JF
        // need CIB's normalization to correctly test truthiness --
        // i.e., that CIB has some effect JT/JF specifically depends on.
        // It doesn't: CIB is a pure integer-to-boolean *conversion* with
        // no control-flow role whatsoever (see the OpType enum --
        // CIF/CFI/CIB/CFB are all just conversions), and JT/JF branch on
        // "is this register nonzero," which IEQ/INE's direct 0/1 output
        // already satisfies identically to CIB's output on that same
        // value. There's no case where removing the CIB here changes
        // what JT/JF branches on. This carve-out cost real optimizations
        // in exactly the pattern most likely to matter -- a comparison
        // feeding straight into a branch, i.e. every plain `if`/`while`
        // condition -- for no correctness benefit; removed.
        if ((n1->type == OP_IEQ || n1->type == OP_INE) &&
             n2->type == OP_CIB &&
             n1->dst_op.mode == MODE_REG && n2->dst_op.mode == MODE_REG &&
             str_case_eq(n1->dst_op.reg, n2->dst_op.reg))
        {
            AsmNode *nodes[] = {n2};
            if (remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_PAIRS)) optimizations++;
            continue;
        }

        // ----------------------------------------------------------------
        // PATTERN: Self-Inverting Pairs (Involutions)
        // Identical consecutive operations that cancel each other out.
        //
        // BNOT (bitwise NOT) and ISGN (sign: output is always -1/0/1, a
        // fixed point of ISGN) are true involutions.
        //
        // BUG FIX: NOT (logical NOT: 0 -> 1, anything else -> 0) was
        // removed from this list -- "NOT r; NOT r" is NOT an identity for
        // non-boolean r: it computes CIB(r) (0/1), not r (e.g. r=5 stays 5
        // "optimized" instead of becoming 1). "NEG" was also dropped: it
        // isn't a Vircon32 mnemonic at all.
        // ----------------------------------------------------------------
        if (str_case_eq(n1->mnemonic, "BNOT") ||
            str_case_eq(n1->mnemonic, "ISGN"))
        {
            AsmNode *next = skip_other_nodes(n1->next);

            if (next && str_case_eq(next->mnemonic, n1->mnemonic))
            {
                // Register operands only: an indirect destination's
                // ->reg is just the BASE register, so "BNOT [R1]" vs
                // "BNOT [R1+2]" would textually match on "R1" while
                // addressing different memory words (same bug class as
                // the XOR fix below).
                char *reg1 = (n1->has_dst && n1->dst_op.mode == MODE_REG) ? n1->dst_op.reg :
                             ((n1->has_src && n1->src_op.mode == MODE_REG) ? n1->src_op.reg : NULL);
                char *reg2 = (next->has_dst && next->dst_op.mode == MODE_REG) ? next->dst_op.reg :
                             ((next->has_src && next->src_op.mode == MODE_REG) ? next->src_op.reg : NULL);

                if (reg1 && reg2 && str_case_eq(reg1, reg2))
                {
                    AsmNode *nodes[] = {n1, next};
                    if (remove_with_debug(&curr, nodes, 2, OPT_PEEPHOLE_PAIRS)) optimizations += 2;
                    continue;
                }
            }
        }

        // ----------------------------------------------------------------
        // BONUS PATTERN: Identical XOR Pairs
        // Toggling a register with the exact same value twice cancels out
        //
        // BUG FIX: register destinations only. An indirect destination
        // ("XOR [R1], R2") was compared by base register NAME alone, so
        // "XOR [R1], R2; XOR [R1+4], R2" matched and both were deleted --
        // but they address DIFFERENT memory words.
        // ----------------------------------------------------------------
        if (str_case_eq(n1->mnemonic, "XOR"))
        {
            AsmNode *next = skip_other_nodes(n1->next);

            if (next && str_case_eq(next->mnemonic, "XOR"))
            {
                if (n1->has_dst && next->has_dst &&
                    n1->dst_op.mode == MODE_REG && next->dst_op.mode == MODE_REG &&
                    str_case_eq(n1->dst_op.reg, next->dst_op.reg))
                {
                    bool src_match = false;

                    if (n1->src_op.mode == MODE_REG && next->src_op.mode == MODE_REG) {
                        if (name_eq(n1->src_op.reg, next->src_op.reg)) src_match = true;
                    }
                    else if (n1->src_op.mode == MODE_IMMEDIATE && next->src_op.mode == MODE_IMMEDIATE) {
                        if (name_eq(n1->src_op.raw, next->src_op.raw)) src_match = true;
                    }

                    if (src_match)
                    {
                        AsmNode *nodes[] = {n1, next};
                        if (remove_with_debug(&curr, nodes, 2, OPT_PEEPHOLE_PAIRS)) optimizations += 2;
                        continue;
                    }
                }
            }
        }

        // --- PUSH/POP Pair Elimination (adjacent, OR across a safe span) ---
        // PUSH r; ...; POP r -> remove both, PROVIDED nothing in between
        // writes r, touches SP/BP, transfers control, or itself pushes/pops
        // (see blocks_push_pop_removal()). This subsumes the old
        // adjacent-only match (that's just the zero-instructions-in-between
        // case) and additionally catches the extremely common compiler
        // idiom of "PUSH left-operand; evaluate right-operand; POP
        // left-operand" that v32lua emits around essentially every binary
        // operator -- unconditionally, even when the right-hand side
        // provably makes no CALL and so never needed the protection in the
        // first place.
        if (n1->type == OP_PUSH && n1->dst_op.mode == MODE_REG)
        {
            char *reg_name = n1->dst_op.reg;
            AsmNode *scan = n1->next;
            AsmNode *match = NULL;
            int locals = -2;                     // frame_locals_before(n1), looked up when first needed

            while (scan)
            {
                if (scan->type == OP_OTHER) { scan = scan->next; continue; }

                if (scan->type == OP_POP && scan->dst_op.mode == MODE_REG &&
                    str_case_eq(scan->dst_op.reg, reg_name))
                {
                    match = scan;
                    break;
                }

                if (blocks_push_pop_removal(scan, reg_name)) break;
                if ((scan->has_dst && str_case_eq(scan->dst_op.reg, "BP")) ||
                    (scan->has_src && str_case_eq(scan->src_op.reg, "BP"))) {
                    if (locals == -2) locals = frame_locals_before(n1);
                    if (reads_pushed_word_via_bp(scan, locals)) break;
                }

                scan = scan->next;
            }

            if (match)
            {
                AsmNode *nodes[] = {n1, match};
                if (remove_with_debug(&curr, nodes, 2, OPT_PEEPHOLE_PAIRS)) optimizations += 2;
                continue;
            }
        }

        curr = curr->next;
    }

    return optimizations;
}
