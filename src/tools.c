#include "v32opt.h"

// -------------------------------------------------------------------
// String Parsing & AST Utilities
// -------------------------------------------------------------------

// ===================================================================
// SAFE STRING COPY
// Safely copies a string to a destination buffer with guaranteed
// null-termination. Uses memcpy to avoid strncpy truncation warnings.
//   - dest: Destination buffer
//   - src: Source string
//   - dest_size: Size of destination buffer (including space for '\0')
// ===================================================================
void safe_str_copy(char *dest, const char *src, size_t dest_size) {
    if (dest != NULL && src != NULL && dest_size > 0) {
        size_t src_len = strlen(src);
        // Calculate how many bytes to copy (leave room for null terminator)
        size_t copy_len = (src_len < dest_size - 1) ? src_len : (dest_size - 1);
        memcpy(dest, src, copy_len);
        dest[copy_len] = '\0';  // Guarantee null-termination
    }
}

// ===================================================================
// STRING TRIMMING
// Removes leading and trailing whitespace from a string.
//   - str: String to trim (modified in-place)
// Returns: Pointer to the first non-whitespace character
// ===================================================================
char *trim(char *str) {
    // Skip leading whitespace
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;  // Empty string

    // Find end of string and trim trailing whitespace
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';  // Null-terminate after last non-whitespace char

    return str;
}

// ===================================================================
// CASE-INSENSITIVE STRING COMPARISON
// Compares two strings case-insensitively.
//   - s1: First string
//   - s2: Second string
// Returns: true if strings are equal (case-insensitive), false otherwise
// ===================================================================
bool str_case_eq(const char *s1, const char *s2) {
    // Compare character by character (case-insensitive)
    while (*s1 && *s2) {
        if (toupper((unsigned char)*s1) != toupper((unsigned char)*s2)) 
            return false;
        s1++; s2++;
    }
    // Both strings must reach end simultaneously
    return *s1 == *s2;
}

// ===================================================================
// REGISTER INDEX LOOKUP
// Returns the numeric index for a register name (e.g., "R3" → 3).
// Handles special aliases: SP/R15 → 15, BP/R14 → 14.
//   - reg_str: Register name (e.g., "R0", "SP", "r15")
// Returns: Register index (0-15) or -1 if invalid
// ===================================================================
int get_reg_index(const char *reg_str) {
    if (!reg_str || strlen(reg_str) == 0) return -1;

    // Handle special aliases
    if (str_case_eq(reg_str, "SP") || str_case_eq(reg_str, "R15")) return 15;
    if (str_case_eq(reg_str, "BP") || str_case_eq(reg_str, "R14")) return 14;

    // Handle R0-R15 format
    if (toupper((unsigned char)reg_str[0]) == 'R') {
        int idx = atoi(reg_str + 1);  // Parse number after 'R'
        if (idx >= 0 && idx < 16) return idx;
    }
    return -1;  // Invalid register
}

// ===================================================================
// OPERAND PARSING
// Parses an operand string into an Operand struct.
// Handles three addressing modes:
//   - Register: "R0", "SP", "BP"
//   - Immediate: "42", "-10", "0x20"
//   - Indirect: "[R1]", "[BP+4]", "[BP-8]"
// Also detects floating-point literals (e.g., "0.500000") and flags them
// to prevent incorrect integer parsing.
//   - str: Operand string to parse
// Returns: Parsed Operand struct
// ===================================================================
Operand parse_operand(const char *str) {
    Operand op;
    memset(&op, 0, sizeof(Operand));
    if (!str || strlen(str) == 0) return op;

    // Store raw string for later output
    safe_str_copy(op.raw, str, sizeof(op.raw));

    // --- Indirect Addressing: [reg] or [reg+offset] or [reg-offset] ---
    if (str[0] == '[' && str[strlen(str) - 1] == ']') {
        op.mode = MODE_INDIRECT;
        char inner[128] = {0};
        // Extract content between brackets
        snprintf(inner, sizeof(inner), "%.*s", (int)(strlen(str) - 2), str + 1);

        char *plus_ptr  = strchr(inner, '+');
        char *minus_ptr = strrchr(inner, '-');

        // Handle [reg+offset] format
        if (plus_ptr) {
            *plus_ptr = '\0';
            safe_str_copy(op.reg, trim(inner), sizeof(op.reg));
            op.offset = (int)strtoul(trim(plus_ptr + 1), NULL, 0);
        }
        // Handle [reg-offset] format
        else if (minus_ptr && minus_ptr != inner) {
            *minus_ptr = '\0';
            safe_str_copy(op.reg, trim(inner), sizeof(op.reg));
            op.offset = -(int)strtoul(trim(minus_ptr + 1), NULL, 0);
        }
        // Handle [reg] format (no offset)
        else {
            safe_str_copy(op.reg, trim(inner), sizeof(op.reg));
            op.offset = 0;
        }
    }
    // --- Immediate Value: "42", "-10", "0x20" ---
    else if (isdigit((unsigned char)str[0]) || (str[0] == '-' && isdigit((unsigned char)str[1]))) {
        op.mode = MODE_IMMEDIATE;

        // FIX: Detect floating-point literals (e.g., "0.500000")
        // strtoul would incorrectly parse "0.5" as 0, causing silent data corruption
        // in later passes. Flag floats so downstream code treats them as unknown.
        op.is_float = (strchr(str, '.') != NULL);
        op.immediate = op.is_float ? 0 : (int)strtoul(str, NULL, 0);
    }
    // --- Register or Symbol ---
    else {
        if (get_reg_index(str) != -1) {
            op.mode = MODE_REG;
            safe_str_copy(op.reg, str, sizeof(op.reg));
        } else {
            // It is a label, global symbol, or constant name (e.g., my_buffer)
            op.mode = MODE_IMMEDIATE;
            op.is_float = false;
            op.immediate = 0; // Symbol address resolved at link/assemble time
        }
    }

    return op;
}

