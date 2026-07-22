#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

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

// -------------------------------------------------------------------
// String & Parsing Helpers
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

// Memory & Operand Parser
Operand parse_operand(const char *str) {
    Operand op;
    memset(&op, 0, sizeof(Operand));
    if (!str || strlen(str) == 0) return op;

    strncpy(op.raw, str, sizeof(op.raw) - 1);

    // Indirect / Memory addressing: [REG+OFF], [REG-OFF], [REG]
    if (str[0] == '[' && str[strlen(str) - 1] == ']') {
        op.mode = MODE_INDIRECT;

        char inner[32] = {0};
        strncpy(inner, str + 1, strlen(str) - 2); // Strip '[' and ']'

        char *plus_ptr  = strchr(inner, '+');
        char *minus_ptr = strrchr(inner, '-'); // strrchr handles negative offsets

        if (plus_ptr) {
            *plus_ptr = '\0';
            strncpy(op.reg, trim(inner), sizeof(op.reg) - 1);
            op.offset = atoi(trim(plus_ptr + 1));
        } else if (minus_ptr && minus_ptr != inner) { // Ensure '-' isn't a leading sign
            *minus_ptr = '\0';
            strncpy(op.reg, trim(inner), sizeof(op.reg) - 1);
            op.offset = -atoi(trim(minus_ptr + 1));
        } else {
            strncpy(op.reg, trim(inner), sizeof(op.reg) - 1);
            op.offset = 0; // Pure dereference like [R0]
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
// PASS 1: Window Size 2 Peephole Optimizations
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

// -------------------------------------------------------------------
// PASS 2: Single Instruction Simplifications (Algebraic)
// -------------------------------------------------------------------
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
            curr->src_op = curr->dst_op; // Set SRC operand to match DST register
            snprintf(curr->raw, sizeof(curr->raw), "    IADD %s, %s", curr->dst_op.raw, curr->src_op.raw);
            optimizations++;
        }

        curr = next;
    }

    return optimizations;
}

// -------------------------------------------------------------------
// PASS 3: Store-to-Load Forwarding (Memory Offset Optimization)
// -------------------------------------------------------------------
int pass_store_to_load_forwarding(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr && curr->next) {
        AsmNode *n1 = curr;
        AsmNode *n2 = curr->next;

        // Match Pattern:
        // MOV [REG+OFF], R_src
        // MOV R_dst, [REG+OFF]
        if (n1->type == OP_MOV && n2->type == OP_MOV &&
            n1->dst_op.mode == MODE_INDIRECT && n1->src_op.mode == MODE_REG &&
            n2->dst_op.mode == MODE_REG      && n2->src_op.mode == MODE_INDIRECT) 
        {
            // Verify base registers and numerical offsets match
            if (strcmp(n1->dst_op.reg, n2->src_op.reg) == 0 && 
                n1->dst_op.offset == n2->src_op.offset) 
            {
                // Forward register: Rewrite n2 to "MOV R_dst, R_src"
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
// Main Entry Point
// -------------------------------------------------------------------
int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: %s <input.asm> <output.asm>\n", argv[0]);
        return 1;
    }

    AsmNode *program_ast = parse_vircon32_asm(argv[1]);

    int passes = 0;
    int total_opts = 0;
    int opts_in_pass = 0;

    // Run passes until convergence
    do {
        opts_in_pass = 0;
        opts_in_pass += pass_peephole_window2(program_ast);
        opts_in_pass += pass_algebraic_simplifications(program_ast);
        opts_in_pass += pass_store_to_load_forwarding(program_ast);

        total_opts += opts_in_pass;
        passes++;
    } while (opts_in_pass > 0);

    printf("Optimization complete: %d instructions optimized in %d passes.\n", total_opts, passes);
    write_vircon32_asm(argv[2], program_ast);

    free(program_ast);
    return 0;
}
