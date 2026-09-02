#include "v32opt.h"

// Map OptType enum to human-readable names
const char *opt_type_names[]       = {
    [OPT_PEEPHOLE_ALGEBRA]         = "peephole-algebra",
    [OPT_PEEPHOLE_COMPILER_MYOPIA] = "peephole-compiler-myopia",
    [OPT_PEEPHOLE_DEAD_STORES]     = "peephole-dead-stores",
    [OPT_PEEPHOLE_FORWARDING]      = "peephole-forwarding",
    [OPT_PEEPHOLE_IMMEDIATE_PROP]  = "peephole-immediate-prop",
    [OPT_PEEPHOLE_IMMEDIATES]      = "peephole-immediates",
    [OPT_PEEPHOLE_JMP_CHAIN]       = "peephole-jmp-chain",
    [OPT_PEEPHOLE_JUMPS]           = "peephole-jumps",
    [OPT_PEEPHOLE_LOADS]           = "peephole-loads",
    [OPT_PEEPHOLE_MOVS]            = "peephole-movs",
    [OPT_PEEPHOLE_PAIRS]           = "peephole-pairs",
    [OPT_PEEPHOLE_REDUCE]          = "peephole-reduce",
    [OPT_PEEPHOLE_SHIFTS]          = "peephole-shifts",
    [OPT_CONSTANT_FOLDING]         = "constant-folding",
    [OPT_CSE]                      = "cse",
    [OPT_DCE]                      = "dce",
    [OPT_OMIT_FRAME_POINTERS]      = "omit-frame-pointers",
    [OPT_INLINE]                   = "inline",
    [OPT_PROMOTE_LEAF]             = "promote-leaf",
    [OPT_PROMOTE_LOOPS]            = "promote-loops",
    [OPT_PROMOTE_REGS]             = "promote-regs"
};

bool is_lua_mode(void) {
    extern OptConfig config;
    return config.lang_mode == LANG_LUA;
}

bool is_boxed_type_operand(const Operand *op) {
    if (!op || op->mode != MODE_IMMEDIATE) return false;
    const char *raw = op->raw;
    return (strstr(raw, "BOXED_") != NULL) ||
           (strstr(raw, "0x7F") == raw) ||  // BOXED_FUNCTION
           (strstr(raw, "0xFF") == raw);    // BOXED_NIL, BOXED_TABLE, etc.
}

bool is_boxed_tagging(AsmNode *node) {
    if (!node) return false;
    // Any instruction that manipulates a value's NaN-boxing tag bits via a
    // BOXED_* immediate operand. OR boxes a table/function/string pointer
    // ("OR Rd, BOXED_FUNCTION"); AND strips the tag bits back out
    // (unboxing, e.g. "AND Rd, BOXED_PAYLOAD"); IADD boxes a Lua boolean
    // ("IADD Rd, BOXED_BOOLEAN"). This used to only recognize OR, which let
    // the AND/IADD boxing idioms slip past CSE's and constant-folding's
    // Lua-mode guards elsewhere (see fold_constants_cfg() in cfg.c and
    // is_computable_expression() in cse.c).
    //
    // NOTE: "XOR Rd, 3" (flip BOXED_FALSE<->BOXED_TRUE) is a *deliberate*
    // blind spot here -- it has no BOXED_* operand to key off of, so it
    // can't be recognized by this name-based check. It's safe today only
    // because nothing currently treats XOR as constant-foldable or
    // CSE-computable in a way that would corrupt it; if that ever changes,
    // this function will need a smarter check (e.g. tracking that Rd's
    // value came from a boxed-boolean producer) rather than another string
    // match.
    if (node->type != OP_OR && node->type != OP_AND && node->type != OP_IADD)
        return false;
    return is_boxed_type_operand(&node->src_op);
}

// Helper: remove nodes and insert debug comments
void remove_with_debug(AsmNode **curr_ptr, AsmNode *nodes[], int count, OptType opt_type)
{
    if (config.debug) {
        for (int i = 0; i < count; i++) {
            insert_debug_comment(nodes[i]->prev, opt_type, nodes[i]->raw);
        }
    }
    AsmNode *last = nodes[count - 1];
    AsmNode *next_after = last->next;
    for (int i = 0; i < count; i++) {
        remove_node(nodes[i]);
    }
    *curr_ptr = next_after;
}