// ===================================================================
// ASM NODE CREATION
// Allocates and initializes a new AsmNode with the given properties.
//   - raw: Raw assembly text (e.g., "    MOV R1, R2")
//   - type: Operation type (OP_MOV, OP_IADD, etc.)
//   - mnem: Mnemonic (e.g., "MOV", "IADD")
//   - dst: Destination operand string
//   - src: Source operand string
// Returns: Newly allocated AsmNode (caller must free)
// ===================================================================
AsmNode* create_node(const char* raw, OpType type, const char* mnem, const char* dst, const char* src) {
    AsmNode *node = (AsmNode*)calloc(1, sizeof(AsmNode));
    if (raw) safe_str_copy(node->raw, raw, sizeof(node->raw));
    node->type = type;
    if (mnem) safe_str_copy(node->mnemonic, mnem, sizeof(node->mnemonic));

    // Parse destination operand if provided
    if (dst && strlen(dst) > 0) {
        node->dst_op = parse_operand(dst);
        node->has_dst = true;
    }
    // Parse source operand if provided
    if (src && strlen(src) > 0) {
        node->src_op = parse_operand(src);
        node->has_src = true;
    }
    return node;
}

// ===================================================================
// ASM NODE REMOVAL
// Removes a node from its linked list and frees its memory.
//   - node: Node to remove and free
// ===================================================================
void remove_node(AsmNode *node) {
    // Unlink from previous node
    if (node->prev) node->prev->next = node->next;
    // Unlink from next node
    if (node->next) node->next->prev = node->prev;
    free(node);
}

// ===================================================================
// ASM NODE CLONING (STATIC - FILE LOCAL)
// Creates a deep copy of an AsmNode. Used internally by other functions.
//   - src: Node to clone
// Returns: Newly allocated copy (caller must free)
// ===================================================================
AsmNode* clone_node(AsmNode *src) {
    AsmNode *dst = (AsmNode*)calloc(1, sizeof(AsmNode));
    memcpy(dst, src, sizeof(AsmNode));
    // Clear pointers to avoid dangling references
    dst->prev = NULL;
    dst->next = NULL;
    return dst;
}

// -------------------------------------------------------------------
// Assembly File Parser & Writer
// -------------------------------------------------------------------

