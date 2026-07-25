#include "v32opt.h"

// ===================================================================
// HELPER: Check if an instruction is a global memory write barrier
// ===================================================================
bool is_global_memory_clobber(AsmNode *node) {
    if (!node || node->type == OP_OTHER) return false;
    if (str_case_eq(node->mnemonic, "MOVS") ||
        str_case_eq(node->mnemonic, "SETS") ||
        str_case_eq(node->mnemonic, "CALL")) {
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
