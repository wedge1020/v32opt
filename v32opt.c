#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#define MAX_INLINE_CANDIDATES 64
#define MAX_BODY_INS 8
// FIX: 256 was too small for any real-sized program - this project's
// own 42-game, ~61K-line compiled test file has 680 genuine functions
// on its own, comfortably over the original limit. Raised well past
// that with room to grow; each FunctionDef entry is small (a name
// buffer, two pointers, a bool), so the memory cost of a larger static
// array here is negligible.
#define MAX_FUNCTIONS 4096

// DIAGNOSTIC ONLY (not a correctness fix): see comment at its use site
// in pass_inline_trivial_functions, set via -finline-max=N. -1 = no
// limit (default, normal behavior unchanged). Persistent across every
// invocation of the pass within one run (it runs repeatedly until a
// fixed point), so N means a true total, not "N per call".
int g_inline_call_limit = -1;
int g_inline_calls_so_far = 0;
char g_inline_exclude_name[1024] = {0};

// -------------------------------------------------------------------
// Enums & Data Structures
// -------------------------------------------------------------------

typedef enum {
    OP_MOV, OP_IADD, OP_ISUB, OP_IMUL, OP_IDIV, OP_IEQ, OP_INE, 
    OP_CIB, OP_PUSH, OP_POP, OP_BNOT, OP_SHL, OP_SHR, OP_OTHER, OP_LABEL
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
    // FIX: distinguishes a float literal (e.g. "0.500000") from a real
    // integer immediate. Both parse into MODE_IMMEDIATE, but `immediate`
    // is meaningless for a float - see parse_operand and its comment
    // for why. Every place that reads `immediate` for constant
    // tracking/folding must check this first and treat a float operand
    // as unknown (VAL_BOTTOM), never as the (corrupted) integer value.
    bool is_float;
} Operand;

typedef struct AsmNode {
    OpType type;
    char raw[8192];      
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
    bool enable_jump_next;
    bool enable_redundant_movs;
    bool enable_combine_immediates;
    bool enable_strength_reduction;
} OptConfig;

// -------------------------------------------------------------------
// String Parsing & AST Utilities
// -------------------------------------------------------------------

// Wrapper function using strncpy at its core to safely copy strings
// without generating -Wformat-truncation or -Wstringop-truncation warnings.
// Safely copy string and guarantee null-termination without strncpy truncation warnings.
static inline void safe_str_copy(char *dest, const char *src, size_t dest_size) {
    if (dest != NULL && src != NULL && dest_size > 0) {
        size_t src_len = strlen(src);
        size_t copy_len = (src_len < dest_size - 1) ? src_len : (dest_size - 1);
        memcpy(dest, src, copy_len);
        dest[copy_len] = '\0';
    }
}

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
            curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float && curr->src_op.immediate == 0) 
        {
            remove_node(curr);
            optimizations++;
            curr = next;
            continue;
        }

        if (curr->type == OP_IMUL && 
            curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float && curr->src_op.immediate == 2) 
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
// Pass: Redundant Jump Elimination
// -------------------------------------------------------------------
int pass_redundant_jumps(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr) {
        if (str_case_eq(curr->mnemonic, "JMP")) {
            // Find the next actual instruction or label (skip comments/blanks)
            AsmNode *next_node = curr->next;
            while (next_node && next_node->type == OP_OTHER && 
                  (next_node->raw[0] == '\0' || next_node->raw[0] == ';')) {
                next_node = next_node->next;
            }

            if (next_node && next_node->type == OP_LABEL) {
                char lbl[128] = {0};
                safe_str_copy(lbl, next_node->raw, sizeof(lbl));
                char *colon = strchr(lbl, ':');
                if (colon) *colon = '\0';
                
                if (str_case_eq(trim(lbl), trim(curr->dst_op.raw))) {
                    AsmNode *to_remove = curr;
                    curr = curr->next;
                    remove_node(to_remove);
                    optimizations++;
                    continue;
                }
            }
        }
        curr = curr->next;
    }
    return optimizations;
}

// -------------------------------------------------------------------
// Pass: Redundant & Mirror Move Elimination
// -------------------------------------------------------------------
int pass_redundant_movs(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr) {
        if (curr->type == OP_MOV) {
            AsmNode *n2 = curr->next;
            while (n2 && n2->type == OP_OTHER && 
                  (n2->raw[0] == '\0' || n2->raw[0] == ';')) {
                n2 = n2->next;
            }
            if (!n2) break;

            if (n2->type == OP_MOV) {
                // Case 1: Duplicate Move (e.g., MOV R0, X followed by MOV R0, X)
                //
                // FIX: this compared operand *text* only, which is unsound
                // when the source is an indirect load through the very
                // register being written - e.g. "MOV R1, [R1]" followed by
                // another "MOV R1, [R1]". Textually identical, but NOT the
                // same instruction semantically: the first load computes
                // R1 := mem[R1_old], which changes R1 - so the second,
                // textually-identical "[R1]" now addresses a completely
                // different location (mem[R1_new]). This is pointer-
                // chasing (a double dereference), not a duplicate, and
                // deleting the second instruction silently drops one level
                // of indirection. Confirmed directly: this exact pattern
                // appears in every game's compiled CAudio_PlaySound (built
                // from "*CAudio_Sounds[SoundID]", a pointer-to-pointer
                // dereference) - deleting the second load passed a raw
                // pointer to select_sound()/assign_channel_sound() where
                // the actual sound handle was expected, silently breaking
                // audio playback in the packed ROM with no compile,
                // assemble, or pack error anywhere.
                //
                // The two instructions are only safely removable when the
                // source doesn't depend on the register the first
                // instruction just wrote - i.e. skip this whenever the
                // source is an indirect operand whose base register is the
                // same register being written.
                bool self_referential_load =
                    (curr->src_op.mode == MODE_INDIRECT) &&
                    str_case_eq(curr->src_op.reg, curr->dst_op.reg);

                if (!self_referential_load &&
                    str_case_eq(curr->dst_op.raw, n2->dst_op.raw) &&
                    str_case_eq(curr->src_op.raw, n2->src_op.raw)) 
                {
                    remove_node(n2);
                    optimizations++;
                    continue;
                }

                // Case 2: Mirror Move (e.g., MOV R0, R1 followed by MOV R1, R0)
                if (curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_REG &&
                    n2->dst_op.mode == MODE_REG && n2->src_op.mode == MODE_REG) 
                {
                    if (str_case_eq(curr->dst_op.reg, n2->src_op.reg) &&
                        str_case_eq(curr->src_op.reg, n2->dst_op.reg)) 
                    {
                        remove_node(n2);
                        optimizations++;
                        continue;
                    }
                }
            }
        }
        curr = curr->next;
    }
    return optimizations;
}