// Helper: strip comments from line
void strip_comment_from_line(char *dest, const char *src, size_t dest_size) {
    safe_str_copy(dest, src, dest_size);
    char *semicolon = strchr(dest, ';');
    if (semicolon) *semicolon = '\0';
}

// Helper: normalize whitespace (trim leading/trailing, collapse internal)
void normalize_whitespace(char *dest, const char *src, size_t dest_size) {
    const char *p = src;
    char *q = dest;
    char *end = dest + dest_size - 1;

    // Skip leading whitespace
    while (*p && isspace((unsigned char)*p)) p++;

    // Copy and collapse internal whitespace
    while (*p && q < end) {
        if (isspace((unsigned char)*p)) {
            while (isspace((unsigned char)*p)) p++;
            if (q > dest) *q++ = ' ';  // Single space separator
        } else {
            *q++ = *p++;
        }
    }
    *q = '\0';

    // Remove trailing space
    if (q > dest && *(q-1) == ' ') *(q-1) = '\0';
}

void insert_debug_comment(AsmNode *after, OptType opt_type, const char *original_instr) {
    if (!config.debug) return;

    // Validate opt_type is in bounds
    const char *pass_name = "unknown";
    if (opt_type >= 0 && opt_type < MAX_OPTIMIZATION_ALGORITHMS) {
        pass_name = opt_type_names[opt_type];
    }

    // Ensure pass_name doesn't contain % (defensive)
    char safe_pass_name[128];
    size_t i = 0, j = 0;
    while (pass_name[i] && j < sizeof(safe_pass_name) - 1) {
        if (pass_name[i] == '%') {
            if (j + 1 < sizeof(safe_pass_name)) {
                safe_pass_name[j++] = '%';
                safe_pass_name[j++] = '%';
            }
            i++;
        } else {
            safe_pass_name[j++] = pass_name[i++];
        }
    }
    safe_pass_name[j] = '\0';

    char stripped[8192];
    strip_comment_from_line(stripped, original_instr, sizeof(stripped));

    char normalized[8192];
    normalize_whitespace(normalized, stripped, sizeof(normalized));

    AsmNode *comment = calloc(1, sizeof(AsmNode));
    comment->type = OP_OTHER;

    char debug_prefix[128];
    snprintf(debug_prefix, sizeof(debug_prefix), "; [DEBUG %s] ", safe_pass_name);
    snprintf(comment->raw, sizeof(comment->raw), "%s%s", debug_prefix, normalized);

    if (after) {
        comment->prev = after;
        comment->next = after->next;
        if (after->next) after->next->prev = comment;
        after->next = comment;
    }
}

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
        op.is_float   = (strchr(str, '.') != NULL);
        op.immediate  = op.is_float ? 0 : (int)strtoul(str, NULL, 0);
    }
    // --- Register: ONLY R0-R15, SP, BP ---
    else if (get_reg_index (str) >= 0)
    {
        op.mode = MODE_REG;
        safe_str_copy(op.reg, str, sizeof(op.reg));
    }
    // --- Labels/Symbols: Everything else (e.g., __literal_string_11455) ---
    else
    {
        op.mode = MODE_IMMEDIATE;  // Treat as address (resolved by assembler)
        op.immediate = 0;
        op.is_float = false;
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
        if (str_case_eq (mnem, "HLT"))  type  = OP_HLT;
        else if (str_case_eq (mnem, "WAIT"))  type  = OP_WAIT;
        else if (str_case_eq (mnem, "JMP"))  type  = OP_JMP;
        else if (str_case_eq (mnem, "CALL"))  type  = OP_CALL;
        else if (str_case_eq (mnem, "RET"))  type  = OP_RET;
        else if (str_case_eq (mnem, "JT"))  type  = OP_JT;
        else if (str_case_eq (mnem, "JF"))  type  = OP_JF;
        else if (str_case_eq (mnem, "IEQ"))  type  = OP_IEQ;
        else if (str_case_eq (mnem, "INE"))  type  = OP_INE;
        else if (str_case_eq (mnem, "IGT"))  type  = OP_IGT;
        else if (str_case_eq (mnem, "IGE"))  type  = OP_IGE;
        else if (str_case_eq (mnem, "ILT"))  type  = OP_ILT;
        else if (str_case_eq (mnem, "ILE"))  type  = OP_ILE;
        else if (str_case_eq (mnem, "FEQ"))  type  = OP_FEQ;
        else if (str_case_eq (mnem, "FNE"))  type  = OP_FNE;
        else if (str_case_eq (mnem, "FGT"))  type  = OP_FGT;
        else if (str_case_eq (mnem, "FGE"))  type  = OP_FGE;
        else if (str_case_eq (mnem, "FLT"))  type  = OP_FLT;
        else if (str_case_eq (mnem, "FLE"))  type  = OP_FLE;
        else if (str_case_eq (mnem, "MOV"))  type  = OP_MOV;
        else if (str_case_eq (mnem, "LEA"))  type  = OP_LEA;
        else if (str_case_eq (mnem, "PUSH"))  type  = OP_PUSH;
        else if (str_case_eq (mnem, "POP"))  type  = OP_POP;
        else if (str_case_eq (mnem, "IN"))  type  = OP_IN;
        else if (str_case_eq (mnem, "OUT"))  type  = OP_OUT;
        else if (str_case_eq (mnem, "MOVS"))  type  = OP_MOVS;
        else if (str_case_eq (mnem, "SETS"))  type  = OP_SETS;
        else if (str_case_eq (mnem, "CMPS"))  type  = OP_CMPS;
        else if (str_case_eq (mnem, "CIF"))  type  = OP_CIF;
        else if (str_case_eq (mnem, "CFI"))  type  = OP_CFI;
        else if (str_case_eq (mnem, "CIB"))  type  = OP_CIB;
        else if (str_case_eq (mnem, "CFB"))  type  = OP_CFB;
        else if (str_case_eq (mnem, "NOT"))  type  = OP_NOT;
        else if (str_case_eq (mnem, "AND"))  type  = OP_AND;
        else if (str_case_eq (mnem, "OR"))  type  = OP_OR;
        else if (str_case_eq (mnem, "XOR"))  type  = OP_XOR;
        else if (str_case_eq (mnem, "BNOT"))  type  = OP_BNOT;
        else if (str_case_eq (mnem, "SHL"))  type  = OP_SHL;
        else if (str_case_eq (mnem, "IADD"))  type  = OP_IADD;
        else if (str_case_eq (mnem, "ISUB"))  type  = OP_ISUB;
        else if (str_case_eq (mnem, "IMUL"))  type  = OP_IMUL;
        else if (str_case_eq (mnem, "IDIV"))  type  = OP_IDIV;
        else if (str_case_eq (mnem, "IMOD"))  type  = OP_IMOD;
        else if (str_case_eq (mnem, "ISGN"))  type  = OP_ISGN;
        else if (str_case_eq (mnem, "IMIN"))  type  = OP_IMIN;
        else if (str_case_eq (mnem, "IMAX"))  type  = OP_IMAX;
        else if (str_case_eq (mnem, "IABS"))  type  = OP_IABS;
        else if (str_case_eq (mnem, "FADD"))  type  = OP_FADD;
        else if (str_case_eq (mnem, "FSUB"))  type  = OP_FSUB;
        else if (str_case_eq (mnem, "FMUL"))  type  = OP_FMUL;
        else if (str_case_eq (mnem, "FDIV"))  type  = OP_FDIV;
        else if (str_case_eq (mnem, "FMOD"))  type  = OP_FMOD;
        else if (str_case_eq (mnem, "FSGN"))  type  = OP_FSGN;
        else if (str_case_eq (mnem, "FMIN"))  type  = OP_FMIN;
        else if (str_case_eq (mnem, "FMAX"))  type  = OP_FMAX;
        else if (str_case_eq (mnem, "FABS"))  type  = OP_FABS;
        else if (str_case_eq (mnem, "FLR"))  type  = OP_FLR;
        else if (str_case_eq (mnem, "CEIL"))  type  = OP_CEIL;
        else if (str_case_eq (mnem, "ROUND"))  type  = OP_ROUND;
        else if (str_case_eq (mnem, "SIN"))  type  = OP_SIN;
        else if (str_case_eq (mnem, "ACOS"))  type  = OP_ACOS;
        else if (str_case_eq (mnem, "ATAN2"))  type  = OP_ATAN2;
        else if (str_case_eq (mnem, "LOG"))  type  = OP_LOG;
        else if (str_case_eq (mnem, "POW"))  type  = OP_POW;

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
