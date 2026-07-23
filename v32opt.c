#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#define MAX_INLINE_CANDIDATES 64
#define MAX_BODY_INS 8
#define MAX_FUNCTIONS 256

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
    char reg[32];     
    int offset;       
    int immediate;    
    char raw[128];    
} Operand;

typedef struct AsmNode {
    OpType type;
    char raw[512];      
    char mnemonic[32];  
    
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
    char labels[8][128]; 
    int num_labels;

    AsmNode *first_ins;
    AsmNode *last_ins;

    BasicBlock **preds;
    int num_preds;
    int cap_preds;

    BasicBlock **succs;
    int num_succs;
    int cap_succs;

    BlockState in_state;   
    BlockState out_state;  
};

typedef struct {
    BasicBlock **blocks;
    int num_blocks;
    int cap_blocks;
} ControlFlowGraph;

typedef struct {
    char name[128]; 
    AsmNode *body_nodes[MAX_BODY_INS];
    int body_count;
} InlineCandidate;

typedef struct {
    char name[128]; 
    AsmNode *start_node; 
    AsmNode *end_node;   
    bool reachable;
} FunctionDef;

typedef struct {
    bool verbose;
    bool enable_peephole;
    bool enable_algebraic;
    bool enable_forwarding;
    bool enable_inline;
    bool enable_dce;
    bool enable_constant_folding;
} OptConfig;

// -------------------------------------------------------------------
// String Parsing & AST Utilities
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

    snprintf(op.raw, sizeof(op.raw), "%s", str);

    if (str[0] == '[' && str[strlen(str) - 1] == ']') {
        op.mode = MODE_INDIRECT;
        char inner[128] = {0}; 
        snprintf(inner, sizeof(inner), "%.*s", (int)(strlen(str) - 2), str + 1);

        char *plus_ptr  = strchr(inner, '+');
        char *minus_ptr = strrchr(inner, '-');

        if (plus_ptr) {
            *plus_ptr = '\0';
            snprintf(op.reg, sizeof(op.reg), "%s", trim(inner));
            op.offset = atoi(trim(plus_ptr + 1));
        } else if (minus_ptr && minus_ptr != inner) {
            *minus_ptr = '\0';
            snprintf(op.reg, sizeof(op.reg), "%s", trim(inner));
            op.offset = -atoi(trim(minus_ptr + 1));
        } else {
            snprintf(op.reg, sizeof(op.reg), "%s", trim(inner));
            op.offset = 0;
        }
    } 
    else if (isdigit((unsigned char)str[0]) || (str[0] == '-' && isdigit((unsigned char)str[1]))) {
        op.mode = MODE_IMMEDIATE;
        op.immediate = atoi(str);
    } 
    else {
        op.mode = MODE_REG;
        snprintf(op.reg, sizeof(op.reg), "%s", str);
    }

    return op;
}

