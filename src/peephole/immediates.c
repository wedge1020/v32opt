#include "v32opt.h"

// ===================================================================
// PEEPHOLE: Immediate Math Combining (Vircon32-Optimized)
// //
// Combines consecutive arithmetic ops with immediate operands on the same register:
//   - IADD R1, 5; ISUB R1, 3 → IADD R1, 2 (saves 1 word)
//   - FADD R1, 1.5; FSUB R1, 0.5 → FADD R1, 1.0 (floating-point)
//   - IADD R1, 5; ISUB R1, 5 → REMOVE BOTH (cancels to 0)
// //
// Guards:
//   - Different registers: IADD R1, 5; IADD R2, 3 → KEEP
//   - Non-consecutive: IADD R1, 5; MOV R2, 10; IADD R1, 3 → KEEP
//   - Non-immediate: IADD R1, R2; IADD R1, 5 → KEEP
// ===================================================================
int peephole_immediates(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr) {
        // --- INTEGER OPERATIONS ---
        if ((curr->type == OP_IADD || curr->type == OP_ISUB) &&
            curr->dst_op.mode == MODE_REG &&
            is_numeric_immediate(&curr->src_op) && !curr->src_op.is_float) {
            
            AsmNode *next_real = skip_other_nodes(curr->next);
            if (next_real && (next_real->type == OP_IADD || next_real->type == OP_ISUB) &&
                next_real->dst_op.mode == MODE_REG &&
                is_numeric_immediate(&next_real->src_op) && !next_real->src_op.is_float &&
                str_case_eq(curr->dst_op.reg, next_real->dst_op.reg)) {
                
                // Calculate combined value
                int val1 = (curr->type == OP_IADD) ? curr->src_op.immediate : -curr->src_op.immediate;
                int val2 = (next_real->type == OP_IADD) ? next_real->src_op.immediate : -next_real->src_op.immediate;
                int combined = val1 + val2;

                if (combined == 0) {
                    // Complete cancellation: remove both
                    AsmNode *nodes[] = {curr, next_real};
                    remove_with_debug(&curr, nodes, 2, OPT_PEEPHOLE_IMMEDIATES);
                    optimizations += 2;
                    continue;
                } else {
                    // Partial combination: replace first, remove second
                    if (config.debug) {
                        insert_debug_comment(curr->prev, OPT_PEEPHOLE_IMMEDIATES, curr->raw);
                    }
                    curr->type = (combined > 0) ? OP_IADD : OP_ISUB;
                    strcpy(curr->mnemonic, (combined > 0) ? "IADD" : "ISUB");
                    curr->src_op.immediate = abs(combined);
                    snprintf(curr->src_op.raw, sizeof(curr->src_op.raw), "%d", abs(combined));
                    snprintf(curr->raw, sizeof(curr->raw), "    %s %s, %d",
                             curr->mnemonic, curr->dst_op.raw, abs(combined));

                    AsmNode *nodes[] = {next_real};
                    AsmNode *dummy = next_real;
                    remove_with_debug(&dummy, nodes, 1, OPT_PEEPHOLE_IMMEDIATES);
                    optimizations++;
                    continue;
                }
            }
        }

        // --- FLOATING-POINT OPERATIONS (NEW) ---
        if ((curr->type == OP_FADD || curr->type == OP_FSUB) &&
            curr->dst_op.mode == MODE_REG &&
            is_numeric_immediate(&curr->src_op) && curr->src_op.is_float) {
            
            AsmNode *next_real = skip_other_nodes(curr->next);
            if (next_real && (next_real->type == OP_FADD || next_real->type == OP_FSUB) &&
                next_real->dst_op.mode == MODE_REG &&
                is_numeric_immediate(&next_real->src_op) && next_real->src_op.is_float &&
                str_case_eq(curr->dst_op.reg, next_real->dst_op.reg)) {
                
                // Calculate combined value (floating-point)
                float val1 = (curr->type == OP_FADD) ? curr->src_op.float_value : -curr->src_op.float_value;
                float val2 = (next_real->type == OP_FADD) ? next_real->src_op.float_value : -next_real->src_op.float_value;
                float combined = val1 + val2;

                if (combined == 0.0f) {
                    // Complete cancellation: remove both
                    AsmNode *nodes[] = {curr, next_real};
                    remove_with_debug(&curr, nodes, 2, OPT_PEEPHOLE_IMMEDIATES);
                    optimizations += 2;
                    continue;
                } else {
                    // Partial combination: replace first, remove second
                    if (config.debug) {
                        insert_debug_comment(curr->prev, OPT_PEEPHOLE_IMMEDIATES, curr->raw);
                    }
                    curr->type = (combined > 0) ? OP_FADD : OP_FSUB;
                    strcpy(curr->mnemonic, (combined > 0) ? "FADD" : "FSUB");
                    curr->src_op.float_value = fabs(combined);
                    curr->src_op.immediate = 0; // Clear integer value
                    snprintf(curr->src_op.raw, sizeof(curr->src_op.raw), "%.6f", fabs(combined));
                    snprintf(curr->raw, sizeof(curr->raw), "    %s %s, %.6f",
                             curr->mnemonic, curr->dst_op.raw, fabs(combined));

                    AsmNode *nodes[] = {next_real};
                    AsmNode *dummy = next_real;
                    remove_with_debug(&dummy, nodes, 1, OPT_PEEPHOLE_IMMEDIATES);
                    optimizations++;
                    continue;
                }
            }
        }

        curr = curr->next;
    }
    return optimizations;
}
