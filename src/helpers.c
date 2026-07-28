#include "v32opt.h"

// ===================================================================
// NODE TRAVERSAL HELPERS
// ===================================================================

// ---------------------------------------------------------------
// Skip OP_OTHER nodes (comments and blank lines)
// Returns the first non-OP_OTHER node after 'start', or NULL
// Example: skip_other_nodes(comment_node) -> first real instruction
// ---------------------------------------------------------------
AsmNode *skip_other_nodes(AsmNode *start)
{
    AsmNode *node = start;
    while (node && node->type == OP_OTHER) {
        node = node->next;
    }
    return node;
}

// ---------------------------------------------------------------
// Skip comment and blank OP_OTHER nodes only
// Stops at non-comment OP_OTHER (e.g., data directives)
// Returns the first non-comment/blank node after 'start', or NULL
// ---------------------------------------------------------------
AsmNode *skip_comments_and_blanks(AsmNode *start)
{
    AsmNode *node = start;
    while (node && node->type == OP_OTHER &&
           (node->raw[0] == '\0' || node->raw[0] == ';')) {
        node = node->next;
    }
    return node;
}

// ---------------------------------------------------------------
// Get the next non-OP_OTHER node after 'curr'
// Returns NULL if no such node exists
// Example: next_non_other(jmp_node) -> label or instruction after comments
// ---------------------------------------------------------------
AsmNode *next_non_other(AsmNode *curr)
{
    return skip_other_nodes(curr ? curr->next : NULL);
}

// ===================================================================
// OPERAND ANALYSIS HELPERS
// ===================================================================

// ---------------------------------------------------------------
// Check if a string represents a register operand
// Uses get_reg_index() internally for robustness
// Returns: true if operand is R0-R15, SP, or BP
// Example: is_register_operand("R0") -> true
//          is_register_operand("_L1") -> false
// ---------------------------------------------------------------
bool is_register_operand(const char *operand)
{
    if (!operand) return false;
    return get_reg_index(operand) >= 0;
}

// ---------------------------------------------------------------
// Check if a string represents an immediate value
// Handles: decimal (42), negative (-10), hex (0x20, 0X20)
// Returns: true if the string is a numeric immediate
// Example: is_immediate_string("42") -> true
//          is_immediate_string("R0") -> false
// ---------------------------------------------------------------
bool is_immediate_string(const char *str)
{
    if (!str || !str[0]) return false;
    if (isdigit((unsigned char)str[0])) return true;
    if (str[0] == '-' && isdigit((unsigned char)str[1])) return true;
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) return true;
    return false;
}

// ===================================================================
// EXISTING HELPERS (from original helpers.c)
// ===================================================================

// ---------------------------------------------------------------
// Check if an instruction modifies a specific register
// Accounts for read-only destinations (JMP, PUSH, JT, JF, CALL)
// Handles SP/BP implicitly modified by PUSH/POP
// Handles R0-R13 clobbered by CALL
// ---------------------------------------------------------------
bool modifies_register(AsmNode *node, const char *reg_name)
{
    if (!node || !reg_name) return false;

    bool dst_is_read_only = (str_case_eq(node->mnemonic, "JMP")  ||
                             str_case_eq(node->mnemonic, "PUSH") ||
                             str_case_eq(node->mnemonic, "JT")   ||
                             str_case_eq(node->mnemonic, "JF")   ||
                             str_case_eq(node->mnemonic, "CALL"));

    if (!dst_is_read_only && node->has_dst && node->dst_op.mode == MODE_REG) {
        if (str_case_eq(node->dst_op.reg, reg_name)) return true;
    }

    if (node->type == OP_PUSH || node->type == OP_POP) {
        if (str_case_eq(reg_name, "SP") || str_case_eq(reg_name, "R15")) return true;
    }

    if (str_case_eq(node->mnemonic, "CALL")) {
        int idx = get_reg_index(reg_name);
        if (idx >= 0 && idx <= 13) return true;
    }

    return false;
}

