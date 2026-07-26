#include "v32opt.h"

// -------------------------------------------------------------------
// OPTIMIZATION CATEGORY: Control Flow Graph & Global Data-Flow Analysis
// Builds CFG for interprocedural analysis and performs constant
// propagation/folding across basic blocks.
// -------------------------------------------------------------------

// ===================================================================
// CONTROL FLOW GRAPH: EDGE MANAGEMENT
// Adds a directed edge from source to destination block in the CFG.
// Handles dynamic resizing of predecessor/successor arrays.
//   - src: Source basic block
//   - dst: Destination basic block
// ===================================================================
void add_edge(BasicBlock *src, BasicBlock *dst) {
    if (!src || !dst) return;

    // Avoid duplicate edges
    for (int i = 0; i < src->num_succs; i++) {
        if (src->succs[i] == dst) return;
    }

    // Resize successor array if needed
    if (src->num_succs >= src->cap_succs) {
        src->cap_succs = src->cap_succs == 0 ? 4 : src->cap_succs * 2;
        src->succs = realloc(src->succs, src->cap_succs * sizeof(BasicBlock*));
    }
    src->succs[src->num_succs++] = dst;

    // Resize predecessor array if needed
    if (dst->num_preds >= dst->cap_preds) {
        dst->cap_preds = dst->cap_preds == 0 ? 4 : dst->cap_preds * 2;
        dst->preds = realloc(dst->preds, dst->cap_preds * sizeof(BasicBlock*));
    }
    dst->preds[dst->num_preds++] = src;
}

// ===================================================================
// BRANCH DETECTION UTILITIES
//
// FIX: Original code used case-sensitive strcmp against uppercase
// mnemonics ("JMP", "JT", etc.), but Vircon32 compiler emits lowercase
// ("jmp", "jt", "jf", "call", "ret", "hlt"). Using str_case_eq
// makes these work regardless of case.
// ===================================================================

// ===================================================================
// UNCONDITIONAL BRANCH CHECK
// Returns true if the instruction is an unconditional branch:
//   - JMP: Jump to label
//   - RET: Return from function
//   - HLT: Halt execution
// ===================================================================
bool is_unconditional_branch(AsmNode *node) {
    if (!node) return false;
    return (str_case_eq(node->mnemonic, "JMP") ||
            str_case_eq(node->mnemonic, "RET") ||
            str_case_eq(node->mnemonic, "HLT"));
}

// ===================================================================
// CONDITIONAL BRANCH CHECK
// Returns true if the instruction is a conditional branch:
//   - JT: Jump if true
//   - JF: Jump if false
// ===================================================================
bool is_conditional_branch(AsmNode *node) {
    if (!node) return false;
    return (str_case_eq(node->mnemonic, "JT") || str_case_eq(node->mnemonic, "JF"));
}

// ===================================================================
// BRANCH OR CALL CHECK
// Returns true if the instruction is any control transfer:
//   - Unconditional branches (JMP, RET, HLT)
//   - Conditional branches (JT, JF)
//   - Function calls (CALL)
// ===================================================================
bool is_branch_or_call(AsmNode *node) {
    return is_unconditional_branch(node) ||
           is_conditional_branch(node) ||
           (node && str_case_eq(node->mnemonic, "CALL"));
}

// ===================================================================
// CFG: FIND BLOCK BY LABEL
// Searches the CFG for a block containing a specific label.
//   - cfg: Control Flow Graph
//   - label: Label name to search for
// Returns: Pointer to the BasicBlock, or NULL if not found
// ===================================================================
BasicBlock* find_block_by_label(ControlFlowGraph *cfg, const char *label) {
    for (int i = 0; i < cfg->num_blocks; i++) {
        BasicBlock *b = cfg->blocks[i];
        for (int l = 0; l < b->num_labels; l++) {
            if (strcmp(b->labels[l], label) == 0) return b;
        }
    }
    return NULL;
}

