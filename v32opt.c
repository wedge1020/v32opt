#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

typedef enum {
    OP_MOV, OP_IADD, OP_ISUB, OP_IMUL, OP_IEQ, OP_INE, 
    OP_CIB, OP_PUSH, OP_POP, OP_BNOT, OP_OTHER, OP_LABEL
} OpType;

typedef struct AsmNode {
    OpType type;
    char raw[128];
    
    char mnemonic[16];
    char dst[16];
    char src[16];
    bool has_src;

    struct AsmNode *prev;
    struct AsmNode *next;
} AsmNode;

// Helper: Trim leading and trailing whitespace
static char* trim(char *str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

// Helper: Case-insensitive string comparison
static bool str_case_eq(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        if (toupper((unsigned char)*s1) != toupper((unsigned char)*s2)) return false;
        s1++; s2++;
    }
    return *s1 == *s2;
}

// Helper: Construct linked list nodes
AsmNode* create_node(const char* raw, OpType type, const char* mnem, const char* dst, const char* src) {
    AsmNode *node = (AsmNode*)calloc(1, sizeof(AsmNode));
    if (raw) strncpy(node->raw, raw, sizeof(node->raw) - 1);
    node->type = type;
    if (mnem) strncpy(node->mnemonic, mnem, sizeof(node->mnemonic) - 1);
    if (dst)  strncpy(node->dst, dst, sizeof(node->dst) - 1);
    if (src) {
        strncpy(node->src, src, sizeof(node->src) - 1);
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

    // Use a dummy head node to simplify node deletions
    AsmNode *dummy_head = create_node(NULL, OP_OTHER, NULL, NULL, NULL);
    AsmNode *tail = dummy_head;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char raw[128];
        strncpy(raw, line, sizeof(raw) - 1);
        raw[sizeof(raw) - 1] = '\0';
        
        // Strip trailing newline characters
        raw[strcspn(raw, "\r\n")] = '\0';

        char *trimmed = trim(line);

        // Preserve empty lines and comment lines in raw output
        if (strlen(trimmed) == 0 || trimmed[0] == ';') {
            AsmNode *node = create_node(raw, OP_OTHER, NULL, NULL, NULL);
            tail->next = node; node->prev = tail; tail = node;
            continue;
        }

        // Separate inline comments from code
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

        // Check for Label (ends with ':')
        if (code_trimmed[strlen(code_trimmed) - 1] == ':') {
            AsmNode *node = create_node(raw, OP_LABEL, NULL, NULL, NULL);
            tail->next = node; node->prev = tail; tail = node;
            continue;
        }

        // Instruction Parsing (MNEMONIC DST, SRC)
        char mnem[16] = {0}, dst[32] = {0}, src[32] = {0};
        char *space_ptr = strpbrk(code_trimmed, " \t");

        if (!space_ptr) {
            // 0-operand instruction (RET, WAIT, HLT, MOVS, etc.)
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

        // Map Mnemonic to OpType
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

        AsmNode *node = create_node(raw, type, mnem, dst, strlen(src) > 0 ? src : NULL);
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

    // Skip dummy head node
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
    AsmNode *curr = head ? head->next : NULL; // Start after dummy head

    while (curr && curr->next) {
        AsmNode *n1 = curr;
        AsmNode *n2 = curr->next;

        // Pattern A: IEQ / INE followed by CIB on same register
        if ((n1->type == OP_IEQ || n1->type == OP_INE) && 
             n2->type == OP_CIB && 
             strcmp(n1->dst, n2->dst) == 0) 
        {
            remove_node(n2);
            optimizations++;
            continue;
        }

        // Pattern B: Redundant Double Inversion (BNOT R0 -> BNOT R0)
        if (n1->type == OP_BNOT && n2->type == OP_BNOT && 
            strcmp(n1->dst, n2->dst) == 0) 
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
            strcmp(n1->dst, n2->dst) == 0) 
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
        if (curr->type == OP_MOV && curr->has_src && strcmp(curr->dst, curr->src) == 0) {
            remove_node(curr);
            optimizations++;
            curr = next;
            continue;
        }

        // Addition/Subtraction by Zero: IADD R0, 0
        if ((curr->type == OP_IADD || curr->type == OP_ISUB) && 
            curr->has_src && strcmp(curr->src, "0") == 0) 
        {
            remove_node(curr);
            optimizations++;
            curr = next;
            continue;
        }

        // Strength Reduction: IMUL R0, 2  -->  IADD R0, R0
        if (curr->type == OP_IMUL && curr->has_src && strcmp(curr->src, "2") == 0) {
            curr->type = OP_IADD;
            strcpy(curr->mnemonic, "IADD");
            strcpy(curr->src, curr->dst);
            snprintf(curr->raw, sizeof(curr->raw), "    IADD %s, %s", curr->dst, curr->src);
            optimizations++;
        }

        curr = next;
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

        total_opts += opts_in_pass;
        passes++;
    } while (opts_in_pass > 0);

    printf("Optimization complete: %d instructions optimized in %d passes.\n", total_opts, passes);
    write_vircon32_asm(argv[2], program_ast);

    // Clean up dummy head
    free(program_ast);
    return 0;
}
