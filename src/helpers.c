#include "v32opt.h"

// Returns true if the instruction writes to memory in a way that cannot be 
// statically predicted (requiring a total flush of Store-to-Load forwarding caches).
bool is_global_memory_clobber(AsmNode *node) {
    if (!node || node->type == OP_OTHER) return false;

    // 1. String instructions write to dynamic memory pointers [DR]
    if (str_case_eq(node->mnemonic, "MOVS") ||
        str_case_eq(node->mnemonic, "SETS")) {
        return true;
    }

    // 2. Function calls can execute arbitrary memory stores inside the callee
    if (str_case_eq(node->mnemonic, "CALL")) {
        return true;
    }

    return false;
}

// Returns true if the instruction reads from unpredictable memory pointers
bool is_global_memory_read(AsmNode *node) {
    if (!node || node->type == OP_OTHER) return false;

    if (str_case_eq(node->mnemonic, "MOVS") ||
        str_case_eq(node->mnemonic, "CMPS") ||
        str_case_eq(node->mnemonic, "CALL")) {
        return true;
    }

    return false;
}