// ===================================================================
// CONTROL FLOW GRAPH CONSTRUCTION
//
// Builds a CFG from the assembly instruction list:
//   1. Creates blocks at label boundaries and branch targets
//   2. Links blocks via edges based on control flow
//   3. Handles both conditional and unconditional branches
//
// FIX: Uses str_case_eq for mnemonic matching (Vircon32 emits lowercase)
// ===================================================================
ControlFlowGraph* build_cfg(AsmNode *head) {
    ControlFlowGraph *cfg = calloc(1, sizeof(ControlFlowGraph));
    if (!head) return cfg;

    BasicBlock *current_block = NULL;
    char pending_labels[8][128];
    int pending_label_count = 0;

    // Skip dummy head if present
    AsmNode *curr = (head->type == OP_OTHER && head->raw[0] == '\0') ? head->next : head;

    // --- PHASE 1: CREATE BASIC BLOCKS ---
    while (curr) {
        // --- Label: Start new block ---
        if (curr->type == OP_LABEL) {
            char lbl[128] = {0};
            safe_str_copy(lbl, curr->raw, sizeof(lbl));
            char *colon = strchr(lbl, ':');
            if (colon) *colon = '\0';

            if (pending_label_count < 8) {
                safe_str_copy(pending_labels[pending_label_count++], lbl, sizeof(pending_labels[0]));
            }

            current_block = NULL; // Force new block creation
            curr = curr->next;
            continue;
        }

        // Skip comments/blanks
        if (curr->type == OP_OTHER && (curr->raw[0] == '\0' || curr->raw[0] == ';')) {
            curr = curr->next;
            continue;
        }

        // --- Start new block if needed ---
        if (!current_block) {
            current_block = calloc(1, sizeof(BasicBlock));
            current_block->id = cfg->num_blocks;
            current_block->first_ins = curr;

            // Attach pending labels to this block
            for (int l = 0; l < pending_label_count; l++) {
                safe_str_copy(current_block->labels[l], pending_labels[l], sizeof(current_block->labels[l]));
            }
            current_block->num_labels = pending_label_count;
            pending_label_count = 0;

            // Add to CFG
            if (cfg->num_blocks >= cfg->cap_blocks) {
                cfg->cap_blocks = cfg->cap_blocks == 0 ? 8 : cfg->cap_blocks * 2;
                cfg->blocks = realloc(cfg->blocks, cfg->cap_blocks * sizeof(BasicBlock*));
            }
            cfg->blocks[cfg->num_blocks++] = current_block;
        }

        // Update last instruction in current block
        current_block->last_ins = curr;

        // --- Block terminator: branch or call ---
        if (is_branch_or_call(curr)) {
            current_block = NULL; // Next instruction starts new block
        }

        curr = curr->next;
    }

    // --- PHASE 2: ADD CONTROL FLOW EDGES ---
    for (int i = 0; i < cfg->num_blocks; i++) {
        BasicBlock *block = cfg->blocks[i];
        AsmNode *term = block->last_ins;

        if (!term) continue;

        // FIX: Use str_case_eq for case-insensitive matching
        if (str_case_eq(term->mnemonic, "JMP")) {
            // Unconditional jump: edge to target label
            BasicBlock *target = find_block_by_label(cfg, term->dst_op.raw);
            if (target) add_edge(block, target);
        }
        else if (is_conditional_branch(term)) {
            // Conditional branch: edge to target AND fallthrough
            BasicBlock *target = find_block_by_label(cfg, term->src_op.raw);
            if (target) add_edge(block, target);
            if (i + 1 < cfg->num_blocks) add_edge(block, cfg->blocks[i + 1]); // Fallthrough
        }
        else if (str_case_eq(term->mnemonic, "CALL")) {
            // Function call: edge to next block (return continues sequentially)
            if (i + 1 < cfg->num_blocks) add_edge(block, cfg->blocks[i + 1]);
        }
        else if (str_case_eq(term->mnemonic, "RET") || str_case_eq(term->mnemonic, "HLT")) {
            // End of execution path: no successors
        }
        else {
            // Non-branch: edge to next block (fallthrough)
            if (i + 1 < cfg->num_blocks) add_edge(block, cfg->blocks[i + 1]);
        }
    }

    return cfg;
}

