#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

// -------------------------------------------------------------------
// Enums & Data Structures
// -------------------------------------------------------------------

typedef enum {
    OP_MOV, OP_IADD, OP_ISUB, OP_IMUL, OP_IEQ, OP_INE, 
    OP_CIB, OP_PUSH, OP_POP, OP_BNOT, OP_OTHER, OP_LABEL
} OpType;

typedef enum {
    MODE_NONE,
    MODE_REG,         // e.g., R0, R1, SP, BP
    MODE_IMMEDIATE,   // e.g., 42, -10, 0x20
    MODE_INDIRECT     // e.g., [R1], [BP+4], [BP-8]
} AddressingMode;

typedef struct {
    AddressingMode mode;
    char reg[16];     // Base register name ("R0", "BP", "SP")
    int offset;       // Displacement offset (+4, -8, 0)
    int immediate;    // Numeric value if MODE_IMMEDIATE
    char raw[32];     // Original operand string representation
} Operand;

typedef struct AsmNode {
    OpType type;
    char raw[128];
    char mnemonic[16];
    
    Operand dst_op;
    Operand src_op;
    bool has_dst;
    bool has_src;

    struct AsmNode *prev;
    struct AsmNode *next;
} AsmNode;

// Lattice states for Global Constant Propagation
typedef enum { VAL_TOP, VAL_CONST, VAL_BOTTOM } ValType;

typedef struct {
    ValType type;
    int val;
} RegState;

typedef struct {
    RegState regs[16]; // Vircon32 registers R0-R15
} BlockState;

typedef struct BasicBlock BasicBlock;

struct BasicBlock {
    int id;
    char labels[8][32]; // Labels pointing directly to this block
    int num_labels;

    AsmNode *first_ins;
    AsmNode *last_ins;

    BasicBlock **preds;
    int num_preds;
    int cap_preds;

    BasicBlock **succs;
    int num_succs;
    int cap_succs;

    BlockState in_state;   // Computed entry state
    BlockState out_state;  // Computed exit state after transfer function
};

typedef struct {
    BasicBlock **blocks;
    int num_blocks;
    int cap_blocks;
} ControlFlowGraph;

// -------------------------------------------------------------------
// String Parsing & Helper Utilities
// -------------------------------------------------------------------

static char* trim(char *str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static bool str_case_eq(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        if (toupper((unsigned char)*s1) != toupper((unsigned char)*s2)) return false;
        s1++; s2++;
    }
    return *s1 == *s2;
}

// Maps register string to 0-15 index (R0-R13 -> 0-13, BP -> 14, SP -> 15)
int get_reg_index(const char *reg_str) {
    if (!reg_str || strlen(reg_str) == 0) return -1;
    if (str_case_eq(reg_str, "SP") || str_case_eq(reg_str, "R15")) return 15;
    if (str_case_eq(reg_str, "BP") || str_case_eq(reg_str, "R14")) return 14;
    if (toupper((unsigned char)reg_str[0]) == 'R') {
        int idx = atoi(reg_str + 1);
        if (idx >= 0 && idx < 16) return idx;
    }
    return -1;
}

Operand parse_operand(const char *str) {
    Operand op;
    memset(&op, 0, sizeof(Operand));
    if (!str || strlen(str) == 0) return op;

    strncpy(op.raw, str, sizeof(op.raw) - 1);

    // Indirect / Memory addressing: [REG+OFF], [REG-OFF], [REG]
    if (str[0] == '[' && str[strlen(str) - 1] == ']') {
        op.mode = MODE_INDIRECT;

        char inner[32] = {0};
        strncpy(inner, str + 1, strlen(str) - 2);

        char *plus_ptr  = strchr(inner, '+');
        char *minus_ptr = strrchr(inner, '-');

        if (plus_ptr) {
            *plus_ptr = '\0';
            strncpy(op.reg, trim(inner), sizeof(op.reg) - 1);
            op.offset = atoi(trim(plus_ptr + 1));
        } else if (minus_ptr && minus_ptr != inner) {
            *minus_ptr = '\0';
            strncpy(op.reg, trim(inner), sizeof(op.reg) - 1);
            op.offset = -atoi(trim(minus_ptr + 1));
        } else {
            strncpy(op.reg, trim(inner), sizeof(op.reg) - 1);
            op.offset = 0;
        }
    } 
    // Immediate constant
    else if (isdigit((unsigned char)str[0]) || (str[0] == '-' && isdigit((unsigned char)str[1]))) {
        op.mode = MODE_IMMEDIATE;
        op.immediate = atoi(str);
    } 
    // Direct Register
    else {
        op.mode = MODE_REG;
        strncpy(op.reg, str, sizeof(op.reg) - 1);
    }

    return op;
}

