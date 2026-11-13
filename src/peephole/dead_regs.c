#include "v32opt.h"
#include <stdint.h>

// ===================================================================
// PEEPHOLE: DEAD REGISTER WRITES  (-fpeephole-dead-regs)
// PEEPHOLE: COPY CLEANUP          (-fpeephole-copies)
//
// Compiler output, and above all the output of the promote passes, is full
// of register copies whose only purpose was to carry a value one step:
//
//       MOV  R0, R4              ; R0 is overwritten before anyone reads it
//       MOV  R1, R6              ; R1 is never read again
//       ILT  R0, R6
//       ...
//       MOV  R1, R4              ; copy in ...
//       IADD R1, 1               ; ... operate ...
//       MOV  R4, R1              ; ... copy back; R1 is dead
//
// Two passes, both proven with the control-flow-aware liveness query that
// peephole-zero-test already uses (liveness.c: reg_dead_from()):
//
//   dead-regs   An instruction that only writes a register nobody reads
//               afterwards is removed. Pure register writers only: MOV, LEA,
//               ALU, shifts, compares, conversions. Never IN/POP (they move
//               hardware state), the divisions, MOD, SIN/ACOS/ATAN2/LOG/POW
//               (they raise errors the program may depend on), MOVS/SETS/
//               CMPS, CALL. R0-R10 only (liveness.c answers "live" for the
//               rest).
//
//   copies      Copy propagation: after "MOV Rt, Rs", reads of Rt up to the
//               next write of Rt or Rs read Rs instead, so the copy can die.
//               Copy-out coalescing: "MOV Rt, Rs / ... / MOV Rs, Rt" around
//               a straight-line window that never mentions Rs, with Rt dead
//               afterwards, is the window renamed onto Rs with both copies
//               gone ("MOV R1,R4 / IADD R1,1 / MOV R4,R1" -> "IADD R4,1").
//
// Both leave the program's behavior alone and only ever shorten it, so they
// iterate with everything else to a fixed point. Whatever liveness cannot
// prove (computed jumps, ambiguous labels, a callee it cannot see into, the
// step budget) counts as "live" and the site is kept.
// ===================================================================

static bool pure_register_writer(const AsmNode *n) {
    switch (n->type) {
        case OP_MOV: case OP_LEA:
        case OP_CIF: case OP_CFI: case OP_CIB: case OP_CFB:
        case OP_NOT: case OP_AND: case OP_OR: case OP_XOR: case OP_BNOT: case OP_SHL:
        case OP_IADD: case OP_ISUB: case OP_IMUL: case OP_ISGN: case OP_IMIN: case OP_IMAX: case OP_IABS:
        case OP_FADD: case OP_FSUB: case OP_FMUL: case OP_FSGN: case OP_FMIN: case OP_FMAX: case OP_FABS:
        case OP_FLR: case OP_CEIL: case OP_ROUND:
        case OP_IEQ: case OP_INE: case OP_IGT: case OP_IGE: case OP_ILT: case OP_ILE:
        case OP_FEQ: case OP_FNE: case OP_FGT: case OP_FGE: case OP_FLT: case OP_FLE:
            break;
        default:
            return false;
    }
    if (!n->has_dst || n->dst_op.mode != MODE_REG) return false;
    int r = get_reg_index(n->dst_op.reg);
    return r >= 0 && r <= 10 && n->dst_op.reg[0] != '\0' &&
           (n->dst_op.reg[0] == 'R' || n->dst_op.reg[0] == 'r');
}

int peephole_dead_regs(AsmNode *head) {
    int optimizations = 0;
    liveness_begin(head);

    AsmNode *curr = head ? head->next : NULL;
    while (curr) {
        AsmNode *n = curr;
        curr = curr->next;
        if (!pure_register_writer(n) || !n->next) continue;
        if (!reg_dead_from(n->next, n->dst_op.reg)) continue;

        AsmNode *nodes[1] = { n };
        AsmNode *dummy = NULL;
        if (remove_with_debug(&dummy, nodes, 1, OPT_PEEPHOLE_DEAD_REGS)) optimizations++;
    }

    liveness_end();
    return optimizations;
}

