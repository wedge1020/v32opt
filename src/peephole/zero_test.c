#include "v32opt.h"

// ===================================================================
// PEEPHOLE: ZERO TEST  (-fpeephole-zero-test)
//
// Vircon32's conditional jumps test a register against zero directly
// (JT: jump if != 0, JF: jump if == 0), but compilers routinely
// materialise the comparison first:
//
//   Form A (test a copy, keep the original):
//       MOV   Ra, Rb
//       IEQ   Ra, 0            ->      JF    Rb, L
//       JT    Ra, L
//
//   Form B (test in place):
//       IEQ   Ra, 0            ->      JF    Ra, L
//       JT    Ra, L
//
//       compare  jump   becomes
//       IEQ      JT     JF      (jump when the value IS zero)
//       IEQ      JF     JT
//       INE      JT     JT      (jump when the value is NOT zero)
//       INE      JF     JF
//
// v32lua emits Form A in every inline table lookup ("hash block == 0?
// take the slow path"), about 800 times per program, on the hot path.
//
// SOUNDNESS. The original leaves the comparison result (0 or 1) in Ra;
// the rewrite leaves Ra holding whatever it held before (form A) or the
// untested value (form B). That is only invisible if Ra is never read
// again -- on the fall-through side AND at the branch target. Both are
// proven with reg_dead_from() (liveness.c), which follows jumps and
// looks into callees; if it cannot prove it, the site is left alone.
// Form A also needs Ra != Rb.
//
// Integer compares only: FEQ treats -0.0 as equal to 0.0 while JT/JF
// test the raw bits, so the float forms are not equivalent. "0" may be
// a literal or an integer %define that resolves to 0.
// ===================================================================

static bool is_zero_imm(const Operand *op) {
    return is_numeric_immediate(op) && !op->is_float && op->immediate == 0;
}

int peephole_zero_test(AsmNode *head) {
    int optimizations = 0;
    liveness_begin(head);

    AsmNode *curr = head ? head->next : NULL;
    while (curr) {
        AsmNode *cmp = curr;
        if ((cmp->type != OP_IEQ && cmp->type != OP_INE) ||
            !cmp->has_dst || cmp->dst_op.mode != MODE_REG ||
            !cmp->has_src || !is_zero_imm(&cmp->src_op)) {
            curr = curr->next;
            continue;
        }

        const char *ra = cmp->dst_op.reg;
        AsmNode *jmp = skip_other_nodes(cmp->next);
        if (!jmp || (jmp->type != OP_JT && jmp->type != OP_JF) ||
            jmp->dst_op.mode != MODE_REG || !str_case_eq(jmp->dst_op.reg, ra) ||
            !jmp->has_src || is_register_operand(jmp->src_op.raw)) {
            curr = curr->next;
            continue;
        }

        // Ra must be dead once the jump has executed, on both sides.
        if (!reg_dead_from(jmp->next, ra) ||
            !reg_dead_at_label(jmp->src_op.raw, ra)) {
            curr = curr->next;
            continue;
        }

        // Form A: an immediately preceding "MOV Ra, Rb" (Rb != Ra).
        // Walk back over comments/blank lines only.
        AsmNode *mov = cmp->prev;
        while (mov && mov->type == OP_OTHER && !is_directive_node(mov)) mov = mov->prev;
        bool form_a = mov && mov->type == OP_MOV &&
                      mov->dst_op.mode == MODE_REG && str_case_eq(mov->dst_op.reg, ra) &&
                      mov->src_op.mode == MODE_REG && !str_case_eq(mov->src_op.reg, ra) &&
                      get_reg_index(mov->src_op.reg) >= 0;

        // Snapshot everything needed BEFORE removing nodes.
        Operand tested = form_a ? mov->src_op : cmp->dst_op;
        bool jump_if_zero = (cmp->type == OP_IEQ) == (jmp->type == OP_JT);
        char old_jmp_raw[sizeof(jmp->raw)];
        safe_str_copy(old_jmp_raw, jmp->raw, sizeof(old_jmp_raw));

        // One atomic transform: remove first (the shared trigger gate),
        // rewrite the jump only if the removal committed.
        AsmNode *nodes[2];
        int count = 0;
        if (form_a) nodes[count++] = mov;
        nodes[count++] = cmp;
        AsmNode *dummy = NULL;
        if (!remove_with_debug(&dummy, nodes, count, OPT_PEEPHOLE_ZERO_TEST)) {
            curr = jmp->next;       // budget exhausted: leave the site intact
            continue;
        }

        insert_debug_comment(jmp->prev, OPT_PEEPHOLE_ZERO_TEST, old_jmp_raw);
        jmp->type = jump_if_zero ? OP_JF : OP_JT;
        strcpy(jmp->mnemonic, jump_if_zero ? "JF" : "JT");
        jmp->dst_op = tested;
        snprintf(jmp->raw, sizeof(jmp->raw), "    %-5s %s, %s",
                 jmp->mnemonic, jmp->dst_op.raw, jmp->src_op.raw);

        optimizations += count;
        curr = jmp->next;
    }

    liveness_end();
    return optimizations;
}
