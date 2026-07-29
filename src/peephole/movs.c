#include "v32opt.h"

// Helper: Check if an operand uses a specific register (direct or indirect base)
static bool operand_uses_register(Operand *op, const char *reg) {
    if (op->mode == MODE_REG && str_case_eq(op->reg, reg)) return true;
    if (op->mode == MODE_INDIRECT && str_case_eq(op->reg, reg)) return true;
    return false;
}

// Helper: Check if any register in an operand was modified between two nodes
static bool operand_registers_modified(AsmNode *start, AsmNode *end, Operand *op) {
    if (op->mode == MODE_IMMEDIATE) return false;
    if (op->mode == MODE_REG || op->mode == MODE_INDIRECT) {
        AsmNode *check = start;
        while (check != end && check != NULL) {
            if (modifies_register(check, op->reg)) return true;
            check = check->next;
        }
    }
    return false;
}

// Helper: Check for memory stores between two nodes
static bool has_memory_store(AsmNode *start, AsmNode *end) {
    AsmNode *check = start;
    while (check != end && check != NULL) {
        if (check->type == OP_MOV && check->dst_op.mode == MODE_INDIRECT)
            return true;
        check = check->next;
    }
    return false;
}

// Helper: Strict control flow boundary check (includes JT/JF/CALL/RET/Labels)
static bool is_cf_boundary(AsmNode *node) {
    if (!node) return true;
    if (node->type == OP_JT || node->type == OP_JF || node->type == OP_CALL || node->type == OP_RET)
        return true;
    if (node->type == OP_OTHER && strchr(node->raw, ':') != NULL) // Label
        return true;
    return is_control_flow_boundary(node);
}

int peephole_movs(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr) {
        if (curr->type == OP_MOV) {
            // Skip SP and BP entirely (special registers)
            if ((curr->dst_op.mode == MODE_REG && (str_case_eq(curr->dst_op.reg, "SP") || str_case_eq(curr->dst_op.reg, "BP"))) ||
                (curr->src_op.mode == MODE_REG && (str_case_eq(curr->src_op.reg, "SP") || str_case_eq(curr->src_op.reg, "BP")))) {
                curr = curr->next;
                continue;
            }

            AsmNode *scan = curr->next;
            while (scan && !is_cf_boundary(scan)) {  // Use strict boundary check
                AsmNode *next_scan = scan->next;

                if (scan->type != OP_MOV) {
                    scan = next_scan;
                    continue;
                }

                // Skip SP/BP in the scanned instruction
                if ((scan->dst_op.mode == MODE_REG && (str_case_eq(scan->dst_op.reg, "SP") || str_case_eq(scan->dst_op.reg, "BP"))) ||
                    (scan->src_op.mode == MODE_REG && (str_case_eq(scan->src_op.reg, "SP") || str_case_eq(scan->src_op.reg, "BP")))) {
                    scan = next_scan;
                    continue;
                }

                bool removed = false;

                // --- Duplicate Move Elimination ---
                if (curr->dst_op.mode == MODE_REG &&
                    scan->dst_op.mode == MODE_REG &&
                    operands_equal(&curr->dst_op, &scan->dst_op) &&
                    operands_equal(&curr->src_op, &scan->src_op)) {

                    // GUARD: Self-referential indirect (MOV R1, [R1]; MOV R1, [R1])
                    if (curr->src_op.mode == MODE_INDIRECT &&
                        operand_uses_register(&curr->src_op, curr->dst_op.reg)) {
                        scan = next_scan;
                        continue;
                    }

                    // GUARD: Memory aliasing (MOV R1, [R2]; MOV [R5], R6; MOV R1, [R2])
                    if (curr->src_op.mode == MODE_INDIRECT &&
                        has_memory_store(curr->next, scan)) {
                        scan = next_scan;
                        continue;
                    }

                    // GUARD: Destination or source registers modified between moves
                    bool dst_modified = operand_registers_modified(curr->next, scan, &curr->dst_op);
                    bool src_modified = operand_registers_modified(curr->next, scan, &curr->src_op);

                    if (!dst_modified && !src_modified) {
                        AsmNode *nodes[] = {scan};
                        remove_with_debug(&scan->prev->next, nodes, 1, OPT_PEEPHOLE_MOVS);
                        optimizations++;
                        removed = true;
                    }
                }
                // --- Mirror Move Elimination (register-to-register only) ---
                else if (curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_REG &&
                         scan->dst_op.mode == MODE_REG && scan->src_op.mode == MODE_REG) {
                    if (str_case_eq(curr->dst_op.reg, scan->src_op.reg) &&
                        str_case_eq(curr->src_op.reg, scan->dst_op.reg)) {

                        // Check if either register was modified between curr and scan
                        bool reg1_modified = false;
                        bool reg2_modified = false;
                        AsmNode *check = curr->next;
                        while (check != scan && check != NULL) {
                            if (modifies_register(check, curr->dst_op.reg)) reg1_modified = true;
                            if (modifies_register(check, curr->src_op.reg)) reg2_modified = true;
                            if (reg1_modified && reg2_modified) break;
                            check = check->next;
                        }

                        if (!reg1_modified && !reg2_modified) {
                            AsmNode *nodes[] = {scan};
                            remove_with_debug(&scan->prev->next, nodes, 1, OPT_PEEPHOLE_MOVS);
                            optimizations++;
                            removed = true;
                        }
                    }
                }

                scan = next_scan;
            }
        }
        curr = curr->next;
    }

    return optimizations;
}
