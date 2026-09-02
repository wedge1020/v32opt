#ifndef __V32OPT_H
#define __V32OPT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include <getopt.h>

#define  MAX_OPTIMIZATION_ALGORITHMS 21
#define  MAX_INLINE_CANDIDATES 64
#define  MAX_BODY_INS 8
#define  MAX_FUNCTIONS 4096

// -------------------------------------------------------------------
// Enums & Data Structures
// -------------------------------------------------------------------

// Add at the top with other enums
typedef enum {
    LANG_C,      // Default: current v32opt behavior
    LANG_LUA,    // Lua mode: aware of boxed type system
    LANG_MAX
} LangMode;

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

typedef enum
{
    OPT_PEEPHOLE_ALGEBRA,
    OPT_PEEPHOLE_COMPILER_MYOPIA,
    OPT_PEEPHOLE_DEAD_STORES,
    OPT_PEEPHOLE_FORWARDING,
    OPT_PEEPHOLE_IMMEDIATE_PROP,
    OPT_PEEPHOLE_IMMEDIATES,
    OPT_PEEPHOLE_JMP_CHAIN,
    OPT_PEEPHOLE_JUMPS,
    OPT_PEEPHOLE_LOADS,
    OPT_PEEPHOLE_MOVS,
    OPT_PEEPHOLE_PAIRS,
    OPT_PEEPHOLE_REDUCE,
    OPT_PEEPHOLE_SHIFTS,
    OPT_CONSTANT_FOLDING,
    OPT_CSE,
    OPT_DCE,
    OPT_OMIT_FRAME_POINTERS,
    OPT_INLINE,
    OPT_PROMOTE_LEAF,
    OPT_PROMOTE_LOOPS,
    OPT_PROMOTE_REGS
} OptType;

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
    float float_value;
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
    bool     verbose;
    bool     testing;
    bool     debug;
    bool     opt_peephole_algebra;
    bool     opt_peephole_compiler_myopia;
    bool     opt_peephole_dead_stores;
    bool     opt_peephole_forwarding;
    bool     opt_peephole_immediate_prop;
    bool     opt_peephole_immediates;
    bool     opt_peephole_jmp_chain;
    bool     opt_peephole_jumps;
    bool     opt_peephole_loads;
    bool     opt_peephole_movs;
    bool     opt_peephole_pairs;
    bool     opt_peephole_reduce;
    bool     opt_peephole_shifts;
    bool     opt_constant_folding;
    bool     opt_cse;
    bool     opt_dce;
    bool     opt_omit_frame_pointers;
    bool     opt_inline;
    int      opt_inline_call_limit;
    int      opt_inline_max_body_ins;
    bool     opt_promote_leaf;
    bool     opt_promote_loops;
    bool     opt_promote_regs;
    LangMode lang_mode;
} OptConfig;

////////////////////////////////////////////////////////////////////////////////////////
// global variables
//
extern       int        g_inline_call_limit;
extern       int        g_inline_calls_so_far;
extern       char       g_inline_exclude_name[1024];
extern const char      *opt_type_names[];
extern       OptConfig  config;

////////////////////////////////////////////////////////////////////////////////////////
// general utility function prototypes
//
void  remove_with_debug       (AsmNode      **, AsmNode       *nodes[], int, OptType);
void  strip_comment_from_line (char          *, const char    *, size_t);
void  normalize_whitespace    (char          *, const char    *, size_t);
void  insert_debug_comment    (AsmNode       *, OptType,         const char *);
void  safe_str_copy           (char          *, const char    *, size_t);
char *trim                    (char          *);
bool  str_case_eq             (const char    *, const char    *);
int   get_reg_index           (const char    *);
bool  operands_equal          (const Operand *, const Operand *);
bool  is_power_of_two         (int);
int   get_log2                (int);
bool  is_lua_mode             (void);
bool  is_boxed_type_operand   (const Operand *);
bool  is_boxed_tagging        (AsmNode       *);

Operand  parse_operand (const char *);
AsmNode *create_node   (const char *, OpType, const char *, const char *, const char *);
void     remove_node   (AsmNode    *);
AsmNode *clone_node    (AsmNode    *);

bool     is_unconditional_branch  (AsmNode       *);
bool     is_conditional_branch    (AsmNode       *);
bool     is_branch_or_call        (AsmNode       *);
bool     modifies_register        (AsmNode       *, const char *);
bool     is_control_flow_boundary (AsmNode       *);
bool     is_register_read         (AsmNode       *, const char *);
bool     is_live_out_register     (const char    *);
long     parse_imm_val            (const char    *);
bool     is_numeric_immediate     (const Operand *);
void     get_label_name           (const AsmNode *, char       *, size_t);

// parsing / writing functions
AsmNode *parse_vircon32_asm (const char *);
void     write_vircon32_asm (const char *, AsmNode *);

// Argument processing
void     process_args (int,         char **,
                       OptConfig *, char  *,
                       size_t,      char  *,
                       size_t,      char  *,
                       size_t,      int   *);

// peephole optimizations
int      peephole_pairs           (AsmNode *);
int      peephole_algebra         (AsmNode *);
int      peephole_compiler_myopia (AsmNode *);
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

// helper functions
AsmNode *skip_other_nodes         (AsmNode       *);
AsmNode *skip_comments_and_blanks (AsmNode       *);
AsmNode *next_non_other           (AsmNode       *);
bool     is_register_operand      (const char    *);
bool     is_immediate_string      (const char    *);
bool     modifies_register        (AsmNode       *, const char    *);
bool     is_control_flow_boundary (AsmNode       *);
bool     is_register_read         (AsmNode       *, const char    *);
bool     is_live_out_register     (const char    *);
long     parse_imm_val            (const char    *);
bool     is_numeric_immediate     (const Operand *);
bool     operands_equal           (const Operand *, const Operand *);
void     get_label_name           (const AsmNode *, char          *, size_t);

// stack optimizations
int      omit_frame_pointers      (AsmNode *);

// inline optimization
int      inline_trivial_functions (AsmNode *);

// common subexpression elimination
int      opt_cse                  (AsmNode *);

// dead code elimination
int      opt_dce                  (AsmNode *);

// control flow graph
BasicBlock* find_block_by_label(ControlFlowGraph *, const char *);
ControlFlowGraph* build_cfg(AsmNode *);
void export_cfg_to_dot(const char *, ControlFlowGraph *);
void free_cfg(ControlFlowGraph *);
void add_edge(BasicBlock *, BasicBlock *);

// global data-flow analysis
RegState merge_reg(RegState, RegState);
bool apply_transfer_function(BasicBlock *);
void propagate_constants_cfg(ControlFlowGraph *);
int fold_constants_cfg(ControlFlowGraph *);

// promote.c prototypes
void  promote_operand_to_reg      (Operand    *, const char *);
bool  is_inside_loop              (const char *, AsmNode    *,    AsmNode *);
int   pass_promote_stack_slots    (AsmNode    *);
int   pass_promote_loop_registers (AsmNode    *);

#endif