// -------------------------------------------------------------------
// Operand helpers
// -------------------------------------------------------------------
static bool op_mentions(const Operand *op, const char *reg) {
    return (op->mode == MODE_REG || op->mode == MODE_INDIRECT) && str_case_eq(op->reg, reg);
}

static bool node_mentions(const AsmNode *n, const char *reg) {
    return (n->has_dst && op_mentions(&n->dst_op, reg)) || (n->has_src && op_mentions(&n->src_op, reg));
}

// Rebuild the instruction text after its operands changed.
static void refresh_raw(AsmNode *n) {
    if (n->has_dst && n->has_src) node_set_rawf(n, "    %s %s, %s", n->mnemonic, n->dst_op.raw, n->src_op.raw);
    else if (n->has_dst)          node_set_rawf(n, "    %s %s", n->mnemonic, n->dst_op.raw);
}

static void retarget_operand(Operand *op, const char *to) {
    if (op->mode == MODE_REG) {
        safe_str_copy(op->reg, to, sizeof(op->reg));
        safe_str_copy(op->raw, to, sizeof(op->raw));
    } else if (op->mode == MODE_INDIRECT) {
        // swap the register name inside the text, so whatever follows it
        // (offset, symbol) stays exactly as written
        char old[32], rebuilt[128];
        safe_str_copy(old, op->reg, sizeof(old));
        const char *p = op->raw;
        while (*p && *p != '[') p++;
        if (*p == '[') p++;
        while (*p == ' ') p++;
        size_t n = strlen(old);
        if (strncasecmp(p, old, n) == 0) {
            snprintf(rebuilt, sizeof(rebuilt), "%.*s%s%s", (int)(p - op->raw), op->raw, to, p + n);
            safe_str_copy(op->raw, rebuilt, sizeof(op->raw));
        }
        safe_str_copy(op->reg, to, sizeof(op->reg));
    }
}

static int reg_number(const char *name) {
    int r = get_reg_index(name);
    return (r >= 0 && r <= 10 && (name[0] == 'R' || name[0] == 'r')) ? r : -1;
}

// Is this "MOV Rd, Rs" between two distinct R0-R10 registers?
static bool is_reg_copy(const AsmNode *n) {
    return n->type == OP_MOV && n->has_dst && n->has_src &&
           n->dst_op.mode == MODE_REG && n->src_op.mode == MODE_REG &&
           reg_number(n->dst_op.reg) >= 0 && reg_number(n->src_op.reg) >= 0 &&
           !str_case_eq(n->dst_op.reg, n->src_op.reg);
}

// Straight-line code only: nothing that can transfer control or whose
// register effects aren't spelled out in its operands.
static bool window_breaker(const AsmNode *n) {
    if (n->type == OP_LABEL) return true;
    if (n->type == OP_OTHER) return is_directive_node(n);
    switch (n->type) {
        case OP_JMP: case OP_JT: case OP_JF: case OP_CALL: case OP_RET: case OP_HLT: case OP_WAIT:
        case OP_MOVS: case OP_SETS: case OP_CMPS:
            return true;
        default:
            return false;
    }
}