// ===================================================================
// VIRCON32 ASSEMBLY PARSER
// Parses a Vircon32 assembly file into a linked list of AsmNode structs.
// Handles:
//   - Instructions (MOV, IADD, ISUB, etc.)
//   - Labels (e.g., "my_label:")
//   - Comments (lines starting with ';' or after ';')
//   - Blank lines
//   - Operands in various formats
//   - filename: Path to the input .asm file
// Returns: Dummy head node (first real node is head->next)
// ===================================================================
AsmNode* parse_vircon32_asm(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening input assembly file");
        exit(EXIT_FAILURE);
    }

    // Create dummy head for easier list management
    AsmNode *dummy_head = create_node(NULL, OP_OTHER, NULL, NULL, NULL);
    AsmNode *tail = dummy_head;

    char line[8192];
    while (fgets(line, sizeof(line), fp)) {
        char raw[8192];
        safe_str_copy(raw, line, sizeof(raw));
        // Remove line endings
        raw[strcspn(raw, "\r\n")] = '\0';

        char *trimmed = trim(line);

        // --- Blank Lines & Comments ---
        if (strlen(trimmed) == 0 || trimmed[0] == ';') {
            AsmNode *node = create_node(raw, OP_OTHER, NULL, NULL, NULL);
            tail->next = node; node->prev = tail; tail = node;
            continue;
        }

        // --- Extract Code (Remove Inline Comments) ---
        char code_part[256] = {0};
        char *comment_ptr = strchr(trimmed, ';');
        if (comment_ptr) {
            // Copy only the part before the comment
            size_t len = comment_ptr - trimmed;
            safe_str_copy(code_part, trimmed, len + 1);
        } else {
            safe_str_copy(code_part, trimmed, sizeof(code_part));
        }
        char *code_trimmed = trim(code_part);

        // --- Labels (e.g., "my_label:") ---
        if (code_trimmed[strlen(code_trimmed) - 1] == ':') {
            AsmNode *node = create_node(raw, OP_LABEL, NULL, NULL, NULL);
            tail->next = node; node->prev = tail; tail = node;
            continue;
        }

        // --- Instructions ---
        char mnem[32] = {0}, dst[128] = {0}, src[128] = {0};
        char *space_ptr = strpbrk(code_trimmed, " \t");

        // Extract mnemonic
        if (!space_ptr) {
            safe_str_copy(mnem, code_trimmed, sizeof(mnem));
        } else {
            size_t mnem_len = space_ptr - code_trimmed;
            if (mnem_len >= sizeof(mnem)) mnem_len = sizeof(mnem) - 1;
            safe_str_copy(mnem, code_trimmed, mnem_len + 1);

            // Parse operands
            char *operands = trim(space_ptr);
            char *comma_ptr = strchr(operands, ',');
            if (comma_ptr) {
                // Two operands: split at comma
                *comma_ptr = '\0';
                safe_str_copy(dst, trim(operands), sizeof(dst));
                safe_str_copy(src, trim(comma_ptr + 1), sizeof(src));
            } else {
                // Single operand
                safe_str_copy(dst, trim(operands), sizeof(dst));
            }
        }

        // Map mnemonic to OpType
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

// ===================================================================
// VIRCON32 ASSEMBLY WRITER
// Writes a linked list of AsmNode structs to a Vircon32 assembly file.
// Preserves original formatting and blank lines.
//   - filename: Path to the output .asm file
//   - head: Head of the AsmNode list (dummy head node is skipped)
// ===================================================================
void write_vircon32_asm(const char *filename, AsmNode *head) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror("Error opening output assembly file");
        exit(EXIT_FAILURE);
    }

    // Skip dummy head if present (identified by empty raw string)
    AsmNode *curr = (head && head->type == OP_OTHER && head->raw[0] == '\0') ? head->next : head;
    bool last_was_blank = false;

    while (curr) {
        char line_copy[8192];
        safe_str_copy(line_copy, curr->raw, sizeof(line_copy));
        char *trimmed = trim(line_copy);

        bool is_blank = (strlen(trimmed) == 0);

        // --- Blank Line Handling ---
        // Collapse consecutive blank lines to avoid excessive whitespace
        if (is_blank) {
            if (!last_was_blank) {
                fprintf(fp, "\n");
                last_was_blank = true;
            }
        } else {
            // Write non-blank line as-is
            fprintf(fp, "%s\n", curr->raw);
            last_was_blank = false;
        }

        curr = curr->next;
    }

    fclose(fp);
}

// ===================================================================
// POWER OF TWO CHECK
// Checks if a positive integer is a power of two (2, 4, 8, 16, ...).
// Uses bitwise trick: x & (x - 1) == 0 for powers of two.
//   - x: Integer to check
// Returns: true if x is a power of two, false otherwise
// ===================================================================
bool is_power_of_two(int x) {
    return (x > 0) && ((x & (x - 1)) == 0);
}

// ===================================================================
// LOG2 CALCULATION
// Computes the base-2 logarithm of a power-of-two integer.
// Used for determining shift amounts in strength reduction.
//   - x: Power-of-two integer
// Returns: log2(x) (e.g., 8 → 3, 16 → 4)
// ===================================================================
int get_log2(int x) {
    int log = 0;
    while (x > 1) {
        x >>= 1;  // Right shift (divide by 2)
        log++;
    }
    return log;
}
