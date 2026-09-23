#ifndef __V32OPT_ASM_H
#define __V32OPT_ASM_H

// ===========================================================================
// asm.h -- the Vircon32 assembly language model
// ---------------------------------------------------------------------------
// Everything that describes an assembly program as parsed text: the
// instruction (OpType) and addressing-mode vocabulary, the AsmNode linked
// list that the whole optimizer manipulates, the operand/string utilities
// shared by every pass (tools.c, helpers.c), the generic control-flow
// predicates, and the .asm file parser/writer.
// ===========================================================================

// ---------------------------------------------------------------------------
// Instruction set (OpType) -- one entry per Vircon32 mnemonic the parser
// recognizes, plus OP_OTHER (comments, blanks, directives) and OP_LABEL.
// ---------------------------------------------------------------------------
typedef enum
{
    OP_HLT   = 0,
    OP_WAIT,
    OP_JMP,
    OP_CALL,
    OP_RET,
    OP_JT,
    OP_JF,
    OP_IEQ,
    OP_INE,
    OP_IGT,
    OP_IGE,
    OP_ILT,
    OP_ILE,
    OP_FEQ,
    OP_FNE,
    OP_FGT,
    OP_FGE,
    OP_FLT,
    OP_FLE,
    OP_MOV,
    OP_LEA,
    OP_PUSH,
    OP_POP,
    OP_IN,
    OP_OUT,
    OP_MOVS,
    OP_SETS,
    OP_CMPS,
    OP_CIF,
    OP_CFI,
    OP_CIB,
    OP_CFB,
    OP_NOT,
    OP_AND,
    OP_OR,
    OP_XOR,
    OP_BNOT,
    OP_SHL,
    OP_IADD,
    OP_ISUB,
    OP_IMUL,
    OP_IDIV,
    OP_IMOD,
    OP_ISGN,
    OP_IMIN,
    OP_IMAX,
    OP_IABS,
    OP_FADD,
    OP_FSUB,
    OP_FMUL,
    OP_FDIV,
    OP_FMOD,
    OP_FSGN,
    OP_FMIN,
    OP_FMAX,
    OP_FABS,
    OP_FLR,
    OP_CEIL,
    OP_ROUND,
    OP_SIN,
    OP_ACOS,
    OP_ATAN2,
    OP_LOG,
    OP_POW,
    OP_OTHER,
    OP_LABEL
} OpType;

// ---------------------------------------------------------------------------
// Addressing modes for parsed operands
// ---------------------------------------------------------------------------
typedef enum {
    MODE_NONE,
    MODE_REG,         // e.g., R0, R1, SP, BP
    MODE_IMMEDIATE,   // e.g., 42, -10, 0x20, 0.500000, symbol names
    MODE_INDIRECT     // e.g., [R1], [BP+4], [BP-8]
} AddressingMode;

typedef struct {
    AddressingMode mode;
    char reg[32];
    int offset;
    int immediate;
    char raw[128];
    bool is_float;
    float float_value;
} Operand;

typedef struct AsmNode {
    OpType type;
    char raw[1024];              // was: char raw[8192];
    char mnemonic[32];

    Operand dst_op;
    Operand src_op;
    bool has_dst;
    bool has_src;

    struct AsmNode *prev;
    struct AsmNode *next;
} AsmNode;

// ---------------------------------------------------------------------------
// Node & operand construction/inspection (tools.c)
// ---------------------------------------------------------------------------
Operand  parse_operand (const char *);
AsmNode *create_node   (const char *, OpType, const char *, const char *, const char *);
void     remove_node   (AsmNode    *);
AsmNode *clone_node    (AsmNode    *);

// ---------------------------------------------------------------------------
// Generic string utilities (tools.c)
// ---------------------------------------------------------------------------
void  safe_str_copy           (char *, const char *, size_t);
char *trim                    (char *);
bool  str_case_eq             (const char *, const char *);
void  strip_comment_from_line (char *, const char    *, size_t);
void  normalize_whitespace    (char *, const char    *, size_t);
bool  is_power_of_two         (int);
int   get_log2                (int);

// ---------------------------------------------------------------------------
// Register & operand analysis (tools.c / helpers.c)
// ---------------------------------------------------------------------------
int   get_reg_index         (const char *);
bool  operands_equal        (const Operand *, const Operand *);
bool  is_numeric_immediate  (const Operand *);
long  parse_imm_val         (const char *);
void  get_label_name        (const AsmNode *, char *, size_t);

// ---------------------------------------------------------------------------
// Node traversal helpers (helpers.c)
// ---------------------------------------------------------------------------
AsmNode *skip_other_nodes         (AsmNode *);
AsmNode *skip_comments_and_blanks (AsmNode *);
AsmNode *next_non_other           (AsmNode *);
bool     is_register_operand      (const char *);
bool     is_immediate_string      (const char *);

// ---------------------------------------------------------------------------
// Control-flow & register-effect predicates (helpers.c / cfg.c)
// ---------------------------------------------------------------------------
bool     is_unconditional_branch     (AsmNode *);
bool     is_conditional_branch       (AsmNode *);
bool     is_branch_or_call           (AsmNode *);
bool     modifies_register           (AsmNode *, const char *);
bool     is_control_flow_boundary    (AsmNode *);
bool     is_dead_store_scan_boundary (AsmNode *);
bool     is_register_read            (AsmNode *, const char *);
bool     is_live_out_register        (const char *);

// ---------------------------------------------------------------------------
// Assembly file parser & writer (tools.c)
// ---------------------------------------------------------------------------
AsmNode *parse_vircon32_asm (const char *);
void     write_vircon32_asm (const char *, AsmNode *);

#endif