// ===================================================================
// CFG EXPORT TO DOT
// Exports the CFG in Graphviz DOT format for visualization.
//   - filename: Output .dot file path
//   - cfg: Control Flow Graph to export
// ===================================================================
void export_cfg_to_dot(const char *filename, ControlFlowGraph *cfg) {
    FILE *fp = fopen(filename, "w");
    if (!fp) return;

    fprintf(fp, "digraph Vircon32_CFG {\n");
    fprintf(fp, "    node [shape=box, fontname=\"Courier\"];\n\n");

    // --- Export each block ---
    for (int i = 0; i < cfg->num_blocks; i++) {
        BasicBlock *b = cfg->blocks[i];

        // Block header with labels
        fprintf(fp, "    block_%d [label=\"Block %d", b->id, b->id);
        if (b->num_labels > 0) {
            fprintf(fp, " (");
            for (int l = 0; l < b->num_labels; l++) {
                fprintf(fp, "%s%s", b->labels[l], (l < b->num_labels - 1) ? ", " : "");
            }
            fprintf(fp, ")");
        }
        fprintf(fp, "\\n----------------\\n");

        // Block instructions
        for (AsmNode *ins = b->first_ins; ins != NULL; ins = ins->next) {
            fprintf(fp, "%s\\n", ins->raw);
            if (ins == b->last_ins) break;
        }
        fprintf(fp, "\"];\n");

        // Edges to successors
        for (int s = 0; s < b->num_succs; s++) {
            fprintf(fp, "    block_%d -> block_%d;\n", b->id, b->succs[s]->id);
        }
    }

    fprintf(fp, "}\n");
    fclose(fp);
}

// ===================================================================
// CFG MEMORY CLEANUP
// Frees all memory allocated for a Control Flow Graph.
//   - cfg: CFG to free
// ===================================================================
void free_cfg(ControlFlowGraph *cfg) {
    if (!cfg) return;
    for (int i = 0; i < cfg->num_blocks; i++) {
        free(cfg->blocks[i]->preds);
        free(cfg->blocks[i]->succs);
        free(cfg->blocks[i]);
    }
    free(cfg->blocks);
    free(cfg);
}

// ===================================================================
// CONSTANT PROPAGATION: LATTICE MERGE
// Merges two register states according to lattice semantics:
//   - VAL_TOP (unknown) is identity element
//   - VAL_BOTTOM (conflicting) absorbs all
//   - VAL_CONST merges only if both are CONST with same value
//   - Otherwise results in VAL_BOTTOM
// ===================================================================
RegState merge_reg(RegState a, RegState b) {
    if (a.type == VAL_TOP) return b;
    if (b.type == VAL_TOP) return a;
    if (a.type == VAL_BOTTOM || b.type == VAL_BOTTOM) return (RegState){VAL_BOTTOM, 0};
    if (a.type == VAL_CONST && b.type == VAL_CONST && a.val == b.val) return a;
    return (RegState){VAL_BOTTOM, 0};
}

