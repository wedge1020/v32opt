#include "v32opt.h"

// ===================================================================
// HELPER: Check if an instruction modifies a specific register
// ===================================================================
bool modifies_register(AsmNode *node, const char *reg_name) {
    if (!node || !reg_name) return false;

    // Check if the node explicitly overwrites the destination register
    if (node->has_dst && node->dst_op.mode == MODE_REG) {
        if (str_case_eq(node->dst_op.reg, reg_name)) return true;
    }

    // PUSH and POP implicitly modify SP; POP also modifies its destination
    if (node->type == OP_PUSH || node->type == OP_POP) {
        if (str_case_eq(reg_name, "SP") || str_case_eq(reg_name, "R15")) return true;
    }

    // In Vircon32, function CALLs clobber volatile scratch registers R0-R13
    if (str_case_eq(node->mnemonic, "CALL")) {
        int idx = get_reg_index(reg_name);
        if (idx >= 0 && idx <= 13) return true;
    }

    return false;
}

// ===================================================================
// HELPER: Check if an instruction breaks a basic block (control flow)
// ===================================================================
bool is_control_flow_boundary(AsmNode *node) {
    if (!node) return true;
    if (node->type == OP_LABEL) return true;
    if (str_case_eq(node->mnemonic, "JMP") ||
        str_case_eq(node->mnemonic, "CIB") ||
        str_case_eq(node->mnemonic, "RET") ||
        str_case_eq(node->mnemonic, "HLT")) {
        return true;
    }
    return false;
}

// ===================================================================
// HELPER: Robustly check if an instruction READS a specific register
// ===================================================================
bool is_register_read(AsmNode *node, const char *reg_name) {
    if (!node || !reg_name) return false;

    // 1. Check explicit source operand (e.g., MOV R1, R0 reads R0)
    if (node->has_src && node->src_op.mode == MODE_REG) {
        if (str_case_eq(node->src_op.reg, reg_name)) return true;
    }

    // 2. Check memory dereferences! (e.g., MOV R1, [R0] or MOV [R0], R1 both READ R0)
    if (node->has_src && node->src_op.mode == MODE_INDIRECT) {
        if (str_case_eq(node->src_op.reg, reg_name)) return true;
    }
    if (node->has_dst && node->dst_op.mode == MODE_INDIRECT) {
        if (str_case_eq(node->dst_op.reg, reg_name)) return true;
    }

    // 3. For ALU ops (IADD, ISUB, etc.), destination is READ and WRITTEN (read-modify-write)
    // e.g., IADD R0, 5 READS R0 before adding 5 to it!
    if (node->type != OP_MOV && node->has_dst && node->dst_op.mode == MODE_REG) {
        if (str_case_eq(node->dst_op.reg, reg_name)) return true;
    }

    // 4. PUSH instructions read whatever register they are pushing onto the stack
    if (node->type == OP_PUSH) {
        if (node->has_dst && node->dst_op.mode == MODE_REG && str_case_eq(node->dst_op.reg, reg_name)) return true;
        if (node->has_src && node->src_op.mode == MODE_REG && str_case_eq(node->src_op.reg, reg_name)) return true;
    }

    return false;
}

// ===================================================================
// HELPER: Check if a register is "Live-Out" across function returns
// In Vircon32, only R0 (return value), SP, and BP survive a RET.
// ===================================================================
bool is_live_out_register(const char *reg_name) {
    if (!reg_name) return true; // Be conservative on NULL
    if (str_case_eq(reg_name, "R0")) return true;
    if (str_case_eq(reg_name, "SP")) return true;
    if (str_case_eq(reg_name, "BP")) return true;
    return false;
}

// ===================================================================
// HELPER: Safely parse integer immediates (hex, decimal, negative)
// ===================================================================
long parse_imm_val(const char *raw_val) {
    if (!raw_val || raw_val[0] == '\0') return 0;
    return strtol(raw_val, NULL, 0);
}

bool is_numeric_immediate(const Operand *op) {
    if (op->mode != MODE_IMMEDIATE || op->is_float) return false;
    const char *raw = op->raw;
    if (!raw || raw[0] == '\0') return false;
    // Skip leading whitespace
    while (isspace((unsigned char)*raw)) raw++;
    if (isdigit((unsigned char)raw[0])) return true;
    if (raw[0] == '-' && isdigit((unsigned char)raw[1])) return true;
    if (raw[0] == '0' && (raw[1] == 'x' || raw[1] == 'X')) return true;
    return false;
}
