#ifndef __V32OPT_H
#define __V32OPT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>

#define MAX_INLINE_CANDIDATES 64
#define MAX_BODY_INS 8
#define MAX_FUNCTIONS 4096

// -------------------------------------------------------------------
// Enums & Data Structures
// -------------------------------------------------------------------
#include <stdbool.h>
#include <stdint.h>

// Behavioral categories based on Vircon32 instruction operand semantics
typedef enum {
    INST_NORMAL,      // Standard explicit dst/src operations (MOV, IADD, etc.)
    INST_UNARY_RMW,   // Read-Modify-Write on a single operand (CIB, NOT, SIN, etc.)
    INST_BRANCH,      // Control flow and program redirection (JMP, JT, CALL, HLT, etc.)
    INST_STRING,      // Hardware iterative string ops (MOVS, SETS, CMPS)
    INST_STACK        // Hardware stack ops (PUSH, POP)
} InstBehavior;

typedef struct {
    const char *mnemonic;
    InstBehavior behavior;
    bool is_control_flow;
    uint32_t implicit_read_mask;  // Bitmask of hardware registers implicitly read
    uint32_t implicit_write_mask; // Bitmask of hardware registers implicitly written
} InstructionMeta;

// Vircon32 Register Bitmasks (R0 is bit 0 ... R15 is bit 15)
#define MASK_R11_CR (1 << 11) // R11: String Count Register (CR)
#define MASK_R12_SR (1 << 12) // R12: String Source Register (SR)
#define MASK_R13_DR (1 << 13) // R13: String Destination Register (DR)
#define MASK_R15_SP (1 << 15) // R15: Stack Pointer (SP)

#define MASK_STRING_ALL (MASK_R11_CR | MASK_R12_SR | MASK_R13_DR)

