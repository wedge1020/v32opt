#include "v32opt.h"

// ===================================================================
// PEEPHOLE: Forward-Scanning Store & Copy Propagation (Vircon32-Optimized)
// //
// Patterns:
//   1. Store-to-Load Forwarding:
//      - MOV [mem], R1; ... MOV R2, [mem] → MOV R2, R1 (saves 1 word)
//      - MOV [mem], R1; ... FADD R2, [mem] → FADD R2, R1 (floating-point)
// illeg;l:      - MOV [R1], [R2]; ... MOV R3, [R1] → MOV R3, [R2] (memory-to-memory)
//   2. Copy Propagation:
//      - MOV R1, val; ... OP R2, R1 → OP R2, val (ONLY if OP accepts immediates)
//      - MOV R1, 42; ... IADD R2, R1 → IADD R2, 42 (safe: IADD accepts immediates)
//      - MOV R1, 42; ... POW R2, R1 → KEEP (unsafe: POW rejects immediates)
// ===================================================================
int peephole_forwarding(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr) {
        if (curr->type == OP_MOV) {
            // --- RULE 1: Store-to-Load Forwarding (Register -> Indirect) ---
            if (curr->dst_op.mode == MODE_INDIRECT && curr->src_op.mode == MODE_REG) {
                char *mem_reg = curr->dst_op.reg;
                int mem_off = curr->dst_op.offset;
                char *src_reg = curr->src_op.reg;

                AsmNode *scan = curr->next;
                while (scan) {
                    // Skip comments/whitespace
                    if (scan->type == OP_OTHER && (scan->raw[0] == '\0' || scan->raw[0] == ';')) {
                        scan = scan->next;
                        continue;
                    }

                    // Stop if memory base or source register is modified
                    if (modifies_register(scan, mem_reg) || modifies_register(scan, src_reg)) break;

                    // Stop if we hit a store to the same memory location
                    if (scan->type == OP_MOV && scan->dst_op.mode == MODE_INDIRECT &&
                        str_case_eq(scan->dst_op.reg, mem_reg) && scan->dst_op.offset == mem_off) {
                        break;
                    }

                    // --- CASE 1: Register Load from Same Memory (Standard) ---
                    if (scan->type == OP_MOV && scan->dst_op.mode == MODE_REG &&
                        scan->src_op.mode == MODE_INDIRECT &&
                        str_case_eq(scan->src_op.reg, mem_reg) && scan->src_op.offset == mem_off &&
                        trigger_allowed()) {
                        insert_debug_comment(scan->prev, OPT_PEEPHOLE_FORWARDING, scan->raw);
                        scan->src_op = curr->src_op; // Replace [mem] with R1
                        snprintf(scan->raw, sizeof(scan->raw), "    MOV %s, %s",
                                 scan->dst_op.raw, scan->src_op.raw);
                        optimizations++;
                        break;
                    }

                    if (is_control_flow_boundary(scan)) break;
                    scan = scan->next;
                }
            }

            // --- RULE 2: Copy Propagation (Register-to-Register ONLY) ---
            if (curr->dst_op.mode == MODE_REG) {
                char *def_reg = curr->dst_op.reg;
                if (str_case_eq(def_reg, "SP") || str_case_eq(def_reg, "BP")) {
                    curr = curr->next;
                    continue;
                }

                // --- GUARD 0: Only propagate register values (optimize for size) ---
                // On Vircon32, register operands are most compact; never replace reg with imm/mem
                if (curr->src_op.mode != MODE_REG) {
                    curr = curr->next;
                    continue;
                }

                AsmNode *scan = curr->next;
                while (scan) {
                    if (scan->type == OP_OTHER && (scan->raw[0] == '\0' || scan->raw[0] == ';')) {
                        scan = scan->next;
                        continue;
                    }

                    if (modifies_register(scan, def_reg)) break;
                    if (modifies_register(scan, curr->src_op.reg)) break;

                    bool uses_def_reg = false;
                    Operand *target_op = NULL;

                    // Check if this instruction uses def_reg in src or dst
                    if (scan->src_op.mode == MODE_REG && str_case_eq(scan->src_op.reg, def_reg)) {
                        uses_def_reg = true;
                        target_op = &scan->src_op;
                    } else if (str_case_eq(scan->mnemonic, "JMP") &&
                               scan->dst_op.mode == MODE_REG && str_case_eq(scan->dst_op.reg, def_reg)) {
                        uses_def_reg = true;
                        target_op = &scan->dst_op;
                    }

                    if (uses_def_reg && target_op) {
                        // --- GUARD 1: Don't propagate into JT/JF ---
                        if ((str_case_eq(scan->mnemonic, "JT") || str_case_eq(scan->mnemonic, "JF"))) {
                            break;
                        }

                        // --- GUARD 2: Don't propagate into instructions that reject immediates ---
                        // Note: Since we only propagate registers (GUARD 0), this only checks instruction types
                        if (str_case_eq(scan->mnemonic, "POW") ||
                            str_case_eq(scan->mnemonic, "ATAN2") ||
                            str_case_eq(scan->mnemonic, "IN") ||
                            str_case_eq(scan->mnemonic, "OUT")) {
                            break;
                        }

                        // --- GUARD 3: Don't propagate into indirect destinations ---
                        if (scan->dst_op.mode == MODE_INDIRECT &&
                            !str_case_eq(scan->mnemonic, "JMP")) {
                            break;
                        }

                        // --- GUARD 4: Don't propagate into JMP ---
                        if (str_case_eq(scan->mnemonic, "JMP")) {
                            break;
                        }

                        // TRIGGER CAP: treat an exhausted budget the same as
                        // any other guard above -- stop propagating here.
                        if (!trigger_allowed()) {
                            break;
                        }

                        // --- APPLY: Safe register-to-register propagation ---
                        insert_debug_comment(scan->prev, OPT_PEEPHOLE_FORWARDING, scan->raw);
                        *target_op = curr->src_op;

                        // Rebuild the instruction raw string
                        if (str_case_eq(scan->mnemonic, "JMP")) {
                            snprintf(scan->raw, sizeof(scan->raw), "    JMP %s", target_op->raw);
                        } else if (scan->dst_op.mode != MODE_NONE && scan->src_op.mode != MODE_NONE) {
                            snprintf(scan->raw, sizeof(scan->raw), "    %s %s, %s",
                                     scan->mnemonic, scan->dst_op.raw, scan->src_op.raw);
                        } else if (scan->dst_op.mode != MODE_NONE) {
                            snprintf(scan->raw, sizeof(scan->raw), "    %s %s",
                                     scan->mnemonic, scan->dst_op.raw);
                        } else {
                            snprintf(scan->raw, sizeof(scan->raw), "    %s %s",
                                     scan->mnemonic, scan->src_op.raw);
                        }
                        optimizations++;
                    }

                    if (is_control_flow_boundary(scan)) break;
                    scan = scan->next;
                }
            }
        }
        curr = curr->next;
    }
    return optimizations;
}