// ---------------------------------------------------------------
// Check if an instruction breaks a basic block (control flow)
// Returns true for: NULL, labels, JMP, CIB, RET, HLT
// ---------------------------------------------------------------
bool is_control_flow_boundary(AsmNode *node)
{
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

// ---------------------------------------------------------------
// Robustly check if an instruction READS a specific register
// Checks: explicit src, indirect memory, ALU read-modify-write, PUSH
// ---------------------------------------------------------------
bool is_register_read(AsmNode *node, const char *reg_name)
{
    if (!node || !reg_name) return false;

    if (node->has_src && node->src_op.mode == MODE_REG) {
        if (str_case_eq(node->src_op.reg, reg_name)) return true;
    }

    if (node->has_src && node->src_op.mode == MODE_INDIRECT) {
        if (str_case_eq(node->src_op.reg, reg_name)) return true;
    }
    if (node->has_dst && node->dst_op.mode == MODE_INDIRECT) {
        if (str_case_eq(node->dst_op.reg, reg_name)) return true;
    }

    if (node->type != OP_MOV && node->has_dst && node->dst_op.mode == MODE_REG) {
        if (str_case_eq(node->dst_op.reg, reg_name)) return true;
    }

    if (node->type == OP_PUSH) {
        if (node->has_dst && node->dst_op.mode == MODE_REG && str_case_eq(node->dst_op.reg, reg_name)) return true;
        if (node->has_src && node->src_op.mode == MODE_REG && str_case_eq(node->src_op.reg, reg_name)) return true;
    }

    return false;
}

// ---------------------------------------------------------------
// Check if a register is "Live-Out" across function returns
// In Vircon32: R0 (return value), SP, and BP survive a RET
// ---------------------------------------------------------------
bool is_live_out_register(const char *reg_name)
{
    if (!reg_name) return true;
    if (str_case_eq(reg_name, "R0")) return true;
    if (str_case_eq(reg_name, "SP")) return true;
    if (str_case_eq(reg_name, "BP")) return true;
    return false;
}

// ---------------------------------------------------------------
// Safely parse integer immediates (hex, decimal, negative)
// Returns 0 on NULL or empty string
// ---------------------------------------------------------------
long parse_imm_val(const char *raw_val)
{
    if (!raw_val || raw_val[0] == '\0') return 0;
    return strtol(raw_val, NULL, 0);
}

// ---------------------------------------------------------------
// Check if an operand is a numeric immediate (not float)
// ---------------------------------------------------------------
bool is_numeric_immediate(const Operand *op)
{
    if (op->mode != MODE_IMMEDIATE || op->is_float) return false;
    const char *raw = op->raw;
    if (!raw || !raw[0]) return false;
    while (isspace((unsigned char)*raw)) raw++;
    if (isdigit((unsigned char)raw[0])) return true;
    if (raw[0] == '-' && isdigit((unsigned char)raw[1])) return true;
    if (raw[0] == '0' && (raw[1] == 'x' || raw[1] == 'X')) return true;
    return false;
}

// ---------------------------------------------------------------
// Structurally verify if two operands are identical
// Handles: REG, INDIRECT, IMMEDIATE modes
// ---------------------------------------------------------------
bool operands_equal(const Operand *op1, const Operand *op2)
{
    if (op1->mode != op2->mode) return false;

    if (op1->mode == MODE_REG) {
        return str_case_eq(op1->reg, op2->reg);
    }
    if (op1->mode == MODE_INDIRECT) {
        return str_case_eq(op1->reg, op2->reg) && (op1->offset == op2->offset);
    }
    if (op1->mode == MODE_IMMEDIATE) {
        if (op1->is_float != op2->is_float) return false;
        if (op1->is_float) return op1->float_value == op2->float_value;
        return op1->immediate == op2->immediate;
    }
    return str_case_eq(op1->raw, op2->raw);
}

// ---------------------------------------------------------------
// Extract clean label name from an OP_LABEL node
// Removes trailing colon and trims whitespace
// Example: get_label_name(label_node, buf, 128) -> buf = "my_label"
// ---------------------------------------------------------------
void get_label_name(const AsmNode *node, char *buf, size_t buf_size)
{
    buf[0] = '\0';
    if (!node || node->type != OP_LABEL) return;

    safe_str_copy(buf, node->raw, buf_size);
    char *colon = strchr(buf, ':');
    if (colon) *colon = '\0';

    char *trimmed = trim(buf);
    if (trimmed != buf) {
        memmove(buf, trimmed, strlen(trimmed) + 1);
    }
}