// Exhaustive table of all 64 Vircon32 CPU Instructions (Opcodes 0x00 to 0x3F)
static const InstructionMeta VIRCON_INST_TABLE[] = {
    // Control & Branch Instructions (Opcodes 0x00 - 0x06)
    { "HLT",   INST_BRANCH,    true,  0, 0 },                                  // 0x00: Halt CPU
    { "WAIT",  INST_BRANCH,    true,  0, 0 },                                  // 0x01: Wait for next frame
    { "JMP",   INST_BRANCH,    true,  0, 0 },                                  // 0x02: Unconditional jump
    { "CALL",  INST_BRANCH,    true,  MASK_R15_SP, MASK_R15_SP },              // 0x03: Call subroutine (pushes IP to stack)
    { "RET",   INST_BRANCH,    true,  MASK_R15_SP, MASK_R15_SP },              // 0x04: Return from subroutine (pops IP from stack)
    { "JT",    INST_BRANCH,    true,  0, 0 },                                  // 0x05: Jump if true (non-zero)
    { "JF",    INST_BRANCH,    true,  0, 0 },                                  // 0x06: Jump if false (zero)

    // Comparison Instructions (Opcodes 0x07 - 0x12)
    { "IEQ",   INST_NORMAL,    false, 0, 0 },                                  // 0x07: Integer equal
    { "INE",   INST_NORMAL,    false, 0, 0 },                                  // 0x08: Integer not equal
    { "IGT",   INST_NORMAL,    false, 0, 0 },                                  // 0x09: Integer greater than
    { "IGE",   INST_NORMAL,    false, 0, 0 },                                  // 0x0A: Integer greater than or equal
    { "ILT",   INST_NORMAL,    false, 0, 0 },                                  // 0x0B: Integer less than
    { "ILE",   INST_NORMAL,    false, 0, 0 },                                  // 0x0C: Integer less than or equal
    { "FEQ",   INST_NORMAL,    false, 0, 0 },                                  // 0x0D: Float equal
    { "FNE",   INST_NORMAL,    false, 0, 0 },                                  // 0x0E: Float not equal
    { "FGT",   INST_NORMAL,    false, 0, 0 },                                  // 0x0F: Float greater than
    { "FGE",   INST_NORMAL,    false, 0, 0 },                                  // 0x10: Float greater than or equal
    { "FLT",   INST_NORMAL,    false, 0, 0 },                                  // 0x11: Float less than
    { "FLE",   INST_NORMAL,    false, 0, 0 },                                  // 0x12: Float less than or equal

    // Data & Memory Instructions (Opcodes 0x13 - 0x1B)
    { "MOV",   INST_NORMAL,    false, 0, 0 },                                  // 0x13: Copy data
    { "LEA",   INST_NORMAL,    false, 0, 0 },                                  // 0x14: Load effective address
    { "PUSH",  INST_STACK,     false, MASK_R15_SP, MASK_R15_SP },              // 0x15: Push to stack (decrements SP)
    { "POP",   INST_STACK,     false, MASK_R15_SP, MASK_R15_SP },              // 0x16: Pop from stack (increments SP)
    { "IN",    INST_NORMAL,    false, 0, 0 },                                  // 0x17: Read from I/O port
    { "OUT",   INST_NORMAL,    false, 0, 0 },                                  // 0x18: Write to I/O port
    { "MOVS",  INST_STRING,    false, MASK_STRING_ALL, MASK_STRING_ALL },      // 0x19: Copy string (loops CR, reads SR/DR, modifies DR/SR/CR)
    { "SETS",  INST_STRING,    false, MASK_STRING_ALL, MASK_R13_DR | MASK_R11_CR }, // 0x1A: Set string (loops CR, reads SR/DR, modifies DR/CR)
    { "CMPS",  INST_STRING,    false, MASK_STRING_ALL, MASK_STRING_ALL },      // 0x1B: Compare string (loops CR, reads/modifies DR/SR/CR)

    // Conversion Instructions (Opcodes 0x1C - 0x1F)
    { "CIF",   INST_UNARY_RMW, false, 0, 0 },                                  // 0x1C: Convert integer to float
    { "CFI",   INST_UNARY_RMW, false, 0, 0 },                                  // 0x1D: Convert float to integer
    { "CIB",   INST_UNARY_RMW, false, 0, 0 },                                  // 0x1E: Convert integer to boolean
    { "CFB",   INST_UNARY_RMW, false, 0, 0 },                                  // 0x1F: Convert float to boolean

    // Logic Instructions (Opcodes 0x20 - 0x25)
    { "NOT",   INST_UNARY_RMW, false, 0, 0 },                                  // 0x20: Bitwise NOT
    { "AND",   INST_NORMAL,    false, 0, 0 },                                  // 0x21: Bitwise AND
    { "OR",    INST_NORMAL,    false, 0, 0 },                                  // 0x22: Bitwise inclusive OR
    { "XOR",   INST_NORMAL,    false, 0, 0 },                                  // 0x23: Bitwise exclusive OR
    { "BNOT",  INST_UNARY_RMW, false, 0, 0 },                                  // 0x24: Boolean NOT
    { "SHL",   INST_NORMAL,    false, 0, 0 },                                  // 0x25: Bitwise shift left

    // Integer Arithmetic Instructions (Opcodes 0x26 - 0x2E)
    { "IADD",  INST_NORMAL,    false, 0, 0 },                                  // 0x26: Integer addition
    { "ISUB",  INST_NORMAL,    false, 0, 0 },                                  // 0x27: Integer subtraction
    { "IMUL",  INST_NORMAL,    false, 0, 0 },                                  // 0x28: Integer multiplication
    { "IDIV",  INST_NORMAL,    false, 0, 0 },                                  // 0x29: Integer division
    { "IMOD",  INST_NORMAL,    false, 0, 0 },                                  // 0x2A: Integer modulus
    { "ISGN",  INST_UNARY_RMW, false, 0, 0 },                                  // 0x2B: Integer sign toggle
    { "IMIN",  INST_NORMAL,    false, 0, 0 },                                  // 0x2C: Integer minimum
    { "IMAX",  INST_NORMAL,    false, 0, 0 },                                  // 0x2D: Integer maximum
    { "IABS",  INST_UNARY_RMW, false, 0, 0 },                                  // 0x2E: Integer absolute value

    // Floating Point Arithmetic Instructions (Opcodes 0x2F - 0x37)
    { "FADD",  INST_NORMAL,    false, 0, 0 },                                  // 0x2F: Float addition
    { "FSUB",  INST_NORMAL,    false, 0, 0 },                                  // 0x30: Float subtraction
    { "FMUL",  INST_NORMAL,    false, 0, 0 },                                  // 0x31: Float multiplication
    { "FDIV",  INST_NORMAL,    false, 0, 0 },                                  // 0x32: Float division
    { "FMOD",  INST_NORMAL,    false, 0, 0 },                                  // 0x33: Float modulus
    { "FSGN",  INST_UNARY_RMW, false, 0, 0 },                                  // 0x34: Float sign toggle
    { "FMIN",  INST_NORMAL,    false, 0, 0 },                                  // 0x35: Float minimum
    { "FMAX",  INST_NORMAL,    false, 0, 0 },                                  // 0x36: Float maximum
    { "FABS",  INST_UNARY_RMW, false, 0, 0 },                                  // 0x37: Float absolute value

    // Float Math Instructions (Opcodes 0x38 - 0x3F)
    { "FLR",   INST_UNARY_RMW, false, 0, 0 },                                  // 0x38: Float floor
    { "CEIL",  INST_UNARY_RMW, false, 0, 0 },                                  // 0x39: Float ceiling
    { "ROUND", INST_UNARY_RMW, false, 0, 0 },                                  // 0x3A: Float rounding
    { "SIN",   INST_UNARY_RMW, false, 0, 0 },                                  // 0x3B: Float sine
    { "ACOS",  INST_UNARY_RMW, false, 0, 0 },                                  // 0x3C: Float arc cosine
    { "ATAN2", INST_NORMAL,    false, 0, 0 },                                  // 0x3D: Float arc tangent (takes 2 operands)
    { "LOG",   INST_UNARY_RMW, false, 0, 0 },                                  // 0x3E: Float natural log
    { "POW",   INST_NORMAL,    false, 0, 0 }                                   // 0x3F: Float power (takes 2 operands)
};