AsmNode* create_node(const char* raw, OpType type, const char* mnem, const char* dst, const char* src) {
    AsmNode *node = (AsmNode*)calloc(1, sizeof(AsmNode));
    if (raw) snprintf(node->raw, sizeof(node->raw), "%s", raw);
    node->type = type;
    if (mnem) snprintf(node->mnemonic, sizeof(node->mnemonic), "%s", mnem);
    
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

static AsmNode* clone_node(AsmNode *src) {
    AsmNode *dst = (AsmNode*)calloc(1, sizeof(AsmNode));
    memcpy(dst, src, sizeof(AsmNode));
    dst->prev = NULL;
    dst->next = NULL;
    return dst;
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

    char line[512]; 
    while (fgets(line, sizeof(line), fp)) {
        char raw[512]; 
        snprintf(raw, sizeof(raw), "%s", line);
        raw[strcspn(raw, "\r\n")] = '\0';

        char *trimmed = trim(line);

        if (strlen(trimmed) == 0 || trimmed[0] == ';') {
            AsmNode *node = create_node(raw, OP_OTHER, NULL, NULL, NULL);
            tail->next = node; node->prev = tail; tail = node;
            continue;
        }

        char code_part[256] = {0}; 
        char *comment_ptr = strchr(trimmed, ';');
        if (comment_ptr) {
            size_t len = comment_ptr - trimmed;
            snprintf(code_part, len + 1, "%s", trimmed);
        } else {
            snprintf(code_part, sizeof(code_part), "%s", trimmed);
        }
        char *code_trimmed = trim(code_part);

        if (code_trimmed[strlen(code_trimmed) - 1] == ':') {
            AsmNode *node = create_node(raw, OP_LABEL, NULL, NULL, NULL);
            tail->next = node; node->prev = tail; tail = node;
            continue;
        }

        char mnem[32] = {0}, dst[128] = {0}, src[128] = {0}; 
        char *space_ptr = strpbrk(code_trimmed, " \t");

        if (!space_ptr) {
            snprintf(mnem, sizeof(mnem), "%s", code_trimmed);
        } else {
            size_t mnem_len = space_ptr - code_trimmed;
            if (mnem_len >= sizeof(mnem)) mnem_len = sizeof(mnem) - 1;
            snprintf(mnem, mnem_len + 1, "%s", code_trimmed);

            char *operands = trim(space_ptr);
            char *comma_ptr = strchr(operands, ',');
            if (comma_ptr) {
                *comma_ptr = '\0';
                snprintf(dst, sizeof(dst), "%s", trim(operands));
                snprintf(src, sizeof(src), "%s", trim(comma_ptr + 1));
            } else {
                snprintf(dst, sizeof(dst), "%s", trim(operands));
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
    bool last_was_blank = false;

    while (curr) {
        char line_copy[512]; 
        snprintf(line_copy, sizeof(line_copy), "%s", curr->raw);
        char *trimmed = trim(line_copy);

        bool is_blank = (strlen(trimmed) == 0);

        if (is_blank) {
            if (!last_was_blank) {
                fprintf(fp, "\n");
                last_was_blank = true;
            }
        } else {
            fprintf(fp, "%s\n", curr->raw);
            last_was_blank = false;
        }

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

        if ((n1->type == OP_IEQ || n1->type == OP_INE) && 
             n2->type == OP_CIB && 
             n1->dst_op.mode == MODE_REG && n2->dst_op.mode == MODE_REG &&
             strcmp(n1->dst_op.reg, n2->dst_op.reg) == 0) 
        {
            remove_node(n2);
            optimizations++;
            continue;
        }

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

        if (curr->type == OP_MOV && 
            curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_REG && 
            strcmp(curr->dst_op.reg, curr->src_op.reg) == 0) 
        {
            remove_node(curr);
            optimizations++;
            curr = next;
            continue;
        }

        if ((curr->type == OP_IADD || curr->type == OP_ISUB) && 
            curr->src_op.mode == MODE_IMMEDIATE && curr->src_op.immediate == 0) 
        {
            remove_node(curr);
            optimizations++;
            curr = next;
            continue;
        }

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
// Pass: Trivial Leaf Function Inlining
// -------------------------------------------------------------------
int pass_inline_trivial_functions(AsmNode *head) {
    InlineCandidate candidates[MAX_INLINE_CANDIDATES];
    int candidate_count = 0;

    AsmNode *curr = head ? head->next : NULL;

    while (curr) {
        if (curr->type == OP_LABEL && candidate_count < MAX_INLINE_CANDIDATES) {
            char func_name[128] = {0}; 
            char line_copy[512];
            snprintf(line_copy, sizeof(line_copy), "%s", curr->raw);
            char *trimmed_lbl = trim(line_copy);

            snprintf(func_name, sizeof(func_name), "%s", trimmed_lbl);
            char *colon = strchr(func_name, ':');
            if (colon) *colon = '\0';
            trim(func_name);

            AsmNode *scan = curr->next;
            while (scan && (scan->type == OP_OTHER || scan->type == OP_LABEL)) scan = scan->next;

            if (scan && scan->type == OP_PUSH && str_case_eq(scan->dst_op.reg, "BP")) {
                scan = scan->next;
                if (scan && scan->type == OP_MOV && str_case_eq(scan->dst_op.reg, "BP") && str_case_eq(scan->src_op.reg, "SP")) {
                    scan = scan->next;
                }
            }

            AsmNode *core_nodes[MAX_BODY_INS];
            int core_count = 0;
            bool valid_candidate = true;

            while (scan) {
                if (scan->type == OP_LABEL) {
                    scan = scan->next;
                    continue;
                }

                if (scan->type == OP_MOV && str_case_eq(scan->dst_op.reg, "SP") && str_case_eq(scan->src_op.reg, "BP")) break;
                if (str_case_eq(scan->mnemonic, "RET")) break;

                if (str_case_eq(scan->mnemonic, "CALL") || str_case_eq(scan->mnemonic, "JMP") ||
                    str_case_eq(scan->mnemonic, "JT")   || str_case_eq(scan->mnemonic, "JF")) {
                    valid_candidate = false;
                    break;
                }

                if (core_count < MAX_BODY_INS) {
                    core_nodes[core_count++] = scan;
                } else {
                    valid_candidate = false;
                    break;
                }

                scan = scan->next;
            }

            if (valid_candidate && core_count > 0 && core_count <= MAX_BODY_INS) {
                snprintf(candidates[candidate_count].name, sizeof(candidates[candidate_count].name), "%s", func_name);
                candidates[candidate_count].body_count = core_count;
                for (int i = 0; i < core_count; i++) {
                    candidates[candidate_count].body_nodes[i] = core_nodes[i];
                }
                candidate_count++;
            }
        }
        curr = curr->next;
    }

    int inlined_calls = 0;
    curr = head ? head->next : NULL;

    while (curr) {
        AsmNode *next_node = curr->next;

        if (str_case_eq(curr->mnemonic, "CALL")) {
            char target_label[128] = {0}; 
            snprintf(target_label, sizeof(target_label), "%s", trim(curr->dst_op.raw));

            for (int c = 0; c < candidate_count; c++) {
                if (str_case_eq(target_label, candidates[c].name)) {
                    AsmNode *insertion_point = curr->prev;

                    for (int b = 0; b < candidates[c].body_count; b++) {
                        AsmNode *inlined_ins = clone_node(candidates[c].body_nodes[b]);

                        inlined_ins->prev = insertion_point;
                        inlined_ins->next = curr;
                        if (insertion_point) insertion_point->next = inlined_ins;
                        curr->prev = inlined_ins;

                        insertion_point = inlined_ins;
                    }

                    remove_node(curr);
                    inlined_calls++;
                    break;
                }
            }
        }

        curr = next_node;
    }

    return inlined_calls;
}

// -------------------------------------------------------------------
// Pass: Reachability-Based Dead Function Elimination (DFE)
// -------------------------------------------------------------------
// -------------------------------------------------------------------
// Pass: Reachability-Based Dead Function Elimination (DFE)
// -------------------------------------------------------------------
int pass_dead_function_elimination(AsmNode *head) {
    FunctionDef funcs[MAX_FUNCTIONS];
    int func_count = 0;

    AsmNode *curr = head ? head->next : NULL;

    // ----------------------------------------------------------------
    // 1. Capture Unlabeled Preamble (Boot Vector)
    // ----------------------------------------------------------------
    AsmNode *preamble_start = curr;
    // Skip leading blank lines or full-line comments
    while (preamble_start && preamble_start->type == OP_OTHER && 
          (preamble_start->raw[0] == '\0' || preamble_start->raw[0] == ';')) {
        preamble_start = preamble_start->next;
    }

    // If real instructions exist before the first label, bundle them into "__boot_vector"
    if (preamble_start && preamble_start->type != OP_LABEL) {
        AsmNode *preamble_end = preamble_start;
        while (preamble_end->next && preamble_end->next->type != OP_LABEL) {
            preamble_end = preamble_end->next;
        }

        if (func_count < MAX_FUNCTIONS) {
            snprintf(funcs[func_count].name, sizeof(funcs[func_count].name), "__boot_vector");
            funcs[func_count].start_node = preamble_start;
            funcs[func_count].end_node = preamble_end;
            funcs[func_count].reachable = false;
            func_count++;
        }
    }

    // ----------------------------------------------------------------
    // 2. Discover Standard Function Boundaries
    // ----------------------------------------------------------------
    while (curr) {
        if (curr->type == OP_LABEL) {
            char line_copy[512]; 
            snprintf(line_copy, sizeof(line_copy), "%s", curr->raw);
            char *lbl = trim(line_copy);

            // Ignore local labels and compiler metadata
            if (lbl[0] == '.' || lbl[0] == '@' || strstr(lbl, "_return:")) {
                curr = curr->next;
                continue;
            }

            char func_name[128] = {0}; 
            snprintf(func_name, sizeof(func_name), "%s", lbl);
            char *colon = strchr(func_name, ':');
            if (colon) *colon = '\0';
            trim(func_name);

            AsmNode *scan = curr->next;
            AsmNode *ret_node = NULL;

            while (scan) {
                if (str_case_eq(scan->mnemonic, "RET") || str_case_eq(scan->mnemonic, "RETI")) {
                    ret_node = scan;
                    break;
                }
                scan = scan->next;
            }

            if (ret_node && func_count < MAX_FUNCTIONS) {
                snprintf(funcs[func_count].name, sizeof(funcs[func_count].name), "%s", func_name);
                funcs[func_count].start_node = curr;
                funcs[func_count].end_node = ret_node;
                funcs[func_count].reachable = false;
                func_count++;

                curr = ret_node->next;
                continue;
            }
        }
        curr = curr->next;
    }

    if (func_count == 0) return 0;

    // ----------------------------------------------------------------
    // 3. Seed Reachability Worklist
    // ----------------------------------------------------------------
    char worklist[MAX_FUNCTIONS][128]; 
    int worklist_size = 0;

    for (int i = 0; i < func_count; i++) {
        if (str_case_eq(funcs[i].name, "__boot_vector")                || // <-- Seed preamble
            str_case_eq(funcs[i].name, "__function_main")              ||
            str_case_eq(funcs[i].name, "main")                         ||
            str_case_eq(funcs[i].name, "_start")                       ||
            str_case_eq(funcs[i].name, "start")                        ||
            str_case_eq(funcs[i].name, "__start")                      ||
            str_case_eq(funcs[i].name, "__init_globals")               ||
            str_case_eq(funcs[i].name, "__function_init")              ||
            str_case_eq(funcs[i].name, "__global_scope_initialization")||
            // NOTE: Blanket "__builtin_" catch-all removed so unused library code is stripped!
            strstr(funcs[i].name, "global_scope") != NULL              ||
            strstr(funcs[i].name, "ISR")          != NULL              ||
            strstr(funcs[i].name, "interrupt")    != NULL)
        {
            funcs[i].reachable = true;
            if (worklist_size < MAX_FUNCTIONS) {
                snprintf(worklist[worklist_size++], sizeof(worklist[0]), "%s", funcs[i].name);
            }
        }
    }

    if (worklist_size == 0 && func_count > 0) {
        funcs[0].reachable = true;
        snprintf(worklist[worklist_size++], sizeof(worklist[0]), "%s", funcs[0].name);
    }

    // ----------------------------------------------------------------
    // 4. Global Scan for `pointer` Directives
    // ----------------------------------------------------------------
    for (AsmNode *node = head; node != NULL; node = node->next) {
        if (str_case_eq(node->mnemonic, "pointer")) {
            char args_copy[512];
            snprintf(args_copy, sizeof(args_copy), "%s", node->raw);
            
            char *token = strtok(args_copy, " ,\t\n\r");
            if (token && str_case_eq(token, "pointer")) {
                token = strtok(NULL, " ,\t\n\r");
                while (token != NULL) {
                    for (int f = 0; f < func_count; f++) {
                        if (str_case_eq(funcs[f].name, token) && !funcs[f].reachable) {
                            funcs[f].reachable = true;
                            if (worklist_size < MAX_FUNCTIONS) {
                                snprintf(worklist[worklist_size++], sizeof(worklist[0]), "%s", funcs[f].name);
                            }
                        }
                    }
                    token = strtok(NULL, " ,\t\n\r");
                }
            }
        }
    }

    // ----------------------------------------------------------------
    // 5. Worklist Traversal (Catch Calls, Branches & Function Pointers)
    // ----------------------------------------------------------------
    while (worklist_size > 0) {
        char current_label[128];
        snprintf(current_label, sizeof(current_label), "%s", worklist[--worklist_size]);

        FunctionDef *fn = NULL;
        for (int i = 0; i < func_count; i++) {
            if (str_case_eq(funcs[i].name, current_label)) {
                fn = &funcs[i];
                break;
            }
        }
        if (!fn) continue;

        for (AsmNode *node = fn->start_node; node != NULL; node = node->next) {
            // Ignore labels and comments during operand evaluation
            if (node->type == OP_LABEL || (node->type == OP_OTHER && node->raw[0] == ';')) {
                if (node == fn->end_node) break;
                continue;
            }

            char *op_dst = trim(node->dst_op.raw);
            char *op_src = trim(node->src_op.raw);

            // Check if EITHER operand references a known function name
            // This catches direct CALL/JMP targets AND function pointer assignments (e.g., MOV R1, my_func)
            for (int f = 0; f < func_count; f++) {
                if (funcs[f].reachable) continue; // Already processed

                if ((strlen(op_dst) > 0 && str_case_eq(funcs[f].name, op_dst)) ||
                    (strlen(op_src) > 0 && str_case_eq(funcs[f].name, op_src))) 
                {
                    funcs[f].reachable = true;
                    if (worklist_size < MAX_FUNCTIONS) {
                        snprintf(worklist[worklist_size++], sizeof(worklist[0]), "%s", funcs[f].name);
                    }
                }
            }

            if (node == fn->end_node) break;
        }
    }

    // ----------------------------------------------------------------
    // 6. Sweep Unreachable Functions
    // ----------------------------------------------------------------
    int eliminated_funcs = 0;
    for (int i = 0; i < func_count; i++) {
        if (!funcs[i].reachable) {
            AsmNode *sweep_curr = funcs[i].start_node;
            AsmNode *sweep_end  = funcs[i].end_node;

            while (sweep_curr) {
                AsmNode *next = sweep_curr->next;
                bool is_last = (sweep_curr == sweep_end);
                remove_node(sweep_curr);
                if (is_last) break;
                sweep_curr = next;
            }
            eliminated_funcs++;
        }
    }

    return eliminated_funcs;
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
    char pending_labels[8][128]; 
    int pending_label_count = 0;

    AsmNode *curr = (head->type == OP_OTHER && head->raw[0] == '\0') ? head->next : head;

    while (curr) {
        if (curr->type == OP_LABEL) {
            char lbl[128] = {0}; 
            snprintf(lbl, sizeof(lbl), "%s", curr->raw);
            char *colon = strchr(lbl, ':');
            if (colon) *colon = '\0';

            if (pending_label_count < 8) {
                snprintf(pending_labels[pending_label_count++], sizeof(pending_labels[0]), "%s", lbl);
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
                snprintf(current_block->labels[l], sizeof(current_block->labels[l]), "%s", pending_labels[l]);
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
            // End of execution path
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
            
            if (node->type == OP_MOV && node->src_op.mode == MODE_REG && node->dst_op.mode == MODE_REG) {
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
    if (argc < 2) {
        fprintf (stdout, "Usage: %s <input.asm> [output.asm] [options]\n", argv[0]);
        fprintf (stdout, "Options:\n");
        fprintf (stdout, "  -v                      Verbose output (show pass statistics)\n");
        fprintf (stdout, "  --dot <cfg.dot>         Export Control Flow Graph to DOT format\n");
        fprintf (stdout, "  -O0                     Disable all optimizations [default]\n");
        fprintf (stdout, "  -O1                     Enables: peephole, algebraic, forwarding\n");
        fprintf (stdout, "  -O2                     -O1 + DCE, constant folding\n");
        fprintf (stdout, "  -O3                     -O2 + function inlining (aggressive)\n");
        fprintf (stdout, "  -fopt_peephole          Enables peephole optimizations\n");
        fprintf (stdout, "  -fopt_algebraic         Enables algebraic simplifications\n");
        fprintf (stdout, "  -fopt_forwarding        Enables store-to-load forwarding\n");
        fprintf (stdout, "  -fopt_inline            Enables function inlining\n");
        fprintf (stdout, "  -fopt_dce               Enables dead function elimination\n");
        fprintf (stdout, "  -fopt_constant_folding  Enables constant folding\n\n");
        return (1);
    }

    char *inFile = argv[1];
    char outFile[256] = {0};
    char dotFile[256] = {0};

    // Default config is -O0
    OptConfig config = {
        .verbose = false,
        .enable_peephole = false,
        .enable_algebraic = false,
        .enable_forwarding = false,
        .enable_inline = false,
        .enable_dce = false,
        .enable_constant_folding = false
    };

    int  out_idx = 0;
	int  opt_count  = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--dot") == 0 && i + 1 < argc) {
            snprintf(dotFile, sizeof(dotFile), "%s", argv[i+1]);
            i++;
        } else if (strcmp(argv[i], "-v") == 0) {
            config.verbose = true;
        } else if (strcmp(argv[i], "-O0") == 0) {
            config.enable_peephole = false;
            config.enable_algebraic = false;
            config.enable_forwarding = false;
            config.enable_inline = false;
            config.enable_dce = false;
            config.enable_constant_folding = false;
        } else if (strcmp(argv[i], "-O1") == 0) {
            config.enable_peephole = true;
            config.enable_algebraic = true;
            config.enable_forwarding = true;
            config.enable_inline = false;
            config.enable_dce = false;
            config.enable_constant_folding = false;
			opt_count = 3;
        } else if (strcmp(argv[i], "-O2") == 0) {
            config.enable_peephole = true;
            config.enable_algebraic = true;
            config.enable_forwarding = true;
            config.enable_inline = false;
            config.enable_dce = true;
            config.enable_constant_folding = true;
			opt_count = 5;
        } else if (strcmp(argv[i], "-O3") == 0) {
            config.enable_peephole = true;
            config.enable_algebraic = true;
            config.enable_forwarding = true;
            config.enable_inline = true;
            config.enable_dce = true;
            config.enable_constant_folding = true;
			opt_count = 6;
        } else if (strcmp(argv[i], "-fopt_peephole") == 0) {
            config.enable_peephole = true;
			opt_count = opt_count + 1;
        } else if (strcmp(argv[i], "-fopt_algebraic") == 0) {
            config.enable_algebraic = true;
			opt_count = opt_count + 1;
        } else if (strcmp(argv[i], "-fopt_forwarding") == 0) {
            config.enable_forwarding = true;
			opt_count = opt_count + 1;
        } else if (strcmp(argv[i], "-fopt_inline") == 0) {
            config.enable_inline = true;
			opt_count = opt_count + 1;
        } else if (strcmp(argv[i], "-fopt_dce") == 0) {
            config.enable_dce = true;
			opt_count = opt_count + 1;
        } else if (strcmp(argv[i], "-fopt_constant_folding") == 0) {
            config.enable_constant_folding = true;
			opt_count = opt_count + 1;
        } else if (out_idx == 0 && argv[i][0] != '-') {
            snprintf(outFile, sizeof(outFile), "%s", argv[i]);
            out_idx = i;
        }
    }

    if (config.verbose)
    {

        if (opt_count  == 0)
        {
            fprintf (stdout, "--- Configuration: NO OPTIMIZATIONS ENABLED ---\n");
        }
        else
        {
			fprintf (stdout, "--- Configuration: Enabled Optimizations ---\n");
        }

        if (config.enable_peephole)
        {
            fprintf (stdout, "  - peephole\n");
        }
        if (config.enable_algebraic)
        {
            fprintf (stdout, "  - algebraic\n");
        }
        if (config.enable_forwarding)
        {
            fprintf (stdout, "  - forwarding\n");
        }
        if (config.enable_inline)
        {
            fprintf (stdout, "  - inline\n");
        }
        if (config.enable_dce)
        {
            fprintf (stdout, "  - dce\n");
        }
        if (config.enable_constant_folding)
        {
            fprintf (stdout, "  - constant_folding\n");
        }
    }

    if (strlen(outFile) == 0) {
        snprintf(outFile, sizeof(outFile), "%s", inFile);
        char *ext = strrchr(outFile, '.');
        if (ext && strcmp(ext, ".asm") == 0) {
            *ext = '\0';
        }
        snprintf(outFile + strlen(outFile), sizeof(outFile) - strlen(outFile), "Opt.asm");
    }

    AsmNode *program_ast = parse_vircon32_asm(inFile);

    int passes = 0;
    int total_opts = 0;
    int opts_in_pass = 0;

    if (config.verbose) printf("--- Starting Optimization Phase 1: Local Passes ---\n");

    // Phase 1: Iterative Peephole, Inlining & Local Optimizations
    do {
        opts_in_pass = 0;
        int p_opts = 0, a_opts = 0, f_opts = 0, i_opts = 0, d_opts = 0;

        if (config.enable_peephole)   p_opts = pass_peephole_window2(program_ast);
        if (config.enable_algebraic)  a_opts = pass_algebraic_simplifications(program_ast);
        if (config.enable_forwarding) f_opts = pass_store_to_load_forwarding(program_ast);
        if (config.enable_inline)     i_opts = pass_inline_trivial_functions(program_ast);
        if (config.enable_dce)        d_opts = pass_dead_function_elimination(program_ast);

        opts_in_pass = p_opts + a_opts + f_opts + i_opts + d_opts;
        total_opts += opts_in_pass;
        passes++;

        if (config.verbose && opts_in_pass > 0) {
            printf("Pass %d applied %d optimizations:\n", passes, opts_in_pass);
            if (p_opts > 0) printf("  - Peephole: %d\n", p_opts);
            if (a_opts > 0) printf("  - Algebraic: %d\n", a_opts);
            if (f_opts > 0) printf("  - Forwarding: %d\n", f_opts);
            if (i_opts > 0) printf("  - Inlined funcs: %d\n", i_opts);
            if (d_opts > 0) printf("  - Dead funcs removed: %d\n", d_opts);
        }
    } while (opts_in_pass > 0);

    // Phase 2: Build CFG & Perform Global Data-Flow Analysis
    int global_folds = 0;
    ControlFlowGraph *cfg = NULL;

    if (config.enable_constant_folding) {
        if (config.verbose) printf("--- Starting Optimization Phase 2: Global Data-Flow ---\n");
        cfg = build_cfg(program_ast);
        propagate_constants_cfg(cfg);

        global_folds = fold_constants_cfg(cfg);
        total_opts += global_folds;

        if (config.verbose && global_folds > 0) {
            printf("  - Global constant folds: %d\n", global_folds);
        }
    }

    if (strlen(dotFile) > 0) {
        if (!cfg) cfg = build_cfg(program_ast); // Ensure CFG exists for export if requested
        export_cfg_to_dot(dotFile, cfg);
        if (config.verbose) printf("CFG exported to '%s'.\n", dotFile);
    }

    //if (config.verbose || total_opts > 0) {
    if (config.verbose) {
        printf("\nOptimization complete: %d total optimizations applied.\n", total_opts);
    }

    write_vircon32_asm(outFile, program_ast);

    if (cfg) free_cfg(cfg);
    free(program_ast);
    return 0;
}
