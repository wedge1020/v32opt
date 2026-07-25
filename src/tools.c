#include "v32opt.h"

// -------------------------------------------------------------------
// String Parsing & AST Utilities
// -------------------------------------------------------------------

// Wrapper function using strncpy at its core to safely copy strings
// without generating -Wformat-truncation or -Wstringop-truncation warnings.
// Safely copy string and guarantee null-termination without strncpy truncation warnings.
void safe_str_copy(char *dest, const char *src, size_t dest_size) {
    if (dest != NULL && src != NULL && dest_size > 0) {
        size_t src_len = strlen(src);
        size_t copy_len = (src_len < dest_size - 1) ? src_len : (dest_size - 1);
        memcpy(dest, src, copy_len);
        dest[copy_len] = '\0';
    }
}

char *trim          (char *str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

bool str_case_eq(const char *s1, const char *s2) {
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

    safe_str_copy(op.raw, str, sizeof(op.raw));

    if (str[0] == '[' && str[strlen(str) - 1] == ']') {
        op.mode = MODE_INDIRECT;
        char inner[128] = {0}; 
        snprintf(inner, sizeof(inner), "%.*s", (int)(strlen(str) - 2), str + 1);

        char *plus_ptr  = strchr(inner, '+');
        char *minus_ptr = strrchr(inner, '-');

        if (plus_ptr) {
            *plus_ptr = '\0';
            safe_str_copy(op.reg, trim(inner), sizeof(op.reg));
            op.offset = (int)strtoul(trim(plus_ptr + 1), NULL, 0);
        } else if (minus_ptr && minus_ptr != inner) {
            *minus_ptr = '\0';
            safe_str_copy(op.reg, trim(inner), sizeof(op.reg));
            op.offset = -(int)strtoul(trim(minus_ptr + 1), NULL, 0);
        } else {
            safe_str_copy(op.reg, trim(inner), sizeof(op.reg));
            op.offset = 0;
        }
    } 
    else if (isdigit((unsigned char)str[0]) || (str[0] == '-' && isdigit((unsigned char)str[1]))) {
        op.mode = MODE_IMMEDIATE;
        // FIX: strtoul stops at the first non-digit character, so a
        // float literal like "0.500000" (a real, valid operand here -
        // e.g. set_channel_volume(0.5)) silently parsed as integer 0,
        // losing the fractional value entirely. That corrupted 0 then
        // got treated as a genuine tracked constant by later passes
        // (constant folding, forwarding) and could get folded into
        // completely unrelated code, replacing a correct register alias
        // with a bogus "MOV reg, 0x0" - confirmed directly: this exact
        // mechanism silently zeroed the SPU channel volume in a real
        // compiled program (every channel set to volume 0 instead of
        // 0.5/0.25), with no compile, assemble, or pack error anywhere,
        // since the corrupted value only ever gets used internally for
        // dataflow tracking - the original, correct "0.500000" text
        // survives untouched in `raw` and prints out fine anywhere
        // nothing tries to fold it. Flagging float operands here so
        // every downstream constant-tracking site can treat them as
        // unknown instead of trusting `immediate`.
        op.is_float = (strchr(str, '.') != NULL);
        op.immediate = op.is_float ? 0 : (int)strtoul(str, NULL, 0);
    } 
    else {
        op.mode = MODE_REG;
        safe_str_copy(op.reg, str, sizeof(op.reg));
    }

    return op;
}

AsmNode* create_node(const char* raw, OpType type, const char* mnem, const char* dst, const char* src) {
    AsmNode *node = (AsmNode*)calloc(1, sizeof(AsmNode));
    if (raw) safe_str_copy(node->raw, raw, sizeof(node->raw));
    node->type = type;
    if (mnem) safe_str_copy(node->mnemonic, mnem, sizeof(node->mnemonic));
    
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

    char line[8192]; 
    while (fgets(line, sizeof(line), fp)) {
        char raw[8192]; 
        safe_str_copy(raw, line, sizeof(raw));
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
            safe_str_copy(code_part, trimmed, len + 1);
        } else {
            safe_str_copy(code_part, trimmed, sizeof(code_part));
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
            safe_str_copy(mnem, code_trimmed, sizeof(mnem));
        } else {
            size_t mnem_len = space_ptr - code_trimmed;
            if (mnem_len >= sizeof(mnem)) mnem_len = sizeof(mnem) - 1;
            safe_str_copy(mnem, code_trimmed, mnem_len + 1);

            char *operands = trim(space_ptr);
            char *comma_ptr = strchr(operands, ',');
            if (comma_ptr) {
                *comma_ptr = '\0';
                safe_str_copy(dst, trim(operands), sizeof(dst));
                safe_str_copy(src, trim(comma_ptr + 1), sizeof(src));
            } else {
                safe_str_copy(dst, trim(operands), sizeof(dst));
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
        else if (str_case_eq(mnem, "IDIV")) type = OP_IDIV;
        else if (str_case_eq(mnem, "SHL"))  type = OP_SHL;
        else if (str_case_eq(mnem, "SHR"))  type = OP_SHR;

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
        char line_copy[8192]; 
        safe_str_copy(line_copy, curr->raw, sizeof(line_copy));
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

// Helper: Check if a positive integer is a power of 2 (2, 4, 8, 16...)
bool is_power_of_two(int x) {
    return (x > 0) && ((x & (x - 1)) == 0);
}

// Helper: Calculate log2 of a power-of-two integer (determines shift amount)
int get_log2(int x) {
    int log = 0;
    while (x > 1) {
        x >>= 1;
        log++;
    }
    return log;
}