typedef enum {
    OP_MOV, OP_IADD, OP_ISUB, OP_IMUL, OP_IDIV, OP_IEQ, OP_INE, OP_IN, 
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
    bool is_float;
} Operand;

typedef struct AsmNode {
    int type;                   // e.g., OP_INST, OP_LABEL, etc.
    char raw[8192];      
    char *mnemonic;             // e.g., "MOV", "CIB", "JT"
    Operand dst_op;             // First operand
    Operand src_op;             // Second operand
    bool has_dst;
    bool has_src;

    // --> ADD THIS LINE: <--
    const InstructionMeta *meta; // Pointer to the architectural instruction properties

    struct AsmNode *next;
    struct AsmNode *prev;
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
    bool enable_peephole_pairs;
    bool enable_peephole_algebra;
    bool enable_peephole_forwarding;
    bool enable_peephole_jumps;
    bool enable_peephole_movs;
    bool enable_peephole_immediates;
    bool enable_peephole_reduce;
    bool enable_peephole_shifts;
    bool enable_peephole_dead_stores;
    bool enable_peephole_loads;
    bool enable_peephole_immediate_prop;
    bool enable_peephole_jmp_chain;
    bool enable_dce;
    bool enable_constant_folding;
    bool enable_inline;
    bool enable_promote_regs;
    bool enable_promote_leaf;
    bool enable_promote_loops;
    bool enable_omit_frame_pointers;
} OptConfig;

