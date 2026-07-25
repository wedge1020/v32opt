#ifndef __V32OPT_H
#define __V32OPT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#define MAX_INLINE_CANDIDATES 64
#define MAX_BODY_INS 8
#define MAX_FUNCTIONS 4096

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
    bool enable_promote_regs;
    bool enable_promote_leaf;
    bool enable_promote_loops;
} OptConfig;

////////////////////////////////////////////////////////////////////////////////////////
//
// general utility function prototypes
//
void     safe_str_copy (char       *, const char *, size_t);
char    *trim          (char       *);
bool     str_case_eq   (const char *, const char *);
int      get_reg_index (const char *);

Operand  parse_operand (const char *);
AsmNode *create_node   (const char *, OpType, const char *, const char *, const char *);
void     remove_node   (AsmNode    *);
AsmNode *clone_node    (AsmNode    *);

// parsing / writing functions
//
AsmNode *parse_vircon32_asm (const char *);
void     write_vircon32_asm (const char *, AsmNode *);

// peephole optimizations
//
int      pass_peephole_window2          (AsmNode *);
int      pass_algebraic_simplifications (AsmNode *);
int      pass_store_to_load_forwarding  (AsmNode *);
int      pass_redundant_jumps           (AsmNode *);
int      pass_redundant_movs            (AsmNode *);
int      pass_combine_immediates        (AsmNode *);

// inline optimization
//
int      pass_inline_trivial_functions  (AsmNode *);

// dead function elimination
//
int      pass_dead_function_elimination (AsmNode *);

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