// -------------------------------------------------------------------
// Pass: Immediate Math Combining
// -------------------------------------------------------------------
int pass_combine_immediates(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr) {
        if ((curr->type == OP_IADD || curr->type == OP_ISUB) &&
            curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float) 
        {
            AsmNode *n2 = curr->next;
            while (n2 && n2->type == OP_OTHER && 
                  (n2->raw[0] == '\0' || n2->raw[0] == ';')) {
                n2 = n2->next;
            }

            if (n2 && (n2->type == OP_IADD || n2->type == OP_ISUB) &&
                n2->dst_op.mode == MODE_REG && n2->src_op.mode == MODE_IMMEDIATE && !n2->src_op.is_float &&
                str_case_eq(curr->dst_op.reg, n2->dst_op.reg)) 
            {
                int val1 = (curr->type == OP_IADD) ? curr->src_op.immediate : -curr->src_op.immediate;
                int val2 = (n2->type == OP_IADD) ? n2->src_op.immediate : -n2->src_op.immediate;
                int combined = val1 + val2;

                if (combined == 0) {
                    // They canceled each other out completely!
                    AsmNode *next_iter = n2->next;
                    remove_node(curr);
                    remove_node(n2);
                    curr = next_iter;
                    optimizations += 2;
                    continue;
                } else if (combined > 0) {
                    curr->type = OP_IADD;
                    safe_str_copy(curr->mnemonic, "IADD", sizeof(curr->mnemonic));
                    curr->src_op.immediate = combined;
                    snprintf(curr->src_op.raw, sizeof(curr->src_op.raw), "%d", combined);
                    snprintf(curr->raw, sizeof(curr->raw), "    IADD %s, %d", curr->dst_op.raw, combined);
                } else {
                    curr->type = OP_ISUB;
                    safe_str_copy(curr->mnemonic, "ISUB", sizeof(curr->mnemonic));
                    curr->src_op.immediate = -combined;
                    snprintf(curr->src_op.raw, sizeof(curr->src_op.raw), "%d", -combined);
                    snprintf(curr->raw, sizeof(curr->raw), "    ISUB %s, %d", curr->dst_op.raw, -combined);
                }
                remove_node(n2);
                optimizations++;
                continue;
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
            char line_copy[8192];
            safe_str_copy(line_copy, curr->raw, sizeof(line_copy));
            char *trimmed_lbl = trim(line_copy);

            safe_str_copy(func_name, trimmed_lbl, sizeof(func_name));
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

                // FIX: reject any candidate whose body touches SP
                // directly (PUSH/POP, or any instruction with SP as an
                // operand register) - these change the stack layout
                // mid-body, which the BP->SP offset rewrite below can't
                // account for. Also reject any [BP-N] (negative offset)
                // reference, which would indicate a local-variable slot
                // the callee's own (skipped) prologue reserved space
                // for - not something this rewrite handles either.
                if (str_case_eq(scan->mnemonic, "PUSH") || str_case_eq(scan->mnemonic, "POP")) {
                    valid_candidate = false;
                    break;
                }
                if (str_case_eq(scan->dst_op.reg, "SP") || str_case_eq(scan->src_op.reg, "SP")) {
                    valid_candidate = false;
                    break;
                }
                if ((scan->dst_op.mode == MODE_INDIRECT && str_case_eq(scan->dst_op.reg, "BP") && scan->dst_op.offset < 0) ||
                    (scan->src_op.mode == MODE_INDIRECT && str_case_eq(scan->src_op.reg, "BP") && scan->src_op.offset < 0)) {
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
                safe_str_copy(candidates[candidate_count].name, func_name, sizeof(candidates[candidate_count].name));
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
            safe_str_copy(target_label, trim(curr->dst_op.raw), sizeof(target_label));

            // DIAGNOSTIC ONLY (not a correctness fix): g_inline_call_limit,
            // set via -finline-max=N, caps how many CALL sites get
            // inlined, in file order, so a real build can be bisected by
            // testing N values against actual hardware/emulator - added
            // to narrow down which of many inlined call sites causes a
            // runtime-only symptom (confirmed-silent audio in a full
            // -O3 build) that static tracing of the obvious candidates
            // didn't explain. Default -1 means no limit (normal
            // behavior, unchanged).
            if (g_inline_call_limit >= 0 && g_inline_calls_so_far >= g_inline_call_limit) {
                curr = next_node;
                continue;
            }
            // DIAGNOSTIC ONLY: g_inline_exclude_name, set via
            // -finline-exclude=NAME, skips inlining just that one named
            // function (everything else inlines normally) - for testing
            // one specific function's inlining in isolation against a
            // real, otherwise-full -O3 build.
            if (g_inline_exclude_name[0] != '\0') {
                // Comma-separated list support: check target_label
                // against each token.
                char list_copy[1024];
                safe_str_copy(list_copy, g_inline_exclude_name, sizeof(list_copy));
                bool excluded = false;
                char *tok = strtok(list_copy, ",");
                while (tok) {
                    if (str_case_eq(trim(tok), target_label)) { excluded = true; break; }
                    tok = strtok(NULL, ",");
                }
                if (excluded) {
                    curr = next_node;
                    continue;
                }
            }

            for (int c = 0; c < candidate_count; c++) {
                if (str_case_eq(target_label, candidates[c].name)) {
                    AsmNode *insertion_point = curr->prev;

                    for (int b = 0; b < candidates[c].body_count; b++) {
                        AsmNode *inlined_ins = clone_node(candidates[c].body_nodes[b]);

                        // FIX: rewrite any [BP+N] (N>=2) parameter
                        // reference in the cloned instruction to
                        // [SP+(N-2)] instead - [BP+N] was only valid
                        // relative to the callee's own prologue-
                        // established BP, which no longer exists once
                        // this body is spliced into the caller with no
                        // new prologue of its own. At the call site,
                        // [BP+N] (inside the callee) and [SP+(N-2)] (at
                        // the call site, before the call executes) refer
                        // to the exact same stack slot - confirmed
                        // empirically: a single-argument callee reads
                        // its parameter at [BP+2], and the caller pushes
                        // that same argument to [SP+0] immediately
                        // before the call.
                        if (inlined_ins->dst_op.mode == MODE_INDIRECT &&
                            str_case_eq(inlined_ins->dst_op.reg, "BP") &&
                            inlined_ins->dst_op.offset >= 2) {
                            inlined_ins->dst_op.offset -= 2;
                            safe_str_copy(inlined_ins->dst_op.reg, "SP", sizeof(inlined_ins->dst_op.reg));
                            snprintf(inlined_ins->dst_op.raw, sizeof(inlined_ins->dst_op.raw),
                                     "[SP+%d]", inlined_ins->dst_op.offset);
                        }
                        if (inlined_ins->src_op.mode == MODE_INDIRECT &&
                            str_case_eq(inlined_ins->src_op.reg, "BP") &&
                            inlined_ins->src_op.offset >= 2) {
                            inlined_ins->src_op.offset -= 2;
                            safe_str_copy(inlined_ins->src_op.reg, "SP", sizeof(inlined_ins->src_op.reg));
                            snprintf(inlined_ins->src_op.raw, sizeof(inlined_ins->src_op.raw),
                                     "[SP+%d]", inlined_ins->src_op.offset);
                        }
                        // write_vircon32_asm() emits raw directly, not
                        // the parsed operand structs - regenerate it
                        // from the (possibly rewritten) operands above,
                        // preserving the original instruction's own
                        // mnemonic case, or the offset fix never reaches
                        // the actual output.
                        if (inlined_ins->has_dst && inlined_ins->has_src) {
                            snprintf(inlined_ins->raw, sizeof(inlined_ins->raw), "  %s %s, %s",
                                     inlined_ins->mnemonic, inlined_ins->dst_op.raw, inlined_ins->src_op.raw);
                        } else if (inlined_ins->has_dst) {
                            snprintf(inlined_ins->raw, sizeof(inlined_ins->raw), "  %s %s",
                                     inlined_ins->mnemonic, inlined_ins->dst_op.raw);
                        }

                        inlined_ins->prev = insertion_point;
                        inlined_ins->next = curr;
                        if (insertion_point) insertion_point->next = inlined_ins;
                        curr->prev = inlined_ins;

                        insertion_point = inlined_ins;
                    }

                    remove_node(curr);
                    inlined_calls++;
                    g_inline_calls_so_far++;
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
int pass_dead_function_elimination(AsmNode *head) {
    FunctionDef funcs[MAX_FUNCTIONS];
    int func_count = 0;

    AsmNode *curr = head ? head->next : NULL;

    // ----------------------------------------------------------------
    // 1. Capture Unlabeled Preamble (Boot Vector)
    // ----------------------------------------------------------------
    AsmNode *preamble_start = curr;
    while (preamble_start && preamble_start->type == OP_OTHER && 
          (preamble_start->raw[0] == '\0' || preamble_start->raw[0] == ';')) {
        preamble_start = preamble_start->next;
    }

    if (preamble_start && preamble_start->type != OP_LABEL) {
        AsmNode *preamble_end = preamble_start;
        while (preamble_end->next && preamble_end->next->type != OP_LABEL) {
            preamble_end = preamble_end->next;
        }

        if (func_count < MAX_FUNCTIONS) {
            safe_str_copy(funcs[func_count].name, "__boot_vector", sizeof(funcs[func_count].name));
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
            char line_copy[8192]; 
            safe_str_copy(line_copy, curr->raw, sizeof(line_copy));
            char *lbl = trim(line_copy);

            // FIX: the original condition here ("not starting with '.'
            // or '@', and not containing '_return:'") treats *every*
            // internal control-flow label (__if_N_start, __for_N_start,
            // __while_N_start, __LogicalAnd_ShortCircuit_N, etc.) as the
            // start of a brand new, separate "function" - confirmed on
            // a real 61K-line compiled program: 680 genuine functions
            // vs. 4230 such internal labels, all misidentified as
            // functions of their own. A real function's own body then
            // gets sliced into fragments at every internal branch point
            // it contains, and any fragment past the first one - which
            // nothing ever explicitly CALLs, since control reaches it
            // via fall-through or a jump instead - looks unreachable to
            // this pass's reachability scan and gets deleted, even
            // though it's still very much live code. This is what let
            // genuinely-called functions (confirmed cases: select_texture,
            // called directly from main(); initGame(), same) get swept
            // as "dead". Only a label actually starting with the
            // compiler's own function-label prefix is a real function
            // boundary - everything else, including '.'/'@'-prefixed
            // and '_return:'-containing labels, is left as part of
            // whatever function it's already inside.
            //
            // FIX (found while testing the fix above): the compiler
            // emits BOTH __function_NAME (entry) and
            // __function_NAME_return (an internal label used for
            // early-return jumps within that same function's own body -
            // see "Generated label names.txt") under the identical
            // __function_ prefix. A bare prefix check treats the
            // _return label as yet another separate trackable
            // "function" of its own - and since nothing ever CALLs a
            // _return label (it's only ever reached via an internal
            // jmp), it always looks unreachable and gets deleted,
            // breaking any early return in that function that jumps to
            // it. Confirmed directly: with only the prefix check, 309
            // labels were removed, and every single one of the sampled
            // ones was a _return label (e.g. addGameAerialBar_return),
            // not a genuinely dead function.
            size_t lbl_len = strlen(lbl);
            bool is_return_label = (lbl_len >= 8 && str_case_eq(lbl + lbl_len - 8, "_return:"));
            if (strncmp(lbl, "__function_", 11) != 0 || is_return_label) {
                curr = curr->next;
                continue;
            }

            char func_name[128] = {0}; 
            safe_str_copy(func_name, lbl, sizeof(func_name));
            char *colon = strchr(func_name, ':');
            if (colon) *colon = '\0';
            trim(func_name);

            AsmNode *scan = curr->next;
            AsmNode *end_of_func = curr;

            while (scan) {
                if (scan->type == OP_LABEL) {
                    char next_copy[8192];
                    safe_str_copy(next_copy, scan->raw, sizeof(next_copy));
                    char *next_lbl = trim(next_copy);
                    
                    // FIX: same criterion as the outer check above - a
                    // function's body only actually ends where the next
                    // *real* function begins, not at its own internal
                    // control-flow labels.
                    // FIX: a function's tracked range must also stop at
                    // data labels - __literal_string_N (string literal
                    // storage) and __global_NAME (global variable
                    // storage), per the compiler's own documented label
                    // scheme (CCompiler/Documents/Generated label
                    // names.txt) - not just at the next real function.
                    // Confirmed directly: __literal_string_1702, holding
                    // an actual string constant ('string "      "'),
                    // sits immediately after a `ret` with no enclosing
                    // __function_ label of its own - without this, it
                    // gets silently absorbed into whatever function
                    // happens to precede it in the file, and deleted
                    // right along with it if that function is dead,
                    // even though the string itself might be referenced
                    // and used from somewhere else entirely.
                    // __global_scope_initialization is the one
                    // exception - it's a real, called code block (see
                    // VirconCEmitter.cpp) despite the __global_ prefix,
                    // not a data label, so it's deliberately excluded
                    // from this stop condition and remains available to
                    // be absorbed like ordinary internal code would be.
                    //
                    // FIX (same as the outer check above): only stop at
                    // an actual function *entry* label, not its
                    // _return label, which shares the __function_
                    // prefix but is internal to that same function.
                    size_t next_lbl_len = strlen(next_lbl);
                    bool next_is_return = (next_lbl_len >= 8 && str_case_eq(next_lbl + next_lbl_len - 8, "_return:"));
                    if ((strncmp(next_lbl, "__function_", 11) == 0 && !next_is_return) ||
                        strncmp(next_lbl, "__literal_", 10) == 0 ||
                        (strncmp(next_lbl, "__global_", 9) == 0 &&
                         !str_case_eq(next_lbl, "__global_scope_initialization:"))) {
                        break;
                    }
                }
                end_of_func = scan;
                scan = scan->next;
            }

            if (func_count < MAX_FUNCTIONS) {
                safe_str_copy(funcs[func_count].name, func_name, sizeof(funcs[func_count].name));
                funcs[func_count].start_node = curr;
                funcs[func_count].end_node = end_of_func; 
                funcs[func_count].reachable = false;
                func_count++;

                curr = scan; 
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
        if (str_case_eq(funcs[i].name, "__boot_vector")                ||
            str_case_eq(funcs[i].name, "__function_main")              ||
            str_case_eq(funcs[i].name, "main")                         ||
            str_case_eq(funcs[i].name, "_start")                       ||
            str_case_eq(funcs[i].name, "start")                        ||
            str_case_eq(funcs[i].name, "__start")                      ||
            str_case_eq(funcs[i].name, "__init_globals")               ||
            str_case_eq(funcs[i].name, "__function_init")              ||
            str_case_eq(funcs[i].name, "__global_scope_initialization")||
            strstr(funcs[i].name, "global_scope") != NULL              ||
            strstr(funcs[i].name, "ISR")          != NULL              ||
            strstr(funcs[i].name, "interrupt")    != NULL)
        {
            funcs[i].reachable = true;
            if (worklist_size < MAX_FUNCTIONS) {
                safe_str_copy(worklist[worklist_size++], funcs[i].name, sizeof(worklist[0]));
            }
        }
    }

    if (worklist_size == 0 && func_count > 0) {
        funcs[0].reachable = true;
        safe_str_copy(worklist[worklist_size++], funcs[0].name, sizeof(worklist[0]));
    }

    // ----------------------------------------------------------------
    // 4. Global Scan for `pointer` Directives
    // ----------------------------------------------------------------
    for (AsmNode *node = head; node != NULL; node = node->next) {
        if (str_case_eq(node->mnemonic, "pointer")) {
            char args_copy[8192];
            safe_str_copy(args_copy, node->raw, sizeof(args_copy));
            
            char *token = strtok(args_copy, " ,\t\n\r");
            if (token && str_case_eq(token, "pointer")) {
                token = strtok(NULL, " ,\t\n\r");
                while (token != NULL) {
                    for (int f = 0; f < func_count; f++) {
                        if (str_case_eq(funcs[f].name, token) && !funcs[f].reachable) {
                            funcs[f].reachable = true;
                            if (worklist_size < MAX_FUNCTIONS) {
                                safe_str_copy(worklist[worklist_size++], funcs[f].name, sizeof(worklist[0]));
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
        safe_str_copy(current_label, worklist[--worklist_size], sizeof(current_label));

        FunctionDef *fn = NULL;
        for (int i = 0; i < func_count; i++) {
            if (str_case_eq(funcs[i].name, current_label)) {
                fn = &funcs[i];
                break;
            }
        }
        if (!fn) continue;

        for (AsmNode *node = fn->start_node; node != NULL; node = node->next) {
            if (node->type == OP_LABEL || (node->type == OP_OTHER && node->raw[0] == ';')) {
                if (node == fn->end_node) break;
                continue;
            }

            char *op_dst = trim(node->dst_op.raw);
            char *op_src = trim(node->src_op.raw);

            for (int f = 0; f < func_count; f++) {
                if (funcs[f].reachable) continue; 

                if ((strlen(op_dst) > 0 && str_case_eq(funcs[f].name, op_dst)) ||
                    (strlen(op_src) > 0 && str_case_eq(funcs[f].name, op_src))) 
                {
                    funcs[f].reachable = true;
                    if (worklist_size < MAX_FUNCTIONS) {
                        safe_str_copy(worklist[worklist_size++], funcs[f].name, sizeof(worklist[0]));
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
            safe_str_copy(lbl, curr->raw, sizeof(lbl));
            char *colon = strchr(lbl, ':');
            if (colon) *colon = '\0';

            if (pending_label_count < 8) {
                safe_str_copy(pending_labels[pending_label_count++], lbl, sizeof(pending_labels[0]));
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
                safe_str_copy(current_block->labels[l], pending_labels[l], sizeof(current_block->labels[l]));
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
        // FIX: a CALL can overwrite any register - at minimum R0, the
        // return value register, for any non-void function, and this
        // pass has no interprocedural knowledge of which (if any)
        // other registers a given callee happens to leave untouched.
        // Without this, whatever constant was known to be in a
        // register *before* the call is incorrectly treated as still
        // being there *after* it - confirmed directly against real
        // compiled output: a fresh function's first real computation,
        // reading pow()'s actual return value from R0 immediately after
        // calling it, was folded to a hardcoded 0 pulled from
        // completely unrelated code elsewhere in the program. This is
        // the single most dangerous bug found in this tool - it
        // produces no compile or assemble error anywhere, just silently
        // wrong output.
        if (str_case_eq(node->mnemonic, "CALL")) {
            for (int r = 0; r < 16; r++) {
                current.regs[r] = (RegState){VAL_BOTTOM, 0};
            }
            if (node == block->last_ins) break;
            continue;
        }

        // FIX: both the MOV and IADD cases below previously keyed off
        // node->dst_op.reg without first checking node->dst_op.mode. The
        // operand parser fills in .reg with the *base register name* for
        // an indirect operand too (e.g. "[BP+2]" parses to mode=INDIRECT,
        // reg="BP") - it is not exclusive to real register destinations.
        // So an indirect store like "MOV [BP+2], R0" was being read as if
        // it were "MOV BP, R0": get_reg_index("BP") happily returns 14,
        // and the tracked constant belonging to R0 got smeared onto BP's
        // slot - even though the instruction never writes to BP itself,
        // only to the memory address BP happens to be pointing at. Since
        // BP is the base register for almost every parameter/local access
        // the real compiler emits, this reliably poisoned BP (and by the
        // same mechanism, SP) with whatever constant last happened to be
        // in the source register of a nearby indirect store - confirmed
        // directly: "mov R0, -1" followed later by "mov [BP+2], R0" then
        // "mov SP, BP" folded to the literal "MOV SP, 0xFFFFFFFF", with
        // no relationship whatsoever to the real stack pointer. Gating
        // both cases on dst_op.mode == MODE_REG restores the correct
        // behavior: an indirect store writes to memory, not to its base
        // register, so it should update no register's tracked state at
        // all.
        if (node->type == OP_MOV && node->dst_op.mode == MODE_REG) {
            int dst_reg = get_reg_index(node->dst_op.reg);
            if (dst_reg >= 0) {
                // FIX: a float immediate (e.g. "0.500000") must not be
                // tracked as VAL_CONST - node->src_op.immediate is 0 for
                // every float literal (see parse_operand), which is
                // correct only for the literal 0.0 and silently wrong
                // for every other float value. Treating it as BOTTOM
                // (unknown) here is always safe; treating it as a
                // trustworthy constant is what let a corrupted 0
                // overwrite a real 0.5 volume value elsewhere in this
                // same pass.
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
        else if (node->type == OP_IADD && node->dst_op.mode == MODE_REG) {
            int dst_reg = get_reg_index(node->dst_op.reg);
            if (dst_reg >= 0) {
                RegState dst_st = current.regs[dst_reg];
                // FIX: same float-immediate guard as the MOV case above.
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
        else if (node->has_dst && node->dst_op.mode == MODE_REG && node->type != OP_PUSH) {
            // FIX: the CALL fix above addressed one specific instance of
            // a broader problem - this transfer function only knows how
            // to compute a definite new state for MOV and IADD; every
            // other instruction that writes to a register (ISUB, IMUL,
            // IDIV, IN - reading a hardware port, FADD/FSUB/etc, CIF/CFI,
            // comparison results, ...) fell through with no case at all,
            // silently leaving that register's old tracked state
            // untouched - exactly the same bug as the CALL case, just
            // not limited to calls. Any instruction that writes to a
            // register and isn't one of the two cases above must be
            // treated the same way CALL is: the old value cannot
            // possibly still be there once something else has written
            // to it, whether or not this pass understands what the new
            // value actually is. PUSH is the one exception worth
            // carving out explicitly rather than just accepting the
            // conservative cost: "PUSH R1" reads R1 to write it to the
            // stack, it doesn't modify R1 itself, so invalidating it
            // would only cost optimization opportunities for no
            // correctness benefit.
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
            // FIX: same reasoning as apply_transfer_function above -
            // this function has its own, separate copy of the register-
            // constant tracking (current.regs[]), used for the actual
            // fold/rewrite rather than just the propagation analysis,
            // and needs the identical CALL-invalidation or the bug
            // isn't actually fixed even with the other copy corrected -
            // this is the copy that performs the unsafe rewrite.
            if (str_case_eq(node->mnemonic, "CALL")) {
                for (int r = 0; r < 16; r++) {
                    current.regs[r] = (RegState){VAL_BOTTOM, 0};
                }
                if (node == block->last_ins) break;
                continue;
            }

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

            // FIX: same indirect-operand bug as apply_transfer_function
            // above - node->dst_op.reg holds the base register name for
            // an indirect operand too ("[BP+2]" -> mode=INDIRECT,
            // reg="BP"), so this must gate on dst_op.mode == MODE_REG or
            // an indirect store like "MOV [BP+2], R0" gets misread as a
            // real write to BP, smearing R0's constant onto BP's tracked
            // state. This is the copy that performs the actual unsafe
            // rewrite, so leaving it unguarded here reproduces the bug
            // even with apply_transfer_function's copy fixed - confirmed
            // directly: a minimal "push BP / mov BP,SP / mov R0,-1 /
            // mov [BP+2],R0 / mov SP,BP / pop BP / ret" input folded the
            // final "mov SP, BP" to the literal "MOV SP, 0xFFFFFFFF".
            if (node->type == OP_MOV && node->dst_op.mode == MODE_REG) {
                int dst_reg = get_reg_index(node->dst_op.reg);
                if (dst_reg >= 0) {
                    // FIX: same float-immediate guard as
                    // apply_transfer_function - a float literal (e.g.
                    // "0.500000") parses with immediate=0 (see
                    // parse_operand), which is only correct for the
                    // literal 0.0 and wrong for every other float value.
                    // Tracking it as VAL_CONST here is what let this
                    // exact pass fold a later register read of it into a
                    // bogus "MOV reg, 0x0", silently zeroing a real
                    // runtime value (confirmed directly: an SPU channel
                    // volume argument of 0.5 got replaced with 0 this
                    // way, in a real compiled program, silencing audio
                    // with no build error anywhere).
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
            // FIX: this copy of the tracking had no IADD case at all
            // (unlike apply_transfer_function above, which does) - an
            // IADD left current.regs[] completely untouched here,
            // neither updated with a newly-computed constant nor
            // invalidated, silently drifting out of sync with what the
            // separate propagation analysis actually determined. Also
            // gated on dst_op.mode == MODE_REG for the same reason as
            // the MOV case just above (the real compiler never emits an
            // indirect-destination IADD, but nothing should rely on
            // that).
            else if (node->type == OP_IADD && node->dst_op.mode == MODE_REG) {
                int dst_reg = get_reg_index(node->dst_op.reg);
                if (dst_reg >= 0) {
                    RegState dst_st = current.regs[dst_reg];
                    // FIX: same float-immediate guard as the MOV case
                    // just above.
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
            // FIX: same generalized catch-all as apply_transfer_function
            // above - any other instruction writing to a register
            // (ISUB, IMUL, IDIV, IN, FADD/etc, CIF/CFI, comparisons,
            // ...) must invalidate that register rather than silently
            // leaving stale tracked state in place. PUSH excluded for
            // the same reason as above - it reads its register operand,
            // it doesn't modify it.
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

// Forward declaration for local optimization passes defined later
int pass_strength_reduction(AsmNode *head);

// -------------------------------------------------------------------
// Main Entry Point
// -------------------------------------------------------------------
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf (stdout, "Usage: %s <input.asm> [output.asm] [options]\n", argv[0]);
        fprintf (stdout, "Options:\n");
        fprintf (stdout, "  -v                           Verbose output (show pass statistics)\n");
        fprintf (stdout, "  --dot <cfg.dot>              Export Control Flow Graph to DOT format\n");
        fprintf (stdout, "  -O0                          Disable all optimizations [default]\n");
        fprintf (stdout, "  -O1                          Enables:\n");
        fprintf (stdout, "     -fopt_peephole            Enables peephole optimizations\n");
        fprintf (stdout, "     -fopt_algebraic           Enables algebraic simplifications\n");
        fprintf (stdout, "     -fopt_forwarding          Enables store-to-load forwarding\n");
        fprintf (stdout, "     -fopt_jump_next           Enables jump next\n");
        fprintf (stdout, "     -fopt_redundant_movs      Enables redundant movs\n");
        fprintf (stdout, "     -fopt_combine_immediates  Enables combine immediates\n");
        fprintf (stdout, "     -fopt_strength_reduction  Enables strength reduction\n");
        fprintf (stdout, "  -O2                          -O1 + these:\n");
        fprintf (stdout, "     -fopt_dce                 Enables dead function elimination\n");
        fprintf (stdout, "     -fopt_constant_folding    Enables constant folding\n");
        fprintf (stdout, "  -O3                          -O2 + these (aggressive):\n");
        fprintf (stdout, "     -fopt_inline              Enables function inlining\n\n");
        return (1);
    }

    char *inFile = argv[1];
    char outFile[256] = {0};
    char dotFile[256] = {0};

    OptConfig config = {
        .verbose = false,
        .enable_peephole = false,
        .enable_algebraic = false,
        .enable_forwarding = false,
        .enable_inline = false,
        .enable_dce = false,
        .enable_constant_folding = false,
		.enable_jump_next = false,
		.enable_redundant_movs = false,
		.enable_combine_immediates = false,
		.enable_strength_reduction = false
    };

    int  out_idx = 0;
	int  opt_count  = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--dot") == 0 && i + 1 < argc) {
            safe_str_copy(dotFile, argv[i+1], sizeof(dotFile));
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
            config.enable_jump_next = false;
            config.enable_redundant_movs = false;
            config.enable_combine_immediates = false;
            config.enable_strength_reduction = false;
		} else if (strcmp(argv[i], "-O1") == 0) {
            config.enable_peephole = true;
            config.enable_algebraic = true;
            config.enable_forwarding = true;
            config.enable_jump_next = true;          // Added
            config.enable_redundant_movs = true;     // Added
            config.enable_combine_immediates = true; // Added
            config.enable_strength_reduction = true; // Added
            config.enable_inline = false;
            config.enable_dce = false;
            config.enable_constant_folding = false;
            opt_count = 7; // Updated count
        } else if (strcmp(argv[i], "-O2") == 0) {
            config.enable_peephole = true;
            config.enable_algebraic = true;
            config.enable_forwarding = true;
            config.enable_jump_next = true;          // Added
            config.enable_redundant_movs = true;     // Added
            config.enable_combine_immediates = true; // Added
            config.enable_strength_reduction = true; // Added
            config.enable_inline = false;
            config.enable_dce = true;
            config.enable_constant_folding = true;
            opt_count = 9; // Updated count
        } else if (strcmp(argv[i], "-O3") == 0) {
            config.enable_peephole = true;
            config.enable_algebraic = true;
            config.enable_forwarding = true;
            config.enable_jump_next = true;          // Added
            config.enable_redundant_movs = true;     // Added
            config.enable_combine_immediates = true; // Added
            config.enable_strength_reduction = true; // Added
            config.enable_inline = true;
            config.enable_dce = true;
            config.enable_constant_folding = true;
            opt_count = 10; // Updated count
        // Individual Flags:
        } else if (strcmp(argv[i], "-fopt_jump_next") == 0) {
            config.enable_jump_next = true;
            opt_count++;
        } else if (strcmp(argv[i], "-fopt_redundant_movs") == 0) {
            config.enable_redundant_movs = true;
            opt_count++;
        } else if (strcmp(argv[i], "-fopt_combine_immediates") == 0) {
            config.enable_combine_immediates = true;
            opt_count++;
        } else if (strcmp(argv[i], "-fopt_strength_reduction") == 0) {
            config.enable_strength_reduction = true;
            opt_count++;
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
        } else if (strncmp(argv[i], "-finline-max=", 13) == 0) {
            // DIAGNOSTIC ONLY - see g_inline_call_limit declaration.
            g_inline_call_limit = atoi(argv[i] + 13);
        } else if (strncmp(argv[i], "-finline-exclude=", 17) == 0) {
            // DIAGNOSTIC ONLY - see g_inline_exclude_name declaration.
            safe_str_copy(g_inline_exclude_name, argv[i] + 17, sizeof(g_inline_exclude_name));
        } else if (strcmp(argv[i], "-fopt_dce") == 0) {
            config.enable_dce = true;
			opt_count = opt_count + 1;
        } else if (strcmp(argv[i], "-fopt_constant_folding") == 0) {
            config.enable_constant_folding = true;
			opt_count = opt_count + 1;
        } else if (out_idx == 0 && argv[i][0] != '-') {
            safe_str_copy(outFile, argv[i], sizeof(outFile));
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
        if (config.enable_constant_folding)   fprintf (stdout, "  - constant_folding\n");
		if (config.enable_jump_next)          fprintf(stdout, "  - jump_next\n");
        if (config.enable_redundant_movs)     fprintf(stdout, "  - redundant_movs\n");
        if (config.enable_combine_immediates) fprintf(stdout, "  - combine_immediates\n");
        if (config.enable_strength_reduction) fprintf(stdout, "  - strength_reduction\n");
    }

    if (strlen(outFile) == 0) {
        safe_str_copy(outFile, inFile, sizeof(outFile));
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
        int j_opts = 0, m_opts = 0, c_opts = 0, s_opts = 0; // Added s_opts

        if (config.enable_peephole)           p_opts = pass_peephole_window2(program_ast);
        if (config.enable_algebraic)          a_opts = pass_algebraic_simplifications(program_ast);
        if (config.enable_forwarding)         f_opts = pass_store_to_load_forwarding(program_ast);
        if (config.enable_jump_next)          j_opts = pass_redundant_jumps(program_ast);
        if (config.enable_redundant_movs)     m_opts = pass_redundant_movs(program_ast);
        if (config.enable_combine_immediates) c_opts = pass_combine_immediates(program_ast);
        if (config.enable_strength_reduction) s_opts = pass_strength_reduction(program_ast); // Added
        if (config.enable_inline)             i_opts = pass_inline_trivial_functions(program_ast);
        if (config.enable_dce)                d_opts = pass_dead_function_elimination(program_ast);

        opts_in_pass = p_opts + a_opts + f_opts + j_opts + m_opts + c_opts + s_opts + i_opts + d_opts;
        total_opts += opts_in_pass;
        passes++;

        if (config.verbose && opts_in_pass > 0) {
            printf("Pass %d applied %d optimizations:\n", passes, opts_in_pass);
            if (p_opts > 0) printf("  - Peephole: %d\n", p_opts);
            if (a_opts > 0) printf("  - Algebraic: %d\n", a_opts);
            if (f_opts > 0) printf("  - Forwarding: %d\n", f_opts);
            if (j_opts > 0) printf("  - Redundant jumps removed: %d\n", j_opts);
            if (m_opts > 0) printf("  - Redundant moves removed: %d\n", m_opts);
            if (c_opts > 0) printf("  - Immediates combined: %d\n", c_opts);
            if (s_opts > 0) printf("  - Strength reductions: %d\n", s_opts);
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
        if (!cfg) cfg = build_cfg(program_ast); 
        export_cfg_to_dot(dotFile, cfg);
        if (config.verbose) printf("CFG exported to '%s'.\n", dotFile);
    }

    if (config.verbose) {
        printf("\nOptimization complete: %d total optimizations applied.\n", total_opts);
    }

    write_vircon32_asm(outFile, program_ast);

    if (cfg) free_cfg(cfg);
    free(program_ast);
    return 0;
}

// Helper: Check if a positive integer is a power of 2 (2, 4, 8, 16...)
static inline bool is_power_of_two(int x) {
    return (x > 0) && ((x & (x - 1)) == 0);
}

// Helper: Calculate log2 of a power-of-two integer (determines shift amount)
static inline int get_log2(int x) {
    int log = 0;
    while (x > 1) {
        x >>= 1;
        log++;
    }
    return log;
}

// -------------------------------------------------------------------
// Pass: Strength Reduction (Multiplication & Division)
// -------------------------------------------------------------------
int pass_strength_reduction(AsmNode *head) {
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr) {
        // --- 1. Integer Multiplication (IMUL) ---
        if (curr->type == OP_IMUL && curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float) {
            int val = curr->src_op.immediate;

            // Case A: Multiply by 0 -> Replace with MOV dst, 0
            if (val == 0) {
                curr->type = OP_MOV;
                safe_str_copy(curr->mnemonic, "MOV", sizeof(curr->mnemonic));
                snprintf(curr->raw, sizeof(curr->raw), "    MOV %s, 0", curr->dst_op.raw);
                optimizations++;
                curr = curr->next;
                continue;
            }

            // Case B: Multiply by 1 -> Identity operation (Remove completely!)
            if (val == 1) {
                AsmNode *to_remove = curr;
                curr = curr->next;
                remove_node(to_remove);
                optimizations++;
                continue;
            }

            // Case C: Multiply by 2 -> Replace with IADD dst, dst
            if (val == 2) {
                curr->type = OP_IADD;
                safe_str_copy(curr->mnemonic, "IADD", sizeof(curr->mnemonic));
                curr->src_op.mode = MODE_REG;
                safe_str_copy(curr->src_op.reg, curr->dst_op.reg, sizeof(curr->src_op.reg));
                safe_str_copy(curr->src_op.raw, curr->dst_op.raw, sizeof(curr->src_op.raw));
                snprintf(curr->raw, sizeof(curr->raw), "    IADD %s, %s", curr->dst_op.raw, curr->dst_op.raw);
                optimizations++;
                curr = curr->next;
                continue;
            }

            // FIX: removed the IMUL-by-power-of-2 -> SHL case that was
            // here. It was mathematically sound (SHL is a real Vircon32
            // instruction, and left-shifting a signed int by N is
            // exactly multiplication by 2^N with no sign caveats,
            // unlike the IDIV case below), but pointless on this
            // specific hardware - IMUL and SHL both cost exactly 1
            // cycle here, so "reducing" one to the other buys nothing
            // and just adds a code path with no benefit to verify.
        }

        // --- 2. Integer Division (IDIV) ---
        if (curr->type == OP_IDIV && curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float) {
            int val = curr->src_op.immediate;

            // Case A: Divide by 1 -> Identity operation (Remove completely!)
            if (val == 1) {
                AsmNode *to_remove = curr;
                curr = curr->next;
                remove_node(to_remove);
                optimizations++;
                continue;
            }

            // FIX: removed the IDIV-by-power-of-2 -> SHR case that was
            // here. Two separate, independent problems, either one
            // alone would be enough to remove this:
            //   1. Vircon32 has no right-shift instruction at all -
            //      only SHL is real (confirmed against
            //      VIRCON32_C_DIALECT.md's own hardware instruction
            //      table). The emitted "SHR" mnemonic doesn't exist on
            //      this hardware; the real assembler rejects it
            //      outright with a parser error.
            //   2. Even a correct right-shift would be the *wrong
            //      value* for negative operands: C's integer division
            //      truncates toward zero (-7 / 2 == -3), while
            //      arithmetic right shift rounds toward negative
            //      infinity (-7 >> 1 == -4 in two's complement) - they
            //      disagree on every negative odd numerator. A correct
            //      fix needs a sign-dependent rounding adjustment
            //      before the shift, which is more instructions than
            //      the IDIV it would replace - and per IDIV already
            //      being 1 cycle on this hardware, that correct version
            //      would be strictly worse than leaving IDIV alone, so
            //      there's no version of this case worth keeping here.
        }

        curr = curr->next;
    }
    return optimizations;
}
