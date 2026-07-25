#include "v32opt.h"

// -------------------------------------------------------------------
// Helper: Rewrite an operand to a general-purpose register
// -------------------------------------------------------------------
void promote_operand_to_reg(Operand *op, const char *reg_name) {
    op->mode = MODE_REG;
    op->offset = 0;
    op->immediate = 0;
    op->is_float = false;
    safe_str_copy(op->reg, reg_name, sizeof(op->reg));
    safe_str_copy(op->raw, reg_name, sizeof(op->raw));
}

// -------------------------------------------------------------------
// Helper: Check if a label is defined inside a loop
// -------------------------------------------------------------------
bool is_inside_loop(const char *target, AsmNode *loop_start, AsmNode *loop_end) {
    for (AsmNode *n = loop_start->next; n && n != loop_end; n = n->next) {
        if (n->type == OP_LABEL) {
            char lbl[128] = {0};
            safe_str_copy(lbl, n->raw, sizeof(lbl));
            char *c = strchr(lbl, ':');
            if (c) *c = '\0';
            if (str_case_eq(trim(lbl), target)) return true;
        }
    }
    return false;
}

// -------------------------------------------------------------------
// Pass: Stack Slot to Register Promotion (Scalar Replacement)
// -------------------------------------------------------------------
int pass_promote_stack_slots(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr) {
        // 1. Identify Function Boundaries
        if (curr->type == OP_LABEL) {
            char line_copy[8192];
            safe_str_copy(line_copy, curr->raw, sizeof(line_copy));
            char *lbl = trim(line_copy);

            size_t lbl_len = strlen(lbl);
            bool is_return_label = (lbl_len >= 8 && str_case_eq(lbl + lbl_len - 8, "_return:"));
            bool is_global_scope_init = str_case_eq(lbl, "__global_scope_initialization:");
            if ((strncmp(lbl, "__function_", 11) != 0 && !is_global_scope_init) || is_return_label) {
                curr = curr->next;
                continue;
            }

            // Find the end of this function body
            AsmNode *scan = curr->next;
            AsmNode *end_of_func = curr;
            while (scan) {
                if (scan->type == OP_LABEL) {
                    char next_copy[8192];
                    safe_str_copy(next_copy, scan->raw, sizeof(next_copy));
                    char *next_lbl = trim(next_copy);
                    size_t next_lbl_len = strlen(next_lbl);
                    bool next_is_return = (next_lbl_len >= 8 && str_case_eq(next_lbl + next_lbl_len - 8, "_return:"));
                    if ((strncmp(next_lbl, "__function_", 11) == 0 && !next_is_return) ||
                        strncmp(next_lbl, "__literal_", 10) == 0 ||
                        (strncmp(next_lbl, "__global_", 9) == 0 && !str_case_eq(next_lbl, "__global_scope_initialization:"))) {
                        break;
                    }
                }
                end_of_func = scan;
                scan = scan->next;
            }

            // ----------------------------------------------------------------
            // Phase A: Analyze Function Safety & Register Liveness
            // ----------------------------------------------------------------
            bool is_leaf_function = true;
            bool address_taken = false;
            bool reg_used[16] = {0};
            int slot_counts[128] = {0};

            for (AsmNode *n = curr->next; n && n != end_of_func->next; n = n->next) {
                if (str_case_eq(n->mnemonic, "CALL")) {
                    is_leaf_function = false;
                }

                // Guardrail: Check if BP is used in arithmetic or indirect mode
                if (n->type != OP_PUSH && n->type != OP_POP && !str_case_eq(n->mnemonic, "MOV")) {
                    if ((n->dst_op.mode == MODE_REG && str_case_eq(n->dst_op.reg, "BP")) ||
                        (n->src_op.mode == MODE_REG && str_case_eq(n->src_op.reg, "BP")) ||
                        (n->dst_op.mode == MODE_INDIRECT && str_case_eq(n->dst_op.reg, "BP")) ||
                        (n->src_op.mode == MODE_INDIRECT && str_case_eq(n->src_op.reg, "BP"))) {
                        address_taken = true;
                        break;
                    }
                }

                // Track ALL register usage (direct + indirect)
                if (n->dst_op.mode == MODE_REG) {
                    int idx = get_reg_index(n->dst_op.reg);
                    if (idx >= 0 && idx < 16) reg_used[idx] = true;
                }
                if (n->src_op.mode == MODE_REG) {
                    int idx = get_reg_index(n->src_op.reg);
                    if (idx >= 0 && idx < 16) reg_used[idx] = true;
                }
                if (n->dst_op.mode == MODE_INDIRECT && n->dst_op.reg[0] != '\0') {
                    int idx = get_reg_index(n->dst_op.reg);
                    if (idx >= 0 && idx < 16) reg_used[idx] = true;
                }
                if (n->src_op.mode == MODE_INDIRECT && n->src_op.reg[0] != '\0') {
                    int idx = get_reg_index(n->src_op.reg);
                    if (idx >= 0 && idx < 16) reg_used[idx] = true;
                }

                // Count references to local stack variables: [BP-offset] where offset < 0
                if (n->dst_op.mode == MODE_INDIRECT && str_case_eq(n->dst_op.reg, "BP") && n->dst_op.offset < 0) {
                    int abs_off = -n->dst_op.offset;
                    if (abs_off < 128) slot_counts[abs_off]++;
                }
                if (n->src_op.mode == MODE_INDIRECT && str_case_eq(n->src_op.reg, "BP") && n->src_op.offset < 0) {
                    int abs_off = -n->src_op.offset;
                    if (abs_off < 128) slot_counts[abs_off]++;
                }
            }

            if (address_taken) {
                curr = end_of_func->next;
                continue;
            }

            // ----------------------------------------------------------------
            // Phase B: Execute Function-Wide Promotion (Leaf Functions Only)
            // ----------------------------------------------------------------
            if (is_leaf_function) {
                int free_reg_for_off[128] = {0}; // Track which register was assigned to each offset

                for (int off = 1; off < 128; off++) {
                    if (slot_counts[off] >= 3) {
                        int free_reg = -1;
                        for (int r = 1; r <= 13; r++) {
                            if (!reg_used[r]) {
                                free_reg = r;
                                free_reg_for_off[off] = free_reg;
                                reg_used[r] = true;
                                break;
                            }
                        }

                        if (free_reg == -1) break;

                        char reg_name[16];
                        snprintf(reg_name, sizeof(reg_name), "R%d", free_reg);

                        // Find insertion point right after prologue (after `MOV BP, SP`)
                        AsmNode *prologue_end = curr->next;
                        while (prologue_end && prologue_end != end_of_func) {
                            if (str_case_eq(prologue_end->mnemonic, "MOV") &&
                                str_case_eq(prologue_end->dst_op.reg, "BP") &&
                                str_case_eq(prologue_end->src_op.reg, "SP")) {
                                break;
                            }
                            prologue_end = prologue_end->next;
                        }

                        // Insert Pre-header Load: MOV R_free, [BP-offset]
                        if (prologue_end) {
                            char src_str[32];
                            snprintf(src_str, sizeof(src_str), "[BP-%d]", off);
                            char load_raw[64];
                            snprintf(load_raw, sizeof(load_raw), "    MOV %s, %s", reg_name, src_str);

                            AsmNode *load_node = create_node(load_raw, OP_MOV, "MOV", reg_name, src_str);
                            load_node->has_dst = true;
                            load_node->has_src = true;
                            load_node->dst_op = parse_operand(reg_name);
                            load_node->src_op = parse_operand(src_str);

                            load_node->next = prologue_end->next;
                            load_node->prev = prologue_end;
                            if (prologue_end->next) prologue_end->next->prev = load_node;
                            prologue_end->next = load_node;
                        }

                        // Rewrite all [BP-off] references in the function body to R_free
                        for (AsmNode *n = curr->next; n && n != end_of_func->next; n = n->next) {
                            bool modified = false;
                            if (n->dst_op.mode == MODE_INDIRECT && str_case_eq(n->dst_op.reg, "BP") && n->dst_op.offset == -off) {
                                promote_operand_to_reg(&n->dst_op, reg_name);
                                modified = true;
                            }
                            if (n->src_op.mode == MODE_INDIRECT && str_case_eq(n->src_op.reg, "BP") && n->src_op.offset == -off) {
                                promote_operand_to_reg(&n->src_op, reg_name);
                                modified = true;
                            }
                            if (modified) {
                                if (n->has_dst && n->has_src) {
                                    snprintf(n->raw, sizeof(n->raw), "    %s %s, %s", n->mnemonic, n->dst_op.raw, n->src_op.raw);
                                } else if (n->has_dst) {
                                    snprintf(n->raw, sizeof(n->raw), "    %s %s", n->mnemonic, n->dst_op.raw);
                                }
                                optimizations++;
                            }
                        }
                    }
                }

                // Insert stores before RET to save promoted values back to stack
                for (int off = 1; off < 128; off++) {
                    if (slot_counts[off] >= 3 && free_reg_for_off[off] != 0) {
                        char dst_str[32];
                        snprintf(dst_str, sizeof(dst_str), "[BP-%d]", off);
                        char reg_name[16];
                        snprintf(reg_name, sizeof(reg_name), "R%d", free_reg_for_off[off]);
                        char store_raw[64];
                        snprintf(store_raw, sizeof(store_raw), "    MOV %s, %s", dst_str, reg_name);

                        for (AsmNode *n = curr->next; n && n != end_of_func->next; n = n->next) {
                            if (str_case_eq(n->mnemonic, "RET")) {
                                AsmNode *store_node = create_node(store_raw, OP_MOV, "MOV", dst_str, reg_name);
                                store_node->prev = n->prev;
                                store_node->next = n;
                                if (n->prev) n->prev->next = store_node;
                                n->prev = store_node;
                                optimizations++;
                            }
                        }
                    }
                }
            }

            curr = end_of_func->next;
            continue;
        }
        curr = curr->next;
    }

    return optimizations;
}

