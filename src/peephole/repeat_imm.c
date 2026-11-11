#include "v32opt.h"

// ===================================================================
// PEEPHOLE: REPEATED IMMEDIATES  (-fpeephole-repeat-imm)
//
// A "MOV Rd, <imm>" whose register provably already holds that immediate
// is deleted. Full unrolling leaves one per copy of the loop body, with only
// the copies' (now unreferenced) labels in between:
//
//       MOV  R0, 0                       MOV  R0, 0
//       MOV  [19], R0                    MOV  [19], R0
//     __for_3_continue_u1:             __for_3_continue_u1:
//       MOV  R0, 0              ->       MOV  [20], R0
//       MOV  [20], R0
//
// A forward walk over the whole program keeps, per register R0-R13, the
// operand text of the immediate it was last set to by a MOV. Facts survive
// only straight-line code and labels that nothing references (the label
// table counts every mention, data and %define directives included, so such
// a label can only be entered by falling into it). They die at:
//   - any write of the register (modifies_register(): ALU, IN, POP, LEA ...)
//   - CALL (every register R0-R13), MOVS/SETS/CMPS (implicit R11-R13: all)
//   - a referenced, duplicate or unknown label, JMP/RET/HLT (whatever
//     follows is reached some other way), any directive, a volatile access,
//     and any instruction whose destination is not a register this pass
//     knows.
// Immediates compare by their exact operand text: integers, symbols and
// floats alike ("0" and "0x0" are different texts and stay). JT/JF do not
// write registers, so facts continue on their fall-through side; the taken
// side starts at a label, which is referenced.
//
// TRIGGER CAP: one deleted MOV = one slot (remove_with_debug); a declined
// deletion just leaves the MOV, whose fact is the same either way.
// ===================================================================

#define RI_REGS 14      // R0..R13

static bool imm_text(const AsmNode *n, char *buf, size_t sz) {
    if (n->type != OP_MOV || !n->has_dst || !n->has_src) return false;
    if (n->dst_op.mode != MODE_REG || n->src_op.mode != MODE_IMMEDIATE) return false;
    int r = get_reg_index(n->dst_op.reg);
    if (r < 0 || r >= RI_REGS) return false;
    safe_str_copy(buf, n->src_op.raw, sz);
    trim(buf);
    return buf[0] != '\0';
}

int peephole_repeat_imm(AsmNode *head) {
    int optimizations = 0;
    if (!head) return 0;
    loop_labels_build_all(head);

    char fact[RI_REGS][128];
    bool known[RI_REGS];
    memset(known, 0, sizeof(known));
#define FORGET_ALL() memset(known, 0, sizeof(known))

    AsmNode *curr = head->next;
    while (curr) {
        AsmNode *n = curr;
        curr = curr->next;

        if (n->type == OP_OTHER) {
            if (is_directive_node(n)) FORGET_ALL();      // data, %define, %include ...
            continue;                                    // comments, blank lines
        }
        if (n->type == OP_LABEL) {
            if (!loop_label_unreferenced(n)) FORGET_ALL();
            continue;
        }
        if (node_is_volatile(n)) { FORGET_ALL(); continue; }

        char imm[128];
        if (imm_text(n, imm, sizeof(imm))) {
            int r = get_reg_index(n->dst_op.reg);
            if (known[r] && strcmp(fact[r], imm) == 0) {
                AsmNode *nodes[1] = { n };
                AsmNode *dummy = NULL;
                if (remove_with_debug(&dummy, nodes, 1, OPT_PEEPHOLE_REPEAT_IMM)) optimizations++;
                continue;                                // either way Rr still holds imm
            }
            safe_str_copy(fact[r], imm, sizeof(fact[r]));
            known[r] = true;
            continue;
        }

        switch (n->type) {
            case OP_CALL: case OP_JMP: case OP_RET: case OP_HLT:
            case OP_MOVS: case OP_SETS: case OP_CMPS:
                FORGET_ALL();
                continue;
            default:
                break;
        }
        // A destination this pass cannot name (a register alias such as CR/SR/DR
        // parses as a symbol): give up on everything. OUT's port, PUSH's and
        // JT/JF's operands are read, not written.
        if (n->has_dst && n->type != OP_PUSH && n->type != OP_OUT &&
            n->type != OP_JT && n->type != OP_JF) {
            if (n->dst_op.mode == MODE_IMMEDIATE ||
                (n->dst_op.mode == MODE_REG && get_reg_index(n->dst_op.reg) < 0)) { FORGET_ALL(); continue; }
        }
        for (int r = 0; r < RI_REGS; r++) {
            if (!known[r]) continue;
            char nm[8];
            snprintf(nm, sizeof(nm), "R%d", r);
            if (modifies_register(n, nm)) known[r] = false;
        }
        // Aliases (CR/SR/DR for R11-R13) written by name: modifies_register()
        // compares names, so also drop the register get_reg_index() maps it to.
        if (n->has_dst && n->dst_op.mode == MODE_REG && n->type != OP_PUSH &&
            n->type != OP_JT && n->type != OP_JF) {
            int r = get_reg_index(n->dst_op.reg);
            if (r >= 0 && r < RI_REGS) known[r] = false;
        }
    }
#undef FORGET_ALL

    loop_labels_free();
    return optimizations;
}