// ===================================================================
// CONSTANT PROPAGATION: TRANSFER FUNCTION
// Applies the effect of a basic block's instructions to the state.
//   - block: Basic block to process
// Returns: true if the block's output state changed
//
// FIXES:
//   - CALL invalidates ALL registers (R0-R15) - a called function may
//     overwrite any register, and we have no interprocedural info
//   - MOV/IADD now check dst_op.mode == MODE_REG to avoid treating
//     indirect operands (e.g., [BP+2]) as register writes
//   - Float immediates (e.g., "0.500000") are flagged as is_float in
//     parse_operand and must NOT be treated as VAL_CONST here
//   - Any instruction writing to a register that isn't MOV/IADD
//     (ISUB, IMUL, IDIV, comparisons, etc.) invalidates that register
// ===================================================================
bool apply_transfer_function(BasicBlock *block) {
    BlockState current = block->in_state;

    for (AsmNode *node = block->first_ins; node != NULL; node = node->next) {
        // --- CALL: Invalidate all registers ---
        // A function call may modify any register (especially R0 for return value).
        // Without interprocedural analysis, we must assume the worst.
        if (str_case_eq(node->mnemonic, "CALL")) {
            for (int r = 0; r < 16; r++) {
                current.regs[r] = (RegState){VAL_BOTTOM, 0};
            }
            if (node == block->last_ins) break;
            continue;
        }

        // --- MOV: Register-to-register or immediate ---
        // FIX: Gate on dst_op.mode == MODE_REG to avoid misinterpreting
        // indirect destinations (e.g., [BP+2]) as writes to BP itself.
        if (node->type == OP_MOV && node->dst_op.mode == MODE_REG) {
            int dst_reg = get_reg_index(node->dst_op.reg);
            if (dst_reg >= 0) {
                // FIX: Float immediates must not be tracked as VAL_CONST.
                // parse_operand sets immediate=0 for ALL floats (since strtoul
                // stops at '.'), which is only correct for 0.0.
                if (node->src_op.mode == MODE_IMMEDIATE && !node->src_op.is_float) {
                    current.regs[dst_reg] = (RegState){VAL_CONST, node->src_op.immediate};
                } else if (node->src_op.mode == MODE_REG) {
                    int src_reg = get_reg_index(node->src_op.reg);
                    current.regs[dst_reg] = (src_reg >= 0) ? current.regs[src_reg] : (RegState){VAL_BOTTOM, 0};
                } else {
                    current.regs[dst_reg] = (RegState){VAL_BOTTOM, 0};
                }
            }
        }
        // --- IADD: Register addition ---
        // FIX: Same mode check as MOV above.
        else if (node->type == OP_IADD && node->dst_op.mode == MODE_REG) {
            int dst_reg = get_reg_index(node->dst_op.reg);
            if (dst_reg >= 0) {
                RegState dst_st = current.regs[dst_reg];
                // FIX: Float immediate guard
                RegState src_st = (node->src_op.mode == MODE_IMMEDIATE && !node->src_op.is_float)
                    ? (RegState){VAL_CONST, node->src_op.immediate}
                    : (node->src_op.mode == MODE_REG ? current.regs[get_reg_index(node->src_op.reg)] : (RegState){VAL_BOTTOM, 0});

                if (dst_st.type == VAL_CONST && src_st.type == VAL_CONST) {
                    current.regs[dst_reg] = (RegState){VAL_CONST, dst_st.val + src_st.val};
                } else {
                    current.regs[dst_reg] = (RegState){VAL_BOTTOM, 0};
                }
            }
        }
        // --- All other register-writing instructions ---
        // FIX: Any instruction that writes to a register and isn't MOV/IADD
        // must invalidate that register's state. PUSH is the exception:
        // it reads the register (to push it), but doesn't modify it.
        else if (node->has_dst && node->dst_op.mode == MODE_REG && node->type != OP_PUSH) {
            int dst_reg = get_reg_index(node->dst_op.reg);
            if (dst_reg >= 0) {
                current.regs[dst_reg] = (RegState){VAL_BOTTOM, 0};
            }
        }

        if (node == block->last_ins) break;
    }

    bool changed = memcmp(&block->out_state, &current, sizeof(BlockState)) != 0;
    block->out_state = current;
    return changed;
}

// ===================================================================
// CONSTANT PROPAGATION: WORKLIST ALGORITHM
// Propagates constants through the CFG using a worklist algorithm:
//   1. Initialize all blocks' in-state to VAL_TOP
//   2. For each block, merge out-states from all predecessors
//   3. Apply transfer function to compute new out-state
//   4. If out-state changed, add successors to worklist
//   5. Repeat until no changes (fixed point)
// ===================================================================
void propagate_constants_cfg(ControlFlowGraph *cfg) {
    if (!cfg || cfg->num_blocks == 0) return;

    bool *in_worklist = calloc(cfg->num_blocks, sizeof(bool));
    int *worklist = malloc(cfg->num_blocks * sizeof(int));
    int worklist_size = 0;

    // Initialize worklist with all blocks
    for (int i = 0; i < cfg->num_blocks; i++) {
        worklist[worklist_size++] = i;
        in_worklist[i] = true;
    }

    while (worklist_size > 0) {
        int b_idx = worklist[--worklist_size];
        in_worklist[b_idx] = false;
        BasicBlock *block = cfg->blocks[b_idx];

        // Compute in-state: merge all predecessor out-states
        BlockState new_in;
        for (int r = 0; r < 16; r++) new_in.regs[r] = (RegState){VAL_TOP, 0};

        for (int p = 0; p < block->num_preds; p++) {
            BasicBlock *pred = block->preds[p];
            for (int r = 0; r < 16; r++) {
                new_in.regs[r] = merge_reg(new_in.regs[r], pred->out_state.regs[r]);
            }
        }
        block->in_state = new_in;

        // Apply transfer function and propagate if changed
        if (apply_transfer_function(block)) {
            for (int s = 0; s < block->num_succs; s++) {
                int succ_id = block->succs[s]->id;
                if (!in_worklist[succ_id]) {
                    worklist[worklist_size++] = succ_id;
                    in_worklist[succ_id] = true;
                }
            }
        }
    }

    free(in_worklist);
    free(worklist);
}

