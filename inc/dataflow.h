#ifndef __V32OPT_DATAFLOW_H
#define __V32OPT_DATAFLOW_H

// ===========================================================================
// dataflow.h -- CFG construction & global data-flow passes
// ---------------------------------------------------------------------------
// Control-flow-graph types and the passes built on top of them (cfg.c:
// graph construction/DOT export and constant propagation/folding; cse.c:
// common-subexpression elimination; dce.c: dead-function elimination).
// ===========================================================================

// Lattice states for global constant propagation
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
    char labels[16][128];
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

// ---------------------------------------------------------------------------
// Dead-function elimination (dce.c)
// ---------------------------------------------------------------------------
typedef struct {
    char name[128];
    AsmNode *start_node;
    AsmNode *end_node;
    bool reachable;
} FunctionDef;

// ---------------------------------------------------------------------------
// CFG construction & export (cfg.c)
// ---------------------------------------------------------------------------
BasicBlock*      find_block_by_label       (ControlFlowGraph *, const char *);
ControlFlowGraph* build_cfg                (AsmNode *);
void             export_cfg_to_dot         (const char *, ControlFlowGraph *);
void             free_cfg                  (ControlFlowGraph *);
void             add_edge                  (BasicBlock *, BasicBlock *);

// ---------------------------------------------------------------------------
// Global constant propagation & folding (cfg.c)
// ---------------------------------------------------------------------------
RegState merge_reg                (RegState, RegState);
bool     apply_transfer_function  (BasicBlock *);
void     propagate_constants_cfg  (ControlFlowGraph *);
int      fold_constants_cfg       (ControlFlowGraph *);

// ---------------------------------------------------------------------------
// Common subexpression elimination (cse.c) & dead code elimination (dce.c)
// ---------------------------------------------------------------------------
int  opt_cse (AsmNode *);
int  opt_dce (AsmNode *);

#endif