// -------------------------------------------------------------------
// Copy-out coalescing:   MOV Rt, Rs  ...window...  MOV Rs, Rt
// -------------------------------------------------------------------
static bool try_coalesce(AsmNode *copy_in, AsmNode **resume) {
    const char *rt = copy_in->dst_op.reg;
    const char *rs = copy_in->src_op.reg;

    AsmNode *copy_out = NULL;
    int dist = 0;
    for (AsmNode *n = copy_in->next; n; n = n->next) {
        if (n->type == OP_OTHER && !is_directive_node(n)) continue;       // comments, blanks
        if (window_breaker(n) || ++dist > PEEPHOLE_MAX_SCAN_DISTANCE) return false;
        if (n->type == OP_MOV && n->has_dst && n->has_src &&
            n->dst_op.mode == MODE_REG && n->src_op.mode == MODE_REG &&
            str_case_eq(n->dst_op.reg, rs) && str_case_eq(n->src_op.reg, rt)) {
            copy_out = n;
            break;
        }
        if (node_mentions(n, rs)) return false;        // Rs must stay untouched in the window
    }
    if (!copy_out || !copy_out->next) return false;
    if (!reg_dead_from(copy_out->next, rt)) return false;     // Rt has to be dead afterwards

    // all-or-nothing: one trigger slot for the whole rewrite
    if (!trigger_allowed()) return false;

    for (AsmNode *n = copy_in->next; n != copy_out; n = n->next) {
        if (n->type == OP_OTHER) continue;
        bool changed = false;
        if (n->has_dst && op_mentions(&n->dst_op, rt)) { retarget_operand(&n->dst_op, rs); changed = true; }
        if (n->has_src && op_mentions(&n->src_op, rt)) { retarget_operand(&n->src_op, rs); changed = true; }
        if (changed) {
            insert_debug_comment(n->prev, OPT_PEEPHOLE_COPIES, n->raw);
            refresh_raw(n);
        }
    }
    insert_debug_comment(copy_in->prev, OPT_PEEPHOLE_COPIES, copy_in->raw);
    insert_debug_comment(copy_out->prev, OPT_PEEPHOLE_COPIES, copy_out->raw);
    *resume = copy_out->next;
    remove_node(copy_in);
    remove_node(copy_out);
    return true;
}

// -------------------------------------------------------------------
// Copy propagation:   MOV Rt, Rs  ...  uses of Rt  ->  uses of Rs
// Returns the number of operands rewritten.
// -------------------------------------------------------------------
static int propagate_copy(AsmNode *copy) {
    const char *rt = copy->dst_op.reg;
    const char *rs = copy->src_op.reg;
    int rewritten = 0, dist = 0;

    for (AsmNode *n = copy->next; n; n = n->next) {
        if (n->type == OP_OTHER && !is_directive_node(n)) continue;
        if (n->type == OP_LABEL || n->type == OP_OTHER) break;
        if (++dist > PEEPHOLE_MAX_SCAN_DISTANCE) break;
        bool ends_block = window_breaker(n);
        // JT/JF read their register: rewriting it is fine, but nothing after
        // the branch is straight-line any more. CALL/RET/... end the scan
        // without a rewrite (their register effects are implicit).
        bool is_branch = (n->type == OP_JT || n->type == OP_JF);
        if (ends_block && !is_branch) break;

        // writes of Rt or Rs end the copy's validity -- after reading Rt
        // in this very instruction when it is a pure source
        bool writes = false;
        if (n->has_dst && n->dst_op.mode == MODE_REG &&
            (str_case_eq(n->dst_op.reg, rt) || str_case_eq(n->dst_op.reg, rs)) &&
            !is_branch && n->type != OP_PUSH) writes = true;
        if (n->type == OP_POP && n->has_dst && n->dst_op.mode == MODE_REG &&
            (str_case_eq(n->dst_op.reg, rt) || str_case_eq(n->dst_op.reg, rs))) writes = true;

        // read-modify-write of Rt itself (IADD Rt, ..., compares): the Rt read
        // cannot be renamed without renaming the write: stop here
        if (writes && n->type != OP_MOV && n->dst_op.mode == MODE_REG &&
            str_case_eq(n->dst_op.reg, rt) && n->type != OP_POP) break;

        bool changed = false;
        if (n->has_src && op_mentions(&n->src_op, rt) &&
            trigger_allowed()) {
            insert_debug_comment(n->prev, OPT_PEEPHOLE_COPIES, n->raw);
            retarget_operand(&n->src_op, rs); changed = true; rewritten++;
        }
        if (n->has_dst && n->dst_op.mode == MODE_INDIRECT && str_case_eq(n->dst_op.reg, rt) &&
            trigger_allowed()) {
            if (!changed) insert_debug_comment(n->prev, OPT_PEEPHOLE_COPIES, n->raw);
            retarget_operand(&n->dst_op, rs); changed = true; rewritten++;
        }
        if ((is_branch || n->type == OP_PUSH) && n->has_dst && n->dst_op.mode == MODE_REG &&
            str_case_eq(n->dst_op.reg, rt) && trigger_allowed()) {
            if (!changed) insert_debug_comment(n->prev, OPT_PEEPHOLE_COPIES, n->raw);
            retarget_operand(&n->dst_op, rs); changed = true; rewritten++;
        }
        if (changed) refresh_raw(n);

        if (writes || ends_block) break;
    }
    return rewritten;
}


