// ===================================================================
// PEEPHOLE: Strength Reduction (Vircon32-Optimized)
// //
// Replaces expensive operations with cheaper equivalents:
//   - IMUL R, 0 → MOV R, 0 (saves 1 word: IMUL+imm → MOV+imm)
//   - IMUL R, 1 → REMOVE (saves 2 words)
//   - IMUL R, 2 → IADD R, R (saves 1 word: IMUL+imm → IADD)
//   - IDIV R, 1 → REMOVE (saves 2 words)
//   - FMUL R, 0.0 → MOV R, 0.0 (floating-point)
//   - FMUL R, 1.0 → REMOVE (floating-point)
// //
// Guards:
//   - Register operands: IMUL R1, R2 → KEEP (not a constant)
//   - Non-reducible: IMUL R1, 3 → KEEP
// ===================================================================
#include "v32opt.h"

// Helper: Check if operand is a numeric immediate (not a label)
static bool is_numeric_immediate_only(Operand *op) {
    if (!is_numeric_immediate(op)) return false;
    if (op->raw[0] == '_') return false; // Label
    return true;
}

int peephole_reduce(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr) {
        AsmNode *next = curr->next;

        // --- INTEGER MULTIPLICATION ---
        if (curr->type == OP_IMUL && curr->dst_op.mode == MODE_REG &&
            curr->src_op.mode == MODE_IMMEDIATE &&
            !curr->src_op.is_float &&
            is_numeric_immediate_only(&curr->src_op)) {
            
            long imm = curr->src_op.immediate;

            if (imm == 0) {
                insert_debug_comment(curr->prev, OPT_PEEPHOLE_REDUCE, curr->raw);
                curr->type = OP_MOV;
                strcpy(curr->mnemonic, "MOV");
                curr->src_op.immediate = 0;
                snprintf(curr->src_op.raw, sizeof(curr->src_op.raw), "0");
                snprintf(curr->raw, sizeof(curr->raw), "    MOV %s, 0", curr->dst_op.raw);
                optimizations++;
            } else if (imm == 1) {
                AsmNode *nodes[] = {curr};
                remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_REDUCE);
                optimizations++;
                continue;
            } else if (imm == 2) {
                insert_debug_comment(curr->prev, OPT_PEEPHOLE_REDUCE, curr->raw);
                curr->type = OP_IADD;
                strcpy(curr->mnemonic, "IADD");
                curr->src_op = curr->dst_op;
                snprintf(curr->raw, sizeof(curr->raw), "    IADD %s, %s",
                         curr->dst_op.raw, curr->src_op.raw);
                optimizations++;
            }
        }

        // --- INTEGER DIVISION ---
        if (curr->type == OP_IDIV && curr->dst_op.mode == MODE_REG &&
            curr->src_op.mode == MODE_IMMEDIATE &&
            !curr->src_op.is_float &&
            is_numeric_immediate_only(&curr->src_op)) {
            
            long imm = curr->src_op.immediate;
            if (imm == 1) {
                AsmNode *nodes[] = {curr};
                remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_REDUCE);
                optimizations++;
                continue;
            }
        }

        // --- INTEGER MODULUS ---
        if (curr->type == OP_IMOD && curr->dst_op.mode == MODE_REG &&
            curr->src_op.mode == MODE_IMMEDIATE &&
            !curr->src_op.is_float &&
            is_numeric_immediate_only(&curr->src_op)) {
            
            long imm = curr->src_op.immediate;
            if (imm == 1) {
                insert_debug_comment(curr->prev, OPT_PEEPHOLE_REDUCE, curr->raw);
                curr->type = OP_MOV;
                strcpy(curr->mnemonic, "MOV");
                curr->src_op.immediate = 0;
                snprintf(curr->src_op.raw, sizeof(curr->src_op.raw), "0");
                snprintf(curr->raw, sizeof(curr->raw), "    MOV %s, 0", curr->dst_op.raw);
                optimizations++;
            }
        }

        // --- FLOATING-POINT MULTIPLICATION ---
        if (curr->type == OP_FMUL && curr->dst_op.mode == MODE_REG &&
            curr->src_op.mode == MODE_IMMEDIATE &&
            curr->src_op.is_float &&
            is_numeric_immediate_only(&curr->src_op)) {
            
            float imm = curr->src_op.float_value;

            if (imm == 0.0f) {
                insert_debug_comment(curr->prev, OPT_PEEPHOLE_REDUCE, curr->raw);
                curr->type = OP_MOV;
                strcpy(curr->mnemonic, "MOV");
                curr->src_op.float_value = 0.0f;
                snprintf(curr->src_op.raw, sizeof(curr->src_op.raw), "0.0");
                snprintf(curr->raw, sizeof(curr->raw), "    MOV %s, 0.0", curr->dst_op.raw);
                optimizations++;
            } else if (imm == 1.0f) {
                AsmNode *nodes[] = {curr};
                remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_REDUCE);
                optimizations++;
                continue;
            }
        }

        // --- FLOATING-POINT DIVISION ---
        if (curr->type == OP_FDIV && curr->dst_op.mode == MODE_REG &&
            curr->src_op.mode == MODE_IMMEDIATE &&
            curr->src_op.is_float &&
            is_numeric_immediate_only(&curr->src_op)) {
            
            float imm = curr->src_op.float_value;
            if (imm == 1.0f) {
                AsmNode *nodes[] = {curr};
                remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_REDUCE);
                optimizations++;
                continue;
            }
        }

        curr = next;
    }
    return optimizations;
}