extern int g_inline_call_limit;
extern int g_inline_calls_so_far;
extern char g_inline_exclude_name[1024];

////////////////////////////////////////////////////////////////////////////////////////
//
// general utility function prototypes
//
void     safe_str_copy   (char       *, const char *, size_t);
char    *trim            (char       *);
bool     str_case_eq     (const char *, const char *);
int      get_reg_index   (const char *);
bool     is_power_of_two (int);
int      get_log2        (int);

Operand  parse_operand (const char *);
AsmNode *create_node   (const char *, OpType, const char *, const char *, const char *);
void     remove_node   (AsmNode    *);
AsmNode *clone_node    (AsmNode    *);

bool     is_unconditional_branch (AsmNode *);
bool     is_conditional_branch   (AsmNode *);
bool     is_branch_or_call       (AsmNode *);

// parsing / writing functions
//
AsmNode *parse_vircon32_asm (const char *);
void     write_vircon32_asm (const char *, AsmNode *);

// peephole, algebraic, forwarding, jump_next, redundant_movs,
// combine_immediates, strength_reduction, inline, dce,
// constant_folding, promote_regs, promote_leaf, promote_loops

//////////////////////////////////////////////////////////////////////////////
//
// peephole optimizations
//
int      peephole_pairs           (AsmNode *);
int      peephole_algebra         (AsmNode *);
int      peephole_forwarding      (AsmNode *);
int      peephole_jumps           (AsmNode *);
int      peephole_movs            (AsmNode *);
int      peephole_immediates      (AsmNode *);
int      peephole_reduce          (AsmNode *);
int      peephole_shifts          (AsmNode *);
int      peephole_dead_stores     (AsmNode *);
int      peephole_loads           (AsmNode *);
int      peephole_immediate_prop  (AsmNode *);
int      peephole_jmp_chain       (AsmNode *);

bool     modifies_register        (AsmNode *, const char *);
bool     is_control_flow_boundary (AsmNode *);
long     parse_imm_val            (const char *);
bool     is_string_instruction    (AsmNode *);
bool     is_register_read         (AsmNode *, const char *);
bool     is_live_out_register     (const char *);
bool     is_pure_reg_def          (AsmNode *);
bool     is_comment_or_empty      (AsmNode *);

const InstructionMeta* lookup_instruction_meta (const char *);

// helpers.c
//
bool  is_global_memory_clobber (AsmNode *);
bool  is_global_memory_read    (AsmNode *);

// stack optimizations
//
int      omit_frame_pointers      (AsmNode *); // -O2

// stack helpers
//
bool     is_reg_op                (AsmNode *, const char *);
bool     references_bp            (AsmNode *);

// inline optimization
//
int      inline_trivial_functions  (AsmNode *);

// dead function elimination
//
int      pass_dead_function_elimination (AsmNode *);

// Add to "control flow graph" section:
BasicBlock* find_block_by_label(ControlFlowGraph *, const char *);
ControlFlowGraph* build_cfg(AsmNode *);
void export_cfg_to_dot(const char *, ControlFlowGraph *);
void free_cfg(ControlFlowGraph *);

// Add to a new "global data-flow analysis" section:
RegState merge_reg(RegState, RegState);
bool apply_transfer_function(BasicBlock *);
void propagate_constants_cfg(ControlFlowGraph *);
int fold_constants_cfg(ControlFlowGraph *);

// control flow graph
//
void     add_edge (BasicBlock *, BasicBlock *);

////////////////////////////////////////////////////////////////////////////////////////
//
// promote.c function prototypes for register promotion optimizations
//
void  promote_operand_to_reg      (Operand    *, const char *);
bool  is_inside_loop              (const char *, AsmNode    *,    AsmNode *);
int   pass_promote_stack_slots    (AsmNode    *);
int   pass_promote_loop_registers (AsmNode    *);

#endif