// -------------------------------------------------------------------
// Immediate propagation:   MOV Rt, imm / OP Rd, Rt   ->  OP Rd, imm
//                          MOV Rt, imm / MOV [Rt], Rs ->  MOV [imm], Rs
//                          MOV Rt, imm / MOV Rd, [Rt] ->  MOV Rd, [imm]
// when every read of Rt before the end of the straight-line window is one
// of those forms (at most two: each costs a word) and Rt is dead after the
// last one, so the MOV goes too: one cycle and at least one word less. Full
// unrolling leaves "MOV R1, 19 / MOV [R1], R0" in every copy.
//
// Float operations (FADD FSUB FMUL FDIV FMIN FMAX FEQ FNE FGT FGE FLT FLE)
// take an immediate too:   MOV R2, 1.0 / FADD R5, R2  ->  FADD R5, 1.0
// but v32as (like the official assembler) reads an INTEGER literal there as
// a number to convert: "FADD R1, 1" adds 1.0 (word 0x3F800000) and
// "FADD R1, 0x3F800000" adds 1065353216.0, while "MOV R2, 1" / "MOV R2,
// 0x3F800000" put the words 0x00000001 / 0x3F800000 in the register. So the
// rule goes by the word the register held, never by the spelling:
//   - a decimal float literal (-?digits.digits, which v32as reads with
//     strtof and a sign flip, in MOV and in a float operation alike) is
//     copied as written into a float operation or a "MOV Rd, Rt"; never
//     into an integer operation or an address;
//   - an integer literal (decimal or 0x hex, 32 bits) goes into a float
//     operation only respelled as a decimal float literal that strtof reads
//     back to exactly that word (0x3F800000 -> 1.0, 0x41200000 -> 10.0);
//     a word that has no such spelling (NaN, infinity, a denormal or a
//     value needing too many digits: "MOV R2, 1 / FADD R1, R2" adds the
//     denormal 0x00000001) is refused;
//   - a symbol never goes into a float operation (v32as takes only a number
//     there).
// Checked with v32as --standard (encoded immediate word of both forms) and
// in v32sim (same result for both forms) for each of the twelve mnemonics.
// -------------------------------------------------------------------
static bool imm_alu_takes_imm(OpType t) {
    switch (t) {
        case OP_MOV: case OP_IADD: case OP_ISUB: case OP_IMUL: case OP_AND: case OP_OR: case OP_XOR:
        case OP_SHL: case OP_IMIN: case OP_IMAX:
        case OP_IEQ: case OP_INE: case OP_IGT: case OP_IGE: case OP_ILT: case OP_ILE:
            return true;
        default:
            return false;
    }
}

static bool imm_float_takes_imm(OpType t) {
    switch (t) {
        case OP_FADD: case OP_FSUB: case OP_FMUL: case OP_FDIV: case OP_FMIN: case OP_FMAX:
        case OP_FEQ: case OP_FNE: case OP_FGT: case OP_FGE: case OP_FLT: case OP_FLE:
            return true;
        default:
            return false;
    }
}

// "-?[0-9]+.[0-9]+": v32as's float token (with an optional unary minus).
static bool float_literal_text(const char *s) {
    if (*s == '-') s++;
    if (!isdigit((unsigned char)*s)) return false;
    while (isdigit((unsigned char)*s)) s++;
    if (*s++ != '.' || !isdigit((unsigned char)*s)) return false;
    while (isdigit((unsigned char)*s)) s++;
    return *s == '\0';
}