AsmNode* create_node(const char* raw, OpType type, const char* mnem, const char* dst, const char* src) {
    AsmNode *node = (AsmNode*)calloc(1, sizeof(AsmNode));
    if (raw) strncpy(node->raw, raw, sizeof(node->raw) - 1);
    node->type = type;
    if (mnem) strncpy(node->mnemonic, mnem, sizeof(node->mnemonic) - 1);
    
    if (dst && strlen(dst) > 0) {
        node->dst_op = parse_operand(dst);
        node->has_dst = true;
    }
    if (src && strlen(src) > 0) {
        node->src_op = parse_operand(src);
        node->has_src = true;
    }
    return node;
}

void remove_node(AsmNode *node) {
    if (node->prev) node->prev->next = node->next;
    if (node->next) node->next->prev = node->prev;
    free(node);
}

// -------------------------------------------------------------------
// Assembly File Parser & Writer
// -------------------------------------------------------------------

AsmNode* parse_vircon32_asm(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening input assembly file");
        exit(EXIT_FAILURE);
    }

    AsmNode *dummy_head = create_node(NULL, OP_OTHER, NULL, NULL, NULL);
    AsmNode *tail = dummy_head;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char raw[128];
        strncpy(raw, line, sizeof(raw) - 1);
        raw[sizeof(raw) - 1] = '\0';
        raw[strcspn(raw, "\r\n")] = '\0';

        char *trimmed = trim(line);

        if (strlen(trimmed) == 0 || trimmed[0] == ';') {
            AsmNode *node = create_node(raw, OP_OTHER, NULL, NULL, NULL);
            tail->next = node; node->prev = tail; tail = node;
            continue;
        }

        char code_part[128] = {0};
        char *comment_ptr = strchr(trimmed, ';');
        if (comment_ptr) {
            size_t len = comment_ptr - trimmed;
            strncpy(code_part, trimmed, len);
            code_part[len] = '\0';
        } else {
            strncpy(code_part, trimmed, sizeof(code_part) - 1);
        }
        char *code_trimmed = trim(code_part);

        if (code_trimmed[strlen(code_trimmed) - 1] == ':') {
            AsmNode *node = create_node(raw, OP_LABEL, NULL, NULL, NULL);
            tail->next = node; node->prev = tail; tail = node;
            continue;
        }

        char mnem[16] = {0}, dst[32] = {0}, src[32] = {0};
        char *space_ptr = strpbrk(code_trimmed, " \t");

        if (!space_ptr) {
            strncpy(mnem, code_trimmed, sizeof(mnem) - 1);
        } else {
            size_t mnem_len = space_ptr - code_trimmed;
            if (mnem_len >= sizeof(mnem)) mnem_len = sizeof(mnem) - 1;
            strncpy(mnem, code_trimmed, mnem_len);
            mnem[mnem_len] = '\0';

            char *operands = trim(space_ptr);
            char *comma_ptr = strchr(operands, ',');
            if (comma_ptr) {
                *comma_ptr = '\0';
                strncpy(dst, trim(operands), sizeof(dst) - 1);
                strncpy(src, trim(comma_ptr + 1), sizeof(src) - 1);
            } else {
                strncpy(dst, trim(operands), sizeof(dst) - 1);
            }
        }

        OpType type = OP_OTHER;
        if (str_case_eq(mnem, "MOV"))   type = OP_MOV;
        else if (str_case_eq(mnem, "IADD")) type = OP_IADD;
        else if (str_case_eq(mnem, "ISUB")) type = OP_ISUB;
        else if (str_case_eq(mnem, "IMUL")) type = OP_IMUL;
        else if (str_case_eq(mnem, "IEQ"))  type = OP_IEQ;
        else if (str_case_eq(mnem, "INE"))  type = OP_INE;
        else if (str_case_eq(mnem, "CIB"))  type = OP_CIB;
        else if (str_case_eq(mnem, "PUSH")) type = OP_PUSH;
        else if (str_case_eq(mnem, "POP"))  type = OP_POP;
        else if (str_case_eq(mnem, "BNOT")) type = OP_BNOT;

        AsmNode *node = create_node(raw, type, mnem, dst, src);
        tail->next = node; node->prev = tail; tail = node;
    }

    fclose(fp);
    return dummy_head;
}