// -------------------------------------------------------------------
// Pass: Loop-Invariant Register Promotion (CALL-Free Loops)
// -------------------------------------------------------------------
int pass_promote_loop_registers(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr) {
        // 1. Identify Loop Headers (e.g., __for_1_start:, __while_1_start:)
        if (curr->type == OP_LABEL) {
            char start_lbl[128] = {0};
            safe_str_copy(start_lbl, curr->raw, sizeof(start_lbl));
            char *colon = strchr(start_lbl, ':');
            if (colon) *colon = '\0';
            trim(start_lbl);

            size_t len = strlen(start_lbl);
            if (len < 6 || !str_case_eq(start_lbl + len - 6, "_start")) {
                curr = curr->next;
                continue;
            }

            // 2. Find the Loop Back-Edge (Unconditional JMP back to start_lbl)
            AsmNode *end_jmp = NULL;
            AsmNode *scan = curr->next;
            while (scan) {
                if (scan->type == OP_LABEL && strncmp(trim(scan->raw), "__function_", 11) == 0) {
                    break;
                }
                if (str_case_eq(scan->mnemonic, "JMP") && str_case_eq(trim(scan->dst_op.raw), start_lbl)) {
                    end_jmp = scan;
                    break;
                }
                scan = scan->next;
            }

            if (!end_jmp) {
                curr = curr->next;
                continue;
            }

            // 3. Find the designated Loop Exit Label
            AsmNode *exit_label_node = end_jmp->next;
            while (exit_label_node && exit_label_node->type == OP_OTHER &&
                  (exit_label_node->raw[0] == '\0' || exit_label_node->raw[0] == ';')) {
                exit_label_node = exit_label_node->next;
            }

            if (!exit_label_node || exit_label_node->type != OP_LABEL) {
                curr = end_jmp->next;
                continue;
            }

            char exit_lbl[128] = {0};
            safe_str_copy(exit_lbl, exit_label_node->raw, sizeof(exit_lbl));
            colon = strchr(exit_lbl, ':');
            if (colon) *colon = '\0';
            trim(exit_lbl);

            // ----------------------------------------------------------------
            // Phase A: Safety Scan across the Loop Body
            // ----------------------------------------------------------------
            bool loop_safe = true;
            bool reg_used[16] = {0};
            int slot_read[128] = {0};
            int slot_written[128] = {0};

            for (AsmNode *n = curr->next; n && n != end_jmp; n = n->next) {
                // Rule 1: No CALLs, RETs, or HLTs inside the loop
                if (str_case_eq(n->mnemonic, "CALL") || str_case_eq(n->mnemonic, "RET") || str_case_eq(n->mnemonic, "HLT")) {
                    loop_safe = false;
                    break;
                }

                // Rule 2: No BP Address-Taking (direct or indirect)
                if (n->type != OP_PUSH && n->type != OP_POP && !str_case_eq(n->mnemonic, "MOV")) {
                    if ((n->dst_op.mode == MODE_REG && str_case_eq(n->dst_op.reg, "BP")) ||
                        (n->src_op.mode == MODE_REG && str_case_eq(n->src_op.reg, "BP")) ||
                        (n->dst_op.mode == MODE_INDIRECT && str_case_eq(n->dst_op.reg, "BP")) ||
                        (n->src_op.mode == MODE_INDIRECT && str_case_eq(n->src_op.reg, "BP"))) {
                        loop_safe = false;
                        break;
                    }
                }

                // Rule 3: Verify all branches jump inside loop, to start, or to exit
                if (str_case_eq(n->mnemonic, "JT") || str_case_eq(n->mnemonic, "JF") || str_case_eq(n->mnemonic, "JMP")) {
                    char target[128] = {0};
                    safe_str_copy(target, trim(n->dst_op.raw), sizeof(target));
                    if (!str_case_eq(target, start_lbl) && !str_case_eq(target, exit_lbl)) {
                        bool internal_lbl = false;
                        for (AsmNode *chk = curr->next; chk && chk != end_jmp; chk = chk->next) {
                            if (chk->type == OP_LABEL) {
                                char int_copy[128] = {0};
                                safe_str_copy(int_copy, chk->raw, sizeof(int_copy));
                                char *c = strchr(int_copy, ':');
                                if (c) *c = '\0';
                                if (str_case_eq(trim(int_copy), target)) { internal_lbl = true; break; }
                            }
                        }
                        if (!internal_lbl) { loop_safe = false; break; }
                    }
                }

                // Track ALL register usage (direct + indirect)
                if (n->dst_op.mode == MODE_REG) {
                    int idx = get_reg_index(n->dst_op.reg);
                    if (idx >= 0 && idx < 16) reg_used[idx] = true;
                }
                if (n->src_op.mode == MODE_REG) {
                    int idx = get_reg_index(n->src_op.reg);
                    if (idx >= 0 && idx < 16) reg_used[idx] = true;
                }
                if (n->dst_op.mode == MODE_INDIRECT && n->dst_op.reg[0] != '\0') {
                    int idx = get_reg_index(n->dst_op.reg);
                    if (idx >= 0 && idx < 16) reg_used[idx] = true;
                }
                if (n->src_op.mode == MODE_INDIRECT && n->src_op.reg[0] != '\0') {
                    int idx = get_reg_index(n->src_op.reg);
                    if (idx >= 0 && idx < 16) reg_used[idx] = true;
                }

                // Track stack slot reads and writes
                if (n->dst_op.mode == MODE_INDIRECT && str_case_eq(n->dst_op.reg, "BP") && n->dst_op.offset < 0) {
                    int abs_off = -n->dst_op.offset;
                    if (abs_off < 128) slot_written[abs_off]++;
                }
                if (n->src_op.mode == MODE_INDIRECT && str_case_eq(n->src_op.reg, "BP") && n->src_op.offset < 0) {
                    int abs_off = -n->src_op.offset;
                    if (abs_off < 128) slot_read[abs_off]++;
                }
            }

            if (!loop_safe) {
                curr = end_jmp->next;
                continue;
            }

            // ----------------------------------------------------------------
            // Phase B: Execute Loop Promotion
            // ----------------------------------------------------------------
            AsmNode *store_insert_pt = exit_label_node;
            int free_reg_for_off[128] = {0}; // Track register assignment per offset

            for (int off = 1; off < 128; off++) {
                if (slot_read[off] + slot_written[off] >= 2) {
                    int free_reg = -1;
                    for (int r = 1; r <= 13; r++) {
                        if (!reg_used[r]) {
                            free_reg = r;
                            free_reg_for_off[off] = free_reg;
                            reg_used[r] = true;
                            break;
                        }
                    }

                    if (free_reg == -1) break;

                    char reg_name[16];
                    snprintf(reg_name, sizeof(reg_name), "R%d", free_reg);

                    // 1. Insert Pre-Header Load AFTER loop start label
                    char src_str[32];
                    snprintf(src_str, sizeof(src_str), "[BP-%d]", off);
                    char load_raw[64];
                    snprintf(load_raw, sizeof(load_raw), "    MOV %s, %s", reg_name, src_str);

                    AsmNode *load_node = create_node(load_raw, OP_MOV, "MOV", reg_name, src_str);
                    load_node->has_dst = true;
                    load_node->has_src = true;
                    load_node->dst_op = parse_operand(reg_name);
                    load_node->src_op = parse_operand(src_str);

                    // Insert AFTER the loop label (not before!)
                    load_node->next = curr->next;
                    load_node->prev = curr;
                    if (curr->next) curr->next->prev = load_node;
                    curr->next = load_node;

                    // 2. Insert Exit Store AFTER loop exit label (if slot was written)
                    if (slot_written[off] > 0) {
                        char dst_str[32];
                        snprintf(dst_str, sizeof(dst_str), "[BP-%d]", off);
                        char store_raw[64];
                        snprintf(store_raw, sizeof(store_raw), "    MOV %s, %s", dst_str, reg_name);

                        AsmNode *store_node = create_node(store_raw, OP_MOV, "MOV", dst_str, reg_name);
                        store_node->has_dst = true;
                        store_node->has_src = true;
                        store_node->dst_op = parse_operand(dst_str);
                        store_node->src_op = parse_operand(reg_name);

                        store_node->next = store_insert_pt->next;
                        store_node->prev = store_insert_pt;
                        if (store_insert_pt->next) store_insert_pt->next->prev = store_node;
                        store_insert_pt->next = store_node;
                        store_insert_pt = store_node;
                    }

                    // 3. Rewrite all [BP-off] references inside loop body to R_free
                    for (AsmNode *n = curr->next; n && n != end_jmp; n = n->next) {
                        bool modified = false;
                        if (n->dst_op.mode == MODE_INDIRECT && str_case_eq(n->dst_op.reg, "BP") && n->dst_op.offset == -off) {
                            promote_operand_to_reg(&n->dst_op, reg_name);
                            modified = true;
                        }
                        if (n->src_op.mode == MODE_INDIRECT && str_case_eq(n->src_op.reg, "BP") && n->src_op.offset == -off) {
                            promote_operand_to_reg(&n->src_op, reg_name);
                            modified = true;
                        }
                        if (modified) {
                            if (n->has_dst && n->has_src) {
                                snprintf(n->raw, sizeof(n->raw), "    %s %s, %s", n->mnemonic, n->dst_op.raw, n->src_op.raw);
                            } else if (n->has_dst) {
                                snprintf(n->raw, sizeof(n->raw), "    %s %s", n->mnemonic, n->dst_op.raw);
                            }
                            optimizations++;
                        }
                    }
                }
            }

            // Insert stores at ALL loop exits (not just the main one)
            for (int off = 1; off < 128; off++) {
                if (slot_read[off] + slot_written[off] >= 2 && free_reg_for_off[off] != 0 && slot_written[off] > 0) {
                    char dst_str[32];
                    snprintf(dst_str, sizeof(dst_str), "[BP-%d]", off);
                    char reg_name[16];
                    snprintf(reg_name, sizeof(reg_name), "R%d", free_reg_for_off[off]);
                    char store_raw[64];
                    snprintf(store_raw, sizeof(store_raw), "    MOV %s, %s", dst_str, reg_name);

                    // Find all jumps that exit the loop
                    for (AsmNode *n = curr->next; n && n != end_jmp; n = n->next) {
                        if ((str_case_eq(n->mnemonic, "JT") || str_case_eq(n->mnemonic, "JF") ||
                             str_case_eq(n->mnemonic, "JMP"))) {
                            char target[128] = {0};
                            safe_str_copy(target, trim(n->dst_op.raw), sizeof(target));
                            if (!is_inside_loop(target, curr, end_jmp) &&
                                !str_case_eq(target, start_lbl) &&
                                !str_case_eq(target, exit_lbl)) {
                                // This is an exit - find the target label and insert store before it
                                AsmNode *exit_target = n->next;
                                while (exit_target && exit_target->type != OP_LABEL) {
                                    exit_target = exit_target->next;
                                }
                                if (exit_target) {
                                    AsmNode *store_node = create_node(store_raw, OP_MOV, "MOV", dst_str, reg_name);
                                    store_node->prev = exit_target->prev;
                                    store_node->next = exit_target;
                                    if (exit_target->prev) exit_target->prev->next = store_node;
                                    exit_target->prev = store_node;
                                    optimizations++;
                                }
                            }
                        }
                    }
                }
            }

            curr = end_jmp->next;
            continue;
        }
        curr = curr->next;
    }

    return optimizations;
}
