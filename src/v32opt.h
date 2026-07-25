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
bool     is_register_read         (AsmNode *, const char *);
bool     is_live_out_register     (const char *);

// stack optimizations
//
int      optimize_frame_pointers  (AsmNode *); // -O2

// stack helpers
//
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