void write_vircon32_asm(const char *filename, AsmNode *head) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror("Error opening output assembly file");
        exit(EXIT_FAILURE);
    }

    AsmNode *curr = (head && head->type == OP_OTHER && head->raw[0] == '\0') ? head->next : head;

    while (curr) {
        fprintf(fp, "%s\n", curr->raw);
        curr = curr->next;
    }

    fclose(fp);
}

// -------------------------------------------------------------------
// Local Peephole Passes
// -------------------------------------------------------------------

int pass_peephole_window2(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr && curr->next) {
        AsmNode *n1 = curr;
        AsmNode *n2 = curr->next;

        // Pattern A: IEQ / INE followed by CIB on same destination register
        if ((n1->type == OP_IEQ || n1->type == OP_INE) && 
             n2->type == OP_CIB && 
             n1->dst_op.mode == MODE_REG && n2->dst_op.mode == MODE_REG &&
             strcmp(n1->dst_op.reg, n2->dst_op.reg) == 0) 
        {
            remove_node(n2);
            optimizations++;
            continue;
        }

        // Pattern B: Redundant Double Inversion (BNOT R0 -> BNOT R0)
        if (n1->type == OP_BNOT && n2->type == OP_BNOT && 
            n1->dst_op.mode == MODE_REG && n2->dst_op.mode == MODE_REG &&
            strcmp(n1->dst_op.reg, n2->dst_op.reg) == 0) 
        {
            AsmNode *next_iter = n2->next;
            remove_node(n1);
            remove_node(n2);
            curr = next_iter;
            optimizations += 2;
            continue;
        }

        // Pattern C: Redundant PUSH/POP pair on same register
        if (n1->type == OP_PUSH && n2->type == OP_POP && 
            n1->dst_op.mode == MODE_REG && n2->dst_op.mode == MODE_REG &&
            strcmp(n1->dst_op.reg, n2->dst_op.reg) == 0) 
        {
            AsmNode *next_iter = n2->next;
            remove_node(n1);
            remove_node(n2);
            curr = next_iter;
            optimizations += 2;
            continue;
        }

        curr = curr->next;
    }

    return optimizations;
}

int pass_algebraic_simplifications(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr != NULL) {
        AsmNode *next = curr->next;

        // Self-Move Elimination: MOV R0, R0
        if (curr->type == OP_MOV && 
            curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_REG && 
            strcmp(curr->dst_op.reg, curr->src_op.reg) == 0) 
        {
            remove_node(curr);
            optimizations++;
            curr = next;
            continue;
        }

        // Addition/Subtraction by Zero: IADD R0, 0
        if ((curr->type == OP_IADD || curr->type == OP_ISUB) && 
            curr->src_op.mode == MODE_IMMEDIATE && curr->src_op.immediate == 0) 
        {
            remove_node(curr);
            optimizations++;
            curr = next;
            continue;
        }

        // Strength Reduction: IMUL R0, 2  -->  IADD R0, R0
        if (curr->type == OP_IMUL && 
            curr->src_op.mode == MODE_IMMEDIATE && curr->src_op.immediate == 2) 
        {
            curr->type = OP_IADD;
            strcpy(curr->mnemonic, "IADD");
            curr->src_op = curr->dst_op;
            snprintf(curr->raw, sizeof(curr->raw), "    IADD %s, %s", curr->dst_op.raw, curr->src_op.raw);
            optimizations++;
        }

        curr = next;
    }

    return optimizations;
}

int pass_store_to_load_forwarding(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr && curr->next) {
        AsmNode *n1 = curr;
        AsmNode *n2 = curr->next;

        // Match Pattern: MOV [REG+OFF], R_src  followed by  MOV R_dst, [REG+OFF]
        if (n1->type == OP_MOV && n2->type == OP_MOV &&
            n1->dst_op.mode == MODE_INDIRECT && n1->src_op.mode == MODE_REG &&
            n2->dst_op.mode == MODE_REG      && n2->src_op.mode == MODE_INDIRECT) 
        {
            if (strcmp(n1->dst_op.reg, n2->src_op.reg) == 0 && 
                n1->dst_op.offset == n2->src_op.offset) 
            {
                n2->src_op = n1->src_op;
                snprintf(n2->raw, sizeof(n2->raw), "    MOV %s, %s", n2->dst_op.reg, n2->src_op.reg);
                optimizations++;
            }
        }

        curr = curr->next;
    }

    return optimizations;
}

