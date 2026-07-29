#include "v32opt.h"

// ===================================================================
// PEEPHOLE: Forward-Scanning Store & Copy Propagation
//
// Scans forward within basic blocks to eliminate redundant operations
// by propagating values through registers and memory.
//
// Patterns handled:
//   1. Store-to-Load Forwarding: MOV [mem], R1; ... MOV R2, [mem] → MOV R2, R1
//   2. Copy Propagation:     MOV R1, val;   ... OP R2, R1     → OP R2, val
//
// Example:
//   Input:  MOV [R1+4], R2
//           MOV R3, [R1+4]
//   Output: MOV [R1+4], R2
//           MOV R3, R2
//
//   Input:  MOV R1, 42
//           IADD R2, R1
//   Output: MOV R1, 42
//           IADD R2, 42
//
// Returns: Number of optimizations applied
// ===================================================================
int peephole_forwarding(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        if (curr->type == OP_MOV)
        {
            // --- RULE 1: Store-to-Load Forwarding ---
            if (curr->dst_op.mode == MODE_INDIRECT && curr->src_op.mode == MODE_REG)
            {
                char *mem_reg = curr->dst_op.reg;
                int mem_off = curr->dst_op.offset;
                char *src_reg = curr->src_op.reg;

                AsmNode *scan = curr->next;
                while (scan)
                {
                    if (scan->type == OP_OTHER && (scan->raw[0] == '\0' || scan->raw[0] == ';')) {
                        scan = scan->next;
                        continue;
                    }

                    if (modifies_register(scan, mem_reg) || modifies_register(scan, src_reg)) break;

                    if (scan->type == OP_MOV && scan->dst_op.mode == MODE_INDIRECT &&
                        str_case_eq(scan->dst_op.reg, mem_reg)) {
                        break;
                    }

                    if (scan->type == OP_MOV && scan->dst_op.mode == MODE_REG &&
                        scan->src_op.mode == MODE_INDIRECT &&
                        str_case_eq(scan->src_op.reg, mem_reg) && scan->src_op.offset == mem_off)
                    {
                        insert_debug_comment(scan->prev, OPT_PEEPHOLE_FORWARDING, scan->raw);
                        scan->src_op = curr->src_op;
                        snprintf(scan->raw, sizeof(scan->raw), "    MOV %s, %s",
                                 scan->dst_op.raw, scan->src_op.raw);
                        optimizations++;
                        break;
                    }

                    if (is_control_flow_boundary(scan)) break;

                    scan = scan->next;
                }
            }

            // --- RULE 2: Copy Propagation ---
            if (curr->dst_op.mode == MODE_REG)
            {
                char *def_reg = curr->dst_op.reg;

                if (str_case_eq(def_reg, "SP") || str_case_eq(def_reg, "BP")) {
                    curr = curr->next;
                    continue;
                }

                AsmNode *scan = curr->next;
                while (scan)
                {
                    if (scan->type == OP_OTHER && (scan->raw[0] == '\0' || scan->raw[0] == ';')) {
                        scan = scan->next;
                        continue;
                    }

                    if (modifies_register(scan, def_reg)) break;
                    if (curr->src_op.mode == MODE_REG && modifies_register(scan, curr->src_op.reg)) {
                        break;
                    }

                    bool uses_def_reg = false;
                    Operand *target_op = NULL;

                    if (scan->has_src && scan->src_op.mode == MODE_REG &&
                        str_case_eq(scan->src_op.reg, def_reg)) {
                        uses_def_reg = true;
                        target_op = &scan->src_op;
                    }
                    else if (str_case_eq(scan->mnemonic, "JMP") && scan->has_dst &&
                             scan->dst_op.mode == MODE_REG &&
                             str_case_eq(scan->dst_op.reg, def_reg)) {
                        uses_def_reg = true;
                        target_op = &scan->dst_op;
                    }

                    if (uses_def_reg)
                    {
                        // Cleaned up immediate check to use your robust helper function
                        if ((str_case_eq(scan->mnemonic, "JT") || str_case_eq(scan->mnemonic, "JF")) &&
                            is_numeric_immediate(&curr->src_op)) {
                            break;
                        }

                        if (str_case_eq(scan->mnemonic, "POW") || str_case_eq(scan->mnemonic, "ATAN2")) {
                            break;
                        }

                        bool is_illegal_imm_store = (curr->src_op.mode == MODE_IMMEDIATE &&
                                                     scan->has_dst && scan->dst_op.mode != MODE_REG &&
                                                     !str_case_eq(scan->mnemonic, "JMP"));
                        if (is_illegal_imm_store) break;

                        if (str_case_eq(scan->mnemonic, "JMP") && curr->src_op.mode == MODE_IMMEDIATE) {
                            break;
                        }

                        if (curr->src_op.mode == MODE_IMMEDIATE && scan->dst_op.mode != MODE_REG) {
                            break;
                        }

                        insert_debug_comment(scan->prev, OPT_PEEPHOLE_FORWARDING, scan->raw);
                        *target_op = curr->src_op;

                        if (str_case_eq(scan->mnemonic, "JMP")) {
                            snprintf(scan->raw, sizeof(scan->raw), "    JMP %s", target_op->raw);
                        } else if (scan->has_dst && scan->has_src) {
                            snprintf(scan->raw, sizeof(scan->raw), "    %s %s, %s",
                                     scan->mnemonic, scan->dst_op.raw, scan->src_op.raw);
                        } else if (scan->has_dst) {
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