// The 32-bit word an integer literal puts in a register: decimal (no leading
// zero: v32as would read octal) or 0x hex with at most 8 digits, an optional
// minus. Anything else (symbols, octal, out of range) answers false.
static bool int_literal_word(const char *s, uint32_t *w) {
    bool neg = (*s == '-');
    if (neg) s++;
    unsigned long long v = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
        int nd = 0;
        for (; *s; s++, nd++) {
            if (!isxdigit((unsigned char)*s) || nd >= 8) return false;
            v = v * 16 + (unsigned)(isdigit((unsigned char)*s) ? *s - '0' : (tolower((unsigned char)*s) - 'a' + 10));
        }
        if (nd == 0) return false;
    } else {
        if (!isdigit((unsigned char)*s) || (s[0] == '0' && s[1] != '\0')) return false;
        for (; *s; s++) {
            if (!isdigit((unsigned char)*s)) return false;
            v = v * 10 + (unsigned)(*s - '0');
            if (v > 0xFFFFFFFFull) return false;
        }
    }
    if (neg) {
        if (v > 0x80000000ull) return false;
        v = (0x100000000ull - v) & 0xFFFFFFFFull;
    }
    *w = (uint32_t)v;
    return true;
}

// A decimal float literal that v32as reads back to exactly `w`, or false.
static bool float_spelling(uint32_t w, char *out, size_t size) {
    float f;
    memcpy(&f, &w, sizeof(f));
    if (isnan(f) || isinf(f)) return false;
    uint32_t mag = w & 0x7FFFFFFFu;
    float m;
    memcpy(&m, &mag, sizeof(m));
    for (int prec = 1; prec <= 12; prec++) {
        char buf[64];
        int len = snprintf(buf, sizeof(buf), "%.*f", prec, (double)m);
        if (len <= 0 || len > 40) return false;
        if (!float_literal_text(buf)) continue;
        float back = strtof(buf, NULL);
        uint32_t bw;
        memcpy(&bw, &back, sizeof(bw));
        if (bw != mag) continue;
        if ((size_t)len + 2 > size) return false;
        snprintf(out, size, "%s%s", (w & 0x80000000u) ? "-" : "", buf);
        return true;
    }
    return false;
}

static bool plain_imm(const Operand *op) {
    if (op->mode != MODE_IMMEDIATE || op->is_float) return false;
    const char *s = op->raw;
    if (!*s) return false;
    if (*s == '-') s++;
    if (isdigit((unsigned char)*s)) {
        for (; *s; s++) if (!isalnum((unsigned char)*s)) return false;      // 19, 0x13
        return true;
    }
    if (!(isalpha((unsigned char)*s) || *s == '_')) return false;
    if (op->raw[0] == '-') return false;
    for (; *s; s++) if (!(isalnum((unsigned char)*s) || *s == '_')) return false;  // global_arr
    return get_reg_index(op->raw) < 0;
}