// -------------------------------------------------------------------
// Control Flow Graph (CFG) Construction
// -------------------------------------------------------------------

static void add_edge(BasicBlock *src, BasicBlock *dst) {
    if (!src || !dst) return;

    for (int i = 0; i < src->num_succs; i++) {
        if (src->succs[i] == dst) return;
    }

    if (src->num_succs >= src->cap_succs) {
        src->cap_succs = src->cap_succs == 0 ? 4 : src->cap_succs * 2;
        src->succs = realloc(src->succs, src->cap_succs * sizeof(BasicBlock*));
    }
    src->succs[src->num_succs++] = dst;

    if (dst->num_preds >= dst->cap_preds) {
        dst->cap_preds = dst->cap_preds == 0 ? 4 : dst->cap_preds * 2;
        dst->preds = realloc(dst->preds, dst->cap_preds * sizeof(BasicBlock*));
    }
    dst->preds[dst->num_preds++] = src;
}

static bool is_unconditional_branch(AsmNode *node) {
    if (!node) return false;
    return (strcmp(node->mnemonic, "JMP") == 0 ||
            strcmp(node->mnemonic, "RET") == 0 ||
            strcmp(node->mnemonic, "HLT") == 0);
}

static bool is_conditional_branch(AsmNode *node) {
    if (!node) return false;
    return (strcmp(node->mnemonic, "JT") == 0 || strcmp(node->mnemonic, "JF") == 0);
}

static bool is_branch_or_call(AsmNode *node) {
    return is_unconditional_branch(node) || 
           is_conditional_branch(node) || 
           (node && strcmp(node->mnemonic, "CALL") == 0);
}

static BasicBlock* find_block_by_label(ControlFlowGraph *cfg, const char *label) {
    for (int i = 0; i < cfg->num_blocks; i++) {
        BasicBlock *b = cfg->blocks[i];
        for (int l = 0; l < b->num_labels; l++) {
            if (strcmp(b->labels[l], label) == 0) return b;
        }
    }
    return NULL;
}

ControlFlowGraph* build_cfg(AsmNode *head) {
    ControlFlowGraph *cfg = calloc(1, sizeof(ControlFlowGraph));
    if (!head) return cfg;

    BasicBlock *current_block = NULL;
    char pending_labels[8][32];
    int pending_label_count = 0;

    AsmNode *curr = (head->type == OP_OTHER && head->raw[0] == '\0') ? head->next : head;

    while (curr) {
        if (curr->type == OP_LABEL) {
            char lbl[32] = {0};
            strncpy(lbl, curr->raw, sizeof(lbl) - 1);
            char *colon = strchr(lbl, ':');
            if (colon) *colon = '\0';

            if (pending_label_count < 8) {
                strncpy(pending_labels[pending_label_count++], lbl, 31);
            }

            current_block = NULL;
            curr = curr->next;
            continue;
        }

        if (curr->type == OP_OTHER && (curr->raw[0] == '\0' || curr->raw[0] == ';')) {
            curr = curr->next;
            continue;
        }

        if (!current_block) {
            current_block = calloc(1, sizeof(BasicBlock));
            current_block->id = cfg->num_blocks;
            current_block->first_ins = curr;

            for (int l = 0; l < pending_label_count; l++) {
                strncpy(current_block->labels[l], pending_labels[l], 31);
            }
            current_block->num_labels = pending_label_count;
            pending_label_count = 0;

            if (cfg->num_blocks >= cfg->cap_blocks) {
                cfg->cap_blocks = cfg->cap_blocks == 0 ? 8 : cfg->cap_blocks * 2;
                cfg->blocks = realloc(cfg->blocks, cfg->cap_blocks * sizeof(BasicBlock*));
            }
            cfg->blocks[cfg->num_blocks++] = current_block;
        }

        current_block->last_ins = curr;

        if (is_branch_or_call(curr)) {
            current_block = NULL;
        }

        curr = curr->next;
    }

    // Connect Control Flow Edges
    for (int i = 0; i < cfg->num_blocks; i++) {
        BasicBlock *block = cfg->blocks[i];
        AsmNode *term = block->last_ins;

        if (!term) continue;

        if (strcmp(term->mnemonic, "JMP") == 0) {
            BasicBlock *target = find_block_by_label(cfg, term->dst_op.raw);
            if (target) add_edge(block, target);
        }
        else if (is_conditional_branch(term)) {
            BasicBlock *target = find_block_by_label(cfg, term->dst_op.raw);
            if (target) add_edge(block, target);
            if (i + 1 < cfg->num_blocks) add_edge(block, cfg->blocks[i + 1]);
        }
        else if (strcmp(term->mnemonic, "CALL") == 0) {
            if (i + 1 < cfg->num_blocks) add_edge(block, cfg->blocks[i + 1]);
        }
        else if (strcmp(term->mnemonic, "RET") == 0 || strcmp(term->mnemonic, "HLT") == 0) {
            // Terminates execution sequence
        }
        else {
            if (i + 1 < cfg->num_blocks) add_edge(block, cfg->blocks[i + 1]);
        }
    }

    return cfg;
}