// ===================================================================
// CONSTANT FOLDING: APPLY PROPAGATED CONSTANTS
// Replaces register operands with constant values where possible.
// Uses the results of propagate_constants_cfg() to fold:
//   - MOV R1, R2 → MOV R1, const (if R2 has known constant value)
//   - Updates the raw text to reflect the folded constant
//
// FIXES:
//   - Same CALL invalidation as apply_transfer_function
//   - Same mode checks for indirect operands
//   - Same float immediate guards
//   - Has its own register state tracking (must mirror transfer function)
// ===================================================================
int fold_constants_cfg(ControlFlowGraph *cfg) {
    int optimizations = 0;
    if (!cfg) return 0;

    for (int i = 0; i < cfg->num_blocks; i++) {
        BasicBlock *block = cfg->blocks[i];
        BlockState current = block->in_state;

        for (AsmNode *node = block->first_ins; node != NULL; node = node->next) {
            // --- CALL: Invalidate all registers ---
            // FIX: Same reasoning as apply_transfer_function
            if (str_case_eq(node->mnemonic, "CALL")) {
                for (int r = 0; r < 16; r++) {
                    current.regs[r] = (RegState){VAL_BOTTOM, 0};
                }
                if (node == block->last_ins) break;
                continue;
            }

            // --- MOV R1, R2: Fold if R2 is constant ---
            if (node->type == OP_MOV && node->src_op.mode == MODE_REG && node->dst_op.mode == MODE_REG) {
                int src_reg = get_reg_index(node->src_op.reg);
                if (src_reg >= 0 && current.regs[src_reg].type == VAL_CONST) {
                    int const_val = current.regs[src_reg].val;
                    node->src_op.mode = MODE_IMMEDIATE;
                    node->src_op.immediate = const_val;
                    snprintf(node->src_op.raw, sizeof(node->src_op.raw), "0x%X", (unsigned int)const_val);
                    snprintf(node->raw, sizeof(node->raw), "    MOV %s, 0x%X", node->dst_op.reg, (unsigned int)const_val);
                    optimizations++;
                }
            }

            // --- Update state for MOV (same logic as transfer function) ---
            // FIX: Same mode check and float guard as apply_transfer_function
            if (node->type == OP_MOV && node->dst_op.mode == MODE_REG) {
                int dst_reg = get_reg_index(node->dst_op.reg);
                if (dst_reg >= 0) {
                    if (node->src_op.mode == MODE_IMMEDIATE && !node->src_op.is_float) {
                        current.regs[dst_reg] = (RegState){VAL_CONST, node->src_op.immediate};
                    } else if (node->src_op.mode == MODE_REG) {
                        int src_reg = get_reg_index(node->src_op.reg);
                        current.regs[dst_reg] = (src_reg >= 0) ? current.regs[src_reg] : (RegState){VAL_BOTTOM, 0};
                    } else {
                        current.regs[dst_reg] = (RegState){VAL_BOTTOM, 0};
                    }
                }
            }
            // --- Update state for IADD ---
            // FIX: Same logic as apply_transfer_function
            else if (node->type == OP_IADD && node->dst_op.mode == MODE_REG) {
                int dst_reg = get_reg_index(node->dst_op.reg);
                if (dst_reg >= 0) {
                    RegState dst_st = current.regs[dst_reg];
                    RegState src_st = (node->src_op.mode == MODE_IMMEDIATE && !node->src_op.is_float)
                        ? (RegState){VAL_CONST, node->src_op.immediate}
                        : (node->src_op.mode == MODE_REG ? current.regs[get_reg_index(node->src_op.reg)] : (RegState){VAL_BOTTOM, 0});
                    if (dst_st.type == VAL_CONST && src_st.type == VAL_CONST) {
                        current.regs[dst_reg] = (RegState){VAL_CONST, dst_st.val + src_st.val};
                    } else {
                        current.regs[dst_reg] = (RegState){VAL_BOTTOM, 0};
                    }
                }
            }
            // --- Invalidate for all other register-writing instructions ---
            // FIX: Same catch-all as apply_transfer_function
            else if (node->has_dst && node->dst_op.mode == MODE_REG && node->type != OP_PUSH) {
                int dst_reg = get_reg_index(node->dst_op.reg);
                if (dst_reg >= 0) {
                    current.regs[dst_reg] = (RegState){VAL_BOTTOM, 0};
                }
            }

            if (node == block->last_ins) break;
        }
    }

    return optimizations;
}