static int propagate_imm(AsmNode *mov) {
    if (mov->type != OP_MOV || !mov->has_dst || !mov->has_src || node_is_volatile(mov)) return 0;
    if (mov->dst_op.mode != MODE_REG || reg_number(mov->dst_op.reg) < 0) return 0;
    // a float literal: into float operations and register MOVs only, as written
    bool is_flt = mov->src_op.mode == MODE_IMMEDIATE && float_literal_text(mov->src_op.raw);
    if (!is_flt && !plain_imm(&mov->src_op)) return 0;
    // an integer literal: into float operations respelled (if it can be)
    uint32_t word = 0;
    char fspell[64] = "";
    if (!is_flt && int_literal_word(mov->src_op.raw, &word) &&
        !float_spelling(word, fspell, sizeof(fspell))) fspell[0] = '\0';
    const char *rt = mov->dst_op.reg;

    enum { U_SRC, U_LOAD, U_STORE, U_FSRC };
    AsmNode *use[2];
    int kind[2], nuse = 0, dist = 0;
    for (AsmNode *n = mov->next; n; n = n->next) {
        if (n->type == OP_OTHER && !is_directive_node(n)) continue;
        if (window_breaker(n) || ++dist > PEEPHOLE_MAX_SCAN_DISTANCE) break;
        if (!loop_mentions_reg(n, rt)) {
            if (modifies_register(n, rt)) return 0;          // (CALL: covered by window_breaker)
            continue;
        }
        // a pure overwrite ends the window
        if (n->type == OP_MOV && n->dst_op.mode == MODE_REG && str_case_eq(n->dst_op.reg, rt) &&
            !(n->has_src && op_mentions(&n->src_op, rt))) break;
        if (nuse == 2 || node_is_volatile(n)) return 0;
        int k = -1;
        if (n->has_dst && n->has_src && n->dst_op.mode == MODE_REG && !str_case_eq(n->dst_op.reg, rt) &&
            n->src_op.mode == MODE_REG && str_case_eq(n->src_op.reg, rt) && imm_alu_takes_imm(n->type) &&
            (!is_flt || n->type == OP_MOV))
            k = U_SRC;
        else if (n->has_dst && n->has_src && n->dst_op.mode == MODE_REG && !str_case_eq(n->dst_op.reg, rt) &&
                 n->src_op.mode == MODE_REG && str_case_eq(n->src_op.reg, rt) && imm_float_takes_imm(n->type) &&
                 (is_flt || fspell[0]))
            k = U_FSRC;
        else if (is_flt)
            k = -1;                                       // (a float is never an address)
        else if (n->type == OP_MOV && n->has_dst && n->has_src && n->dst_op.mode == MODE_REG &&
                 !str_case_eq(n->dst_op.reg, rt) && n->src_op.mode == MODE_INDIRECT &&
                 str_case_eq(n->src_op.reg, rt) && n->src_op.offset == 0)
            k = U_LOAD;
        else if (n->type == OP_MOV && n->has_dst && n->has_src && n->dst_op.mode == MODE_INDIRECT &&
                 str_case_eq(n->dst_op.reg, rt) && n->dst_op.offset == 0 &&
                 n->src_op.mode == MODE_REG && !str_case_eq(n->src_op.reg, rt))
            k = U_STORE;
        if (k < 0) return 0;
        use[nuse] = n;
        kind[nuse++] = k;
        if (modifies_register(n, rt)) return 0;              // (none of the forms writes Rt)
    }
    if (nuse == 0 || !use[nuse - 1]->next || !reg_dead_from(use[nuse - 1]->next, rt)) return 0;
    if (!trigger_allowed()) return 0;

    char addr[160];
    snprintf(addr, sizeof(addr), "[%s]", mov->src_op.raw);
    for (int i = 0; i < nuse; i++) {
        AsmNode *n = use[i];
        insert_debug_comment(n->prev, OPT_PEEPHOLE_COPIES, n->raw);
        if (kind[i] == U_SRC)       n->src_op = mov->src_op;
        else if (kind[i] == U_FSRC) n->src_op = is_flt ? mov->src_op : parse_operand(fspell);
        else if (kind[i] == U_LOAD) n->src_op = parse_operand(addr);
        else                        n->dst_op = parse_operand(addr);
        refresh_raw(n);
    }
    insert_debug_comment(mov->prev, OPT_PEEPHOLE_COPIES, mov->raw);
    remove_node(mov);
    return nuse + 1;
}

int peephole_copies(AsmNode *head) {
    int optimizations = 0;
    liveness_begin(head);

    AsmNode *curr = head ? head->next : NULL;
    while (curr) {
        AsmNode *n = curr;
        curr = curr->next;
        if (!is_reg_copy(n)) {
            optimizations += propagate_imm(n);     // (n is freed when it fires; curr is past it)
            continue;
        }

        AsmNode *resume = NULL;
        if (try_coalesce(n, &resume)) { optimizations++; curr = resume; continue; }

        int k = propagate_copy(n);
        optimizations += k;
        if (k > 0 && n->next && reg_dead_from(n->next, n->dst_op.reg)) {
            AsmNode *nodes[1] = { n };
            AsmNode *dummy = NULL;
            if (remove_with_debug(&dummy, nodes, 1, OPT_PEEPHOLE_COPIES)) optimizations++;
        }
    }

    liveness_end();
    return optimizations;
}