void export_cfg_to_dot(const char *filename, ControlFlowGraph *cfg) {
    FILE *fp = fopen(filename, "w");
    if (!fp) return;

    fprintf(fp, "digraph Vircon32_CFG {\n");
    fprintf(fp, "    node [shape=box, fontname=\"Courier\"];\n\n");

    for (int i = 0; i < cfg->num_blocks; i++) {
        BasicBlock *b = cfg->blocks[i];

        fprintf(fp, "    block_%d [label=\"Block %d", b->id, b->id);
        if (b->num_labels > 0) {
            fprintf(fp, " (");
            for (int l = 0; l < b->num_labels; l++) {
                fprintf(fp, "%s%s", b->labels[l], (l < b->num_labels - 1) ? ", " : "");
            }
            fprintf(fp, ")");
        }
        fprintf(fp, "\\n----------------\\n");

        for (AsmNode *ins = b->first_ins; ins != NULL; ins = ins->next) {
            fprintf(fp, "%s\\n", ins->raw);
            if (ins == b->last_ins) break;
        }
        fprintf(fp, "\"];\n");

        for (int s = 0; s < b->num_succs; s++) {
            fprintf(fp, "    block_%d -> block_%d;\n", b->id, b->succs[s]->id);
        }
    }

    fprintf(fp, "}\n");
    fclose(fp);
}

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

// -------------------------------------------------------------------
// Global Data-Flow Analysis: Constant Propagation
// -------------------------------------------------------------------

static RegState merge_reg(RegState a, RegState b) {
    if (a.type == VAL_TOP) return b;
    if (b.type == VAL_TOP) return a;
    if (a.type == VAL_BOTTOM || b.type == VAL_BOTTOM) return (RegState){VAL_BOTTOM, 0};
    if (a.type == VAL_CONST && b.type == VAL_CONST && a.val == b.val) return a;
    return (RegState){VAL_BOTTOM, 0};
}

static bool apply_transfer_function(BasicBlock *block) {
    BlockState current = block->in_state;

    for (AsmNode *node = block->first_ins; node != NULL; node = node->next) {
        if (node->type == OP_MOV) {
            int dst_reg = get_reg_index(node->dst_op.reg);
            if (dst_reg >= 0) {
                if (node->src_op.mode == MODE_IMMEDIATE) {
                    current.regs[dst_reg] = (RegState){VAL_CONST, node->src_op.immediate};
                } else if (node->src_op.mode == MODE_REG) {
                    int src_reg = get_reg_index(node->src_op.reg);
                    current.regs[dst_reg] = (src_reg >= 0) ? current.regs[src_reg] : (RegState){VAL_BOTTOM, 0};
                } else {
                    current.regs[dst_reg] = (RegState){VAL_BOTTOM, 0};
                }
            }
        }
        else if (node->type == OP_IADD) {
            int dst_reg = get_reg_index(node->dst_op.reg);
            if (dst_reg >= 0) {
                RegState dst_st = current.regs[dst_reg];
                RegState src_st = (node->src_op.mode == MODE_IMMEDIATE) 
                    ? (RegState){VAL_CONST, node->src_op.immediate} 
                    : current.regs[get_reg_index(node->src_op.reg)];

                if (dst_st.type == VAL_CONST && src_st.type == VAL_CONST) {
                    current.regs[dst_reg] = (RegState){VAL_CONST, dst_st.val + src_st.val};
                } else {
                    current.regs[dst_reg] = (RegState){VAL_BOTTOM, 0};
                }
            }
        }

        if (node == block->last_ins) break;
    }

    bool changed = memcmp(&block->out_state, &current, sizeof(BlockState)) != 0;
    block->out_state = current;
    return changed;
}

void propagate_constants_cfg(ControlFlowGraph *cfg) {
    if (!cfg || cfg->num_blocks == 0) return;

    bool *in_worklist = calloc(cfg->num_blocks, sizeof(bool));
    int *worklist = malloc(cfg->num_blocks * sizeof(int));
    int worklist_size = 0;

    for (int i = 0; i < cfg->num_blocks; i++) {
        worklist[worklist_size++] = i;
        in_worklist[i] = true;
    }

    while (worklist_size > 0) {
        int b_idx = worklist[--worklist_size];
        in_worklist[b_idx] = false;
        BasicBlock *block = cfg->blocks[b_idx];

        BlockState new_in;
        for (int r = 0; r < 16; r++) new_in.regs[r] = (RegState){VAL_TOP, 0};

        for (int p = 0; p < block->num_preds; p++) {
            BasicBlock *pred = block->preds[p];
            for (int r = 0; r < 16; r++) {
                new_in.regs[r] = merge_reg(new_in.regs[r], pred->out_state.regs[r]);
            }
        }
        block->in_state = new_in;

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

int fold_constants_cfg(ControlFlowGraph *cfg) {
    int optimizations = 0;
    if (!cfg) return 0;

    for (int i = 0; i < cfg->num_blocks; i++) {
        BasicBlock *block = cfg->blocks[i];
        BlockState current = block->in_state;

        for (AsmNode *node = block->first_ins; node != NULL; node = node->next) {
            // Fold constant register inputs: MOV R_dst, R_src -> MOV R_dst, <constant>
            if (node->type == OP_MOV && node->src_op.mode == MODE_REG) {
                int src_reg = get_reg_index(node->src_op.reg);
                if (src_reg >= 0 && current.regs[src_reg].type == VAL_CONST) {
                    int const_val = current.regs[src_reg].val;
                    node->src_op.mode = MODE_IMMEDIATE;
                    node->src_op.immediate = const_val;
                    snprintf(node->src_op.raw, sizeof(node->src_op.raw), "%d", const_val);
                    snprintf(node->raw, sizeof(node->raw), "    MOV %s, %d", node->dst_op.reg, const_val);
                    optimizations++;
                }
            }

            // Advance state tracking through current block
            if (node->type == OP_MOV) {
                int dst_reg = get_reg_index(node->dst_op.reg);
                if (dst_reg >= 0) {
                    if (node->src_op.mode == MODE_IMMEDIATE) {
                        current.regs[dst_reg] = (RegState){VAL_CONST, node->src_op.immediate};
                    } else if (node->src_op.mode == MODE_REG) {
                        int src_reg = get_reg_index(node->src_op.reg);
                        current.regs[dst_reg] = (src_reg >= 0) ? current.regs[src_reg] : (RegState){VAL_BOTTOM, 0};
                    } else {
                        current.regs[dst_reg] = (RegState){VAL_BOTTOM, 0};
                    }
                }
            }

            if (node == block->last_ins) break;
        }
    }

    return optimizations;
}

// -------------------------------------------------------------------
// Main Entry Point
// -------------------------------------------------------------------

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: %s <input.asm> <output.asm> [--dot cfg.dot]\n", argv[0]);
        return 1;
    }

    AsmNode *program_ast = parse_vircon32_asm(argv[1]);

    int passes = 0;
    int total_opts = 0;
    int opts_in_pass = 0;

    // Phase 1: Iterative Peephole & Local Optimization Engine
    do {
        opts_in_pass = 0;
        opts_in_pass += pass_peephole_window2(program_ast);
        opts_in_pass += pass_algebraic_simplifications(program_ast);
        opts_in_pass += pass_store_to_load_forwarding(program_ast);

        total_opts += opts_in_pass;
        passes++;
    } while (opts_in_pass > 0);

    // Phase 2: Build CFG & Perform Global Data-Flow Analysis
    ControlFlowGraph *cfg = build_cfg(program_ast);
    propagate_constants_cfg(cfg);
    
    int global_folds = fold_constants_cfg(cfg);
    total_opts += global_folds;

    // Optional DOT Graphviz export
    if (argc >= 5 && strcmp(argv[3], "--dot") == 0) {
        export_cfg_to_dot(argv[4], cfg);
        printf("CFG exported to '%s'.\n", argv[4]);
    }

    printf("Optimization complete: %d total optimizations (%d global folds) in %d passes.\n", 
           total_opts, global_folds, passes);

    write_vircon32_asm(argv[2], program_ast);

    free_cfg(cfg);
    free(program_ast);
    return 0;
}
