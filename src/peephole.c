#include "v32opt.h"

////////////////////////////////////////////////////////////////////////////////////////
//
// -------------------------------------------------------------------
// OPTIMIZATION CATEGORY: Peephole Optimizations
// Small-window (1-3 instruction) local transformations that improve
// code without global analysis.
// -------------------------------------------------------------------
//
// Note: On Vircon32, ALL instructions are 1 cycle, so many transformations
// are cost-neutral. We keep them for code clarity, size reduction, or
// idiomatic style.
//
// peephole_pairs()          - adjacent instruction pair elimination
// peephole_algebra()        - algebraic simplifications
// peephole_forwarding()     - store-to-load forwarding
// peephole_jumps()          - redundant jump elimination
// peephole_movs()           - redundant MOV elimination
// peephole_immediates()     - combine immediates
// peephole_reduce()         - strength reduction (cost-neutral on Vircon32)
// peephole_shifts()         - shift optimizations
// peephole_dead_stores()    - dead store elimination
// peephole_loads()          - redundant load elimination
// peephole_immediate_prop() - immediate propagation
// peephole_jmp_chain()      - jump chain elimination
//
////////////////////////////////////////////////////////////////////////////////////////

// ===================================================================
// PEEPHOLE: Adjacent Instruction Pair Elimination
// Scans a sliding window of two consecutive instructions and removes
// redundant pairs:
//   - IEQ/INE followed by CIB on the same register (CIB is redundant)
//   - BNOT x; BNOT x → remove both (double negation cancels)
//   - PUSH r; POP r → remove both (no net effect)
// ===================================================================
int peephole_pairs(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr && curr->next)
    {
        AsmNode *n1 = curr;
        AsmNode *n2 = curr->next;

        // --- IEQ/INE + CIB Redundancy ---
        // IEQ/INE sets a flag; CIB converts an integer to boolean
        // If they target the same register, the CIB is redundant because
        // the flag is already set and the value is already boolean
        if ((n1->type == OP_IEQ || n1->type == OP_INE) &&
             n2->type == OP_CIB &&
             n1->dst_op.mode == MODE_REG && n2->dst_op.mode == MODE_REG &&
             str_case_eq(n1->dst_op.reg, n2->dst_op.reg))
        {
            // Don't remove CIB if next non-comment is JT/JF
            AsmNode *after_cib = n2->next;
            while (after_cib && after_cib->type == OP_OTHER &&
                   (after_cib->raw[0] == '\0' || after_cib->raw[0] == ';')) {
                after_cib = after_cib->next;
            }
            if (after_cib && (str_case_eq(after_cib->mnemonic, "JT") ||
                               str_case_eq(after_cib->mnemonic, "JF"))) {
                curr = curr->next;
                continue;
            }

            AsmNode *nodes[] = {n2};
            remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_PAIRS);
            optimizations++;
            continue;
        }

        // ----------------------------------------------------------------
        // PATTERN: Self-Inverting Pairs (Involutions)
        // Identical consecutive operations that cancel each other out:
        //   - BNOT R4 ; BNOT R4 -> (Bitwise NOT twice = Identity)
        //   - ISGN R4 ; ISGN R4 -> (Two's complement negate twice = Identity)
        //   - NOT R4  ; NOT R4  -> (Logical NOT twice = Identity)
        // ----------------------------------------------------------------
        if (str_case_eq(n1->mnemonic, "BNOT") ||
            str_case_eq(n1->mnemonic, "ISGN") ||
            str_case_eq(n1->mnemonic, "NEG")  ||
            str_case_eq(n1->mnemonic, "NOT"))
        {
            AsmNode *next = n1->next;
            // Safely skip any inline comments or blank lines between the pair
            while (next && next->type == OP_OTHER) next = next->next;

            if (next && str_case_eq(next->mnemonic, n1->mnemonic))
            {
                char *reg1 = n1->has_dst ? n1->dst_op.reg : (n1->has_src ? n1->src_op.reg : NULL);
                char *reg2 = next->has_dst ? next->dst_op.reg : (next->has_src ? next->src_op.reg : NULL);

                if (reg1 && reg2 && str_case_eq(reg1, reg2))
                {
                    AsmNode *nodes[] = {n1, next};
                    remove_with_debug(&curr, nodes, 2, OPT_PEEPHOLE_PAIRS);
                    optimizations += 2;
                    continue;
                }
            }
        }

        // ----------------------------------------------------------------
        // BONUS PATTERN: Identical XOR Pairs
        // Toggling a register with the exact same value twice cancels out:
        //   - XOR R1, R2 ; XOR R1, R2 -> cancels out!
        //   - XOR R1, 42 ; XOR R1, 42 -> cancels out!
        // ----------------------------------------------------------------
        if (str_case_eq(n1->mnemonic, "XOR"))
        {
            AsmNode *next = n1->next;
            while (next && next->type == OP_OTHER) next = next->next;

            if (next && str_case_eq(next->mnemonic, "XOR"))
            {
                // Verify destination registers match
                if (n1->has_dst && next->has_dst && str_case_eq(n1->dst_op.reg, next->dst_op.reg))
                {
                    bool src_match = false;

                    // Check if both XOR with the same register
                    if (n1->src_op.mode == MODE_REG && next->src_op.mode == MODE_REG) {
                        if (str_case_eq(n1->src_op.reg, next->src_op.reg)) src_match = true;
                    }
                    // Check if both XOR with the exact same immediate value
                    else if (n1->src_op.mode == MODE_IMMEDIATE && next->src_op.mode == MODE_IMMEDIATE) {
                        if (n1->src_op.offset == next->src_op.offset &&
                            str_case_eq(n1->src_op.raw, next->src_op.raw)) src_match = true;
                    }

                    if (src_match)
                    {
                        AsmNode *nodes[] = {n1, next};
                        remove_with_debug(&curr, nodes, 2, OPT_PEEPHOLE_PAIRS);
                        optimizations += 2;
                        continue;
                    }
                }
            }
        }

        // --- PUSH/POP Pair Elimination ---
        // PUSH r; POP r → no net effect on the stack or register
        // Must skip comments between PUSH and POP
        AsmNode *pop_node = n1->next;
        while (pop_node && pop_node->type == OP_OTHER) {
            pop_node = pop_node->next;
        }
        if (n1->type == OP_PUSH && pop_node && pop_node->type == OP_POP &&
            n1->dst_op.mode == MODE_REG && pop_node->dst_op.mode == MODE_REG &&
            str_case_eq(n1->dst_op.reg, pop_node->dst_op.reg))
        {
            AsmNode *nodes[] = {n1, pop_node};
            remove_with_debug(&curr, nodes, 2, OPT_PEEPHOLE_PAIRS);
            optimizations += 2;
            continue;
        }

        curr = curr->next;
    }

    return optimizations;
}

// ===================================================================
// PEEPHOLE: Algebraic Simplification
// Removes or replaces instructions that are algebraically redundant:
//   - MOV r, r → remove (no-op)
//   - IADD/ISUB r, 0 → remove (identity)
//   - IMUL r, 2 → replace with IADD r, r (cost-neutral on Vircon32, but idiomatic)
// ===================================================================
int peephole_algebra(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr != NULL)
    {
        AsmNode *next = curr->next;

        // --- MOV r, r (Self-Move Elimination) ---
        // Copying a register to itself is a no-op.
        if (curr->type == OP_MOV &&
            curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_REG &&
            str_case_eq(curr->dst_op.reg, curr->src_op.reg))
        {
            remove_node(curr);
            optimizations++;
            curr = next;
            continue;
        }

        // --- IADD/ISUB with Immediate 0 ---
        // Adding or subtracting 0 leaves the register unchanged.
        if ((curr->type == OP_IADD || curr->type == OP_ISUB) &&
            curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float && curr->src_op.immediate == 0)
        {
            remove_node(curr);
            optimizations++;
            curr = next;
            continue;
        }

        // --- IMUL by 2 Strength Reduction ---
        // IMUL r, 2 → IADD r, r
        // Note: Cost-neutral on Vircon32 (both are 1 cycle), but IADD may be
        // preferred for clarity or to reduce instruction variety.
        if (curr->type == OP_IMUL &&
            curr->dst_op.mode == MODE_REG &&
            curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float &&
            curr->src_op.immediate == 2)
        {
            curr->type = OP_IADD;
            strcpy(curr->mnemonic, "IADD");
            curr->src_op = curr->dst_op; // Source becomes same as destination
            snprintf(curr->raw, sizeof(curr->raw), "    IADD %s, %s", curr->dst_op.raw, curr->src_op.raw);
            optimizations++;
        }

        // --- IMUL by 0 → MOV r, 0 ---
        if (curr->type == OP_IMUL &&
            curr->dst_op.mode == MODE_REG &&
            curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float &&
            curr->src_op.immediate == 0)
        {
            insert_debug_comment(curr->prev, OPT_PEEPHOLE_ALGEBRA, curr->raw);
            curr->type = OP_MOV;
            strcpy(curr->mnemonic, "MOV");
            snprintf(curr->raw, sizeof(curr->raw), "    MOV %s, 0", curr->dst_op.raw);
            optimizations++;
            curr = next;
            continue;
        }

        // --- IMUL by 1 → Remove (identity) ---
        if (curr->type == OP_IMUL &&
            curr->dst_op.mode == MODE_REG &&
            curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float &&
            curr->src_op.immediate == 1)
        {
            AsmNode *nodes[] = {curr};
            remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_ALGEBRA);
            optimizations++;
            continue;
        }

        // --- IDIV by 1 → Remove (identity) ---
        if (curr->type == OP_IDIV &&
            curr->dst_op.mode == MODE_REG &&
            curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float &&
            curr->src_op.immediate == 1)
        {
            AsmNode *nodes[] = {curr};
            remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_ALGEBRA);
            optimizations++;
            continue;
        }

        curr = next;
    }

    return optimizations;
}

// ===================================================================
// PEEPHOLE: Forward-Scanning Store & Copy Propagation
// Scans forward within basic blocks to forward:
//   1. Store-to-Load: MOV [mem], R1; ... MOV R2, [mem] -> MOV R2, R1
//   2. Copy Prop:     MOV R1, val;   ... OP R2, R1     -> OP R2, val
// ===================================================================
int peephole_forwarding(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        if (curr->type == OP_MOV)
        {
            // =========================================================
            // RULE 1: Store-to-Load Forwarding
            // =========================================================
            if (curr->dst_op.mode == MODE_INDIRECT && curr->src_op.mode == MODE_REG)
            {
                char *mem_reg = curr->dst_op.reg;
                int mem_off = curr->dst_op.offset;
                char *src_reg = curr->src_op.reg;

                AsmNode *scan = curr->next;
                while (scan)
                {
                    if (scan->type == OP_OTHER && (scan->raw[0] == '\0' || scan->raw[0] == ';')) {
                        scan = scan->next;
                        continue;
                    }

                    // Stop if memory base register or source register is modified
                    if (modifies_register(scan, mem_reg) || modifies_register(scan, src_reg)) break;

                    // 🔥 STOP on ANY store to memory with same base register
                    if (scan->type == OP_MOV && scan->dst_op.mode == MODE_INDIRECT &&
                        str_case_eq(scan->dst_op.reg, mem_reg)) {
                        break;
                    }

                    if (scan->type == OP_MOV && scan->dst_op.mode == MODE_REG &&
                        scan->src_op.mode == MODE_INDIRECT &&
                        str_case_eq(scan->src_op.reg, mem_reg) && scan->src_op.offset == mem_off)
                    {
                        insert_debug_comment(scan->prev, OPT_PEEPHOLE_FORWARDING, scan->raw);
                        scan->src_op = curr->src_op;
                        snprintf(scan->raw, sizeof(scan->raw), "    MOV %s, %s",
                                 scan->dst_op.raw, scan->src_op.raw);
                        optimizations++;
                        break;
                    }

                    // Evaluate control flow boundaries AFTER checking for optimization
                    if (is_control_flow_boundary(scan)) break;

                    scan = scan->next;
                }
            }

            // =========================================================
            // RULE 2: Copy Propagation
            // =========================================================
            if (curr->dst_op.mode == MODE_REG)
            {
                char *def_reg = curr->dst_op.reg;

                if (str_case_eq(def_reg, "SP") || str_case_eq(def_reg, "BP")) {
                    curr = curr->next;
                    continue;
                }

                AsmNode *scan = curr->next;
                while (scan)
                {
                    if (scan->type == OP_OTHER && (scan->raw[0] == '\0' || scan->raw[0] == ';')) {
                        scan = scan->next;
                        continue;
                    }

                    if (modifies_register(scan, def_reg)) break;
                    if (curr->src_op.mode == MODE_REG && modifies_register(scan, curr->src_op.reg)) {
                        break;
                    }

                    bool uses_def_reg = false;
                    Operand *target_op = NULL;

                    // Two-operand instructions: check src_op
                    if (scan->has_src && scan->src_op.mode == MODE_REG &&
                        str_case_eq(scan->src_op.reg, def_reg)) {
                        uses_def_reg = true;
                        target_op = &scan->src_op;
                    }
                    // 🔥 FIX: Single-operand instructions (like JMP) are parsed into dst_op!
                    else if (str_case_eq(scan->mnemonic, "JMP") && scan->has_dst &&
                             scan->dst_op.mode == MODE_REG &&
                             str_case_eq(scan->dst_op.reg, def_reg)) {
                        uses_def_reg = true;
                        target_op = &scan->dst_op;
                    }

                    if (uses_def_reg)
                    {
                        // Guard: Block numeric immediates into JT/JF
                        if ((str_case_eq(scan->mnemonic, "JT") || str_case_eq(scan->mnemonic, "JF")) &&
                            curr->src_op.mode == MODE_IMMEDIATE &&
                            (isdigit((unsigned char)curr->src_op.raw[0]) ||
                             (curr->src_op.raw[0] == '-' && isdigit((unsigned char)curr->src_op.raw[1])))) {
                            break;
                        }

                        // Guard: Block POW/ATAN2
                        if (str_case_eq(scan->mnemonic, "POW") || str_case_eq(scan->mnemonic, "ATAN2")) {
                            break;
                        }

                        // Guard: Block illegal immediate stores
                        bool is_illegal_imm_store = (curr->src_op.mode == MODE_IMMEDIATE &&
                                                     scan->has_dst && scan->dst_op.mode != MODE_REG &&
                                                     !str_case_eq(scan->mnemonic, "JMP"));
                        if (is_illegal_imm_store) break;

                        insert_debug_comment(scan->prev, OPT_PEEPHOLE_FORWARDING, scan->raw);
                        *target_op = curr->src_op;

                        if (str_case_eq(scan->mnemonic, "JMP")) {
                            snprintf(scan->raw, sizeof(scan->raw), "    JMP %s", target_op->raw);
                        } else if (scan->has_dst && scan->has_src) {
                            snprintf(scan->raw, sizeof(scan->raw), "    %s %s, %s",
                                     scan->mnemonic, scan->dst_op.raw, scan->src_op.raw);
                        } else if (scan->has_dst) {
                            snprintf(scan->raw, sizeof(scan->raw), "    %s %s",
                                     scan->mnemonic, scan->dst_op.raw);
                        } else {
                            snprintf(scan->raw, sizeof(scan->raw), "    %s %s",
                                     scan->mnemonic, scan->src_op.raw);
                        }
                        optimizations++;
                    }

                    // Evaluate control flow boundaries AFTER attempting optimization
                    if (is_control_flow_boundary(scan)) break;

                    scan = scan->next;
                }
            }
        }
        curr = curr->next;
    }
    return optimizations;
}
/*
int peephole_forwarding(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        if (curr->type == OP_MOV)
        {
            // =========================================================
            // RULE 1: Store-to-Load Forwarding
            // =========================================================
            if (curr->dst_op.mode == MODE_INDIRECT && curr->src_op.mode == MODE_REG)
            {
                char *mem_reg = curr->dst_op.reg;
                int mem_off = curr->dst_op.offset;
                char *src_reg = curr->src_op.reg;

                AsmNode *scan = curr->next;
                while (scan)
                {
                    if (scan->type == OP_OTHER && (scan->raw[0] == '\0' || scan->raw[0] == ';')) {
                        scan = scan->next;
                        continue;
                    }

                    if (is_control_flow_boundary(scan)) break;

                    // 🔥 STOP on ANY store to memory with same base register
                    if (scan->type == OP_MOV && scan->dst_op.mode == MODE_INDIRECT &&
                        str_case_eq(scan->dst_op.reg, mem_reg)) {
                        break;
                    }

                    // Stop if memory base register or source register is modified
                    if (modifies_register(scan, mem_reg) || modifies_register(scan, src_reg)) break;

                    if (scan->type == OP_MOV && scan->dst_op.mode == MODE_REG &&
                        scan->src_op.mode == MODE_INDIRECT &&
                        str_case_eq(scan->src_op.reg, mem_reg) && scan->src_op.offset == mem_off)
                    {
                        insert_debug_comment(scan->prev, OPT_PEEPHOLE_FORWARDING, scan->raw);
                        scan->src_op = curr->src_op;
                        snprintf(scan->raw, sizeof(scan->raw), "    MOV %s, %s",
                                 scan->dst_op.raw, scan->src_op.raw);
                        optimizations++;
                        break;
                    }

                    scan = scan->next;
                }
            }

            // =========================================================
            // RULE 2: Copy Propagation
            // =========================================================
            if (curr->dst_op.mode == MODE_REG)
            {
                char *def_reg = curr->dst_op.reg;

                if (str_case_eq(def_reg, "SP") || str_case_eq(def_reg, "BP")) {
                    curr = curr->next;
                    continue;
                }

                AsmNode *scan = curr->next;
                while (scan)
                {
                    if (scan->type == OP_OTHER && (scan->raw[0] == '\0' || scan->raw[0] == ';')) {
                        scan = scan->next;
                        continue;
                    }

                    if (is_control_flow_boundary(scan)) break;

                    if (modifies_register(scan, def_reg)) break;
                    if (curr->src_op.mode == MODE_REG && modifies_register(scan, curr->src_op.reg)) {
                        break;
                    }

                    bool uses_def_reg = false;
                    Operand *target_op = NULL;

                    // Two-operand instructions: check src_op
                    if (scan->has_src && scan->src_op.mode == MODE_REG &&
                        str_case_eq(scan->src_op.reg, def_reg)) {
                        uses_def_reg = true;
                        target_op = &scan->src_op;
                    }
                    // 🔥 FIX: JMP uses src_op for its target in Vircon32
                    else if (str_case_eq(scan->mnemonic, "JMP") && scan->has_src &&
                             scan->src_op.mode == MODE_REG &&
                             str_case_eq(scan->src_op.reg, def_reg)) {
                        uses_def_reg = true;
                        target_op = &scan->src_op;
                    }

                    if (uses_def_reg)
                    {
                        // Guard: Block numeric immediates into JT/JF
                        if ((str_case_eq(scan->mnemonic, "JT") || str_case_eq(scan->mnemonic, "JF")) &&
                            curr->src_op.mode == MODE_IMMEDIATE &&
                            (isdigit((unsigned char)curr->src_op.raw[0]) ||
                             (curr->src_op.raw[0] == '-' && isdigit((unsigned char)curr->src_op.raw[1])))) {
                            break;
                        }

                        // Guard: Block POW/ATAN2
                        if (str_case_eq(scan->mnemonic, "POW") || str_case_eq(scan->mnemonic, "ATAN2")) {
                            break;
                        }

                        // Guard: Block illegal immediate stores
                        bool is_illegal_imm_store = (curr->src_op.mode == MODE_IMMEDIATE &&
                                                     scan->has_dst && scan->dst_op.mode != MODE_REG);
                        if (is_illegal_imm_store) break;

                        insert_debug_comment(scan->prev, OPT_PEEPHOLE_FORWARDING, scan->raw);
                        *target_op = curr->src_op;

                        if (str_case_eq(scan->mnemonic, "JMP")) {
                            snprintf(scan->raw, sizeof(scan->raw), "    JMP %s", target_op->raw);
                        } else if (scan->has_dst && scan->has_src) {
                            snprintf(scan->raw, sizeof(scan->raw), "    %s %s, %s",
                                     scan->mnemonic, scan->dst_op.raw, scan->src_op.raw);
                        } else if (scan->has_dst) {
                            snprintf(scan->raw, sizeof(scan->raw), "    %s %s",
                                     scan->mnemonic, scan->dst_op.raw);
                        } else {
                            snprintf(scan->raw, sizeof(scan->raw), "    %s %s",
                                     scan->mnemonic, scan->src_op.raw);
                        }
                        optimizations++;
                    }

                    scan = scan->next;
                }
            }
        }
        curr = curr->next;
    }
    return optimizations;
}
*/

// ===================================================================
// PEEPHOLE: Redundant Jump Elimination
// Removes jumps that target the immediately following label:
//   - JMP L; L: → remove JMP
// ===================================================================
int peephole_jumps(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        if (str_case_eq(curr->mnemonic, "JMP"))
        {
            // Skip over comments/blank lines to find the next real instruction
            AsmNode *next_node = curr->next;
            while (next_node && next_node->type == OP_OTHER &&
                  (next_node->raw[0] == '\0' || next_node->raw[0] == ';'))
            {
                next_node = next_node->next;
            }

            // --- Jump to Next Label ---
            // If the next non-comment node is a label, and the JMP targets
            // that label, the JMP is redundant (fall-through).
            if (next_node && next_node->type == OP_LABEL)
            {
                // Extract label name (remove trailing colon if present)
                char lbl[128] = {0};
                safe_str_copy(lbl, next_node->raw, sizeof(lbl));
                char *colon = strchr(lbl, ':');
                if (colon) *colon = '\0';

                if (str_case_eq(trim(lbl), trim(curr->dst_op.raw)))
                {
                    AsmNode *to_remove = curr;
                    curr = curr->next;
                    remove_node(to_remove);
                    optimizations++;
                    continue; // Skip the removed node
                }
            }
        }
        curr = curr->next;
    }
    return optimizations;
}

// ===================================================================
// PEEPHOLE: Redundant & Mirror Move Elimination
// Removes redundant MOV pairs:
//   - MOV r1, X; MOV r1, X → remove second MOV
//   - MOV r1, r2; MOV r2, r1 → remove second MOV (mirror)
// ===================================================================
int peephole_movs(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        if (curr->type == OP_MOV)
        {
            // Skip over comments/blank lines to find the next real instruction
            AsmNode *n2 = curr->next;
            while (n2 && n2->type == OP_OTHER &&
                  (n2->raw[0] == '\0' || n2->raw[0] == ';'))
            {
                n2 = n2->next;
            }
            if (!n2) break;

            if (n2->type == OP_MOV)
            {
                // --- Self-Referential Load Check ---
                // If the first MOV loads from [r1] into r1, we cannot eliminate
                // the second MOV even if it uses r1, because the value might change.
                bool self_referential_load =
                    (curr->src_op.mode == MODE_INDIRECT) &&
                    str_case_eq(curr->src_op.reg, curr->dst_op.reg);

                // --- Duplicate Move Elimination ---
                // MOV r1, X; MOV r1, X → second MOV is redundant
                if (!self_referential_load &&
                    str_case_eq(curr->dst_op.raw, n2->dst_op.raw) &&
                    str_case_eq(curr->src_op.raw, n2->src_op.raw))
                {
                    remove_node(n2);
                    optimizations++;
                    continue;
                }

                // --- Mirror Move Elimination ---
                // MOV r1, r2; MOV r2, r1 → second MOV is redundant
                // (swapping the same two registers twice restores original state)
                if (curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_REG &&
                    n2->dst_op.mode == MODE_REG && n2->src_op.mode == MODE_REG)
                {
                    if (str_case_eq(curr->dst_op.reg, n2->src_op.reg) &&
                        str_case_eq(curr->src_op.reg, n2->dst_op.reg))
                    {
                        remove_node(n2);
                        optimizations++;
                        continue;
                    }
                }
            }
        }
        curr = curr->next;
    }
    return optimizations;
}

// ===================================================================
// PEEPHOLE: Immediate Math Combining
// Combines consecutive arithmetic operations with immediate operands:
//   - IADD r, 5; ISUB r, 3 → IADD r, 2
//   - IADD r, 5; ISUB r, 5 → remove both (cancels out)
// ===================================================================
int peephole_immediates(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        // Only process IADD/ISUB with register destination and immediate source
        if ((curr->type == OP_IADD || curr->type == OP_ISUB) &&
            curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float)
        {
            // 🔥 FIX: Skip ALL OP_OTHER nodes (comments/blanks), not just those starting with ;
            AsmNode *n2 = curr->next;
            while (n2 && n2->type == OP_OTHER) {
                n2 = n2->next;
            }

            // Check if n2 is also an IADD/ISUB with same destination and immediate
            if (n2 && (n2->type == OP_IADD || n2->type == OP_ISUB) &&
                n2->dst_op.mode == MODE_REG && n2->src_op.mode == MODE_IMMEDIATE && !n2->src_op.is_float &&
                str_case_eq(curr->dst_op.reg, n2->dst_op.reg))
            {
                // Calculate effective values: IADD adds, ISUB subtracts
                int val1 = (curr->type == OP_IADD) ? curr->src_op.immediate : -curr->src_op.immediate;
                int val2 = (n2->type == OP_IADD) ? n2->src_op.immediate : -n2->src_op.immediate;
                int combined = val1 + val2;

                // --- Cancellation Case ---
                if (combined == 0)
                {
                    if (config.debug) {
                        insert_debug_comment(curr->prev, OPT_PEEPHOLE_IMMEDIATES, curr->raw);
                        insert_debug_comment(n2->prev, OPT_PEEPHOLE_IMMEDIATES, n2->raw);
                    }
                    AsmNode *nodes[] = {curr, n2};
                    remove_with_debug(&curr, nodes, 2, OPT_PEEPHOLE_IMMEDIATES);
                    optimizations += 2;
                    continue;
                }
                // --- Non-Zero Combination ---
                else
                {
                    if (config.debug) {
                        insert_debug_comment(curr->prev, OPT_PEEPHOLE_IMMEDIATES, curr->raw);
                    }
                    curr->type = (combined > 0) ? OP_IADD : OP_ISUB;
                    strcpy(curr->mnemonic, (combined > 0) ? "IADD" : "ISUB");
                    curr->src_op.immediate = abs(combined);
                    snprintf(curr->src_op.raw, sizeof(curr->src_op.raw), "%d", abs(combined));
                    snprintf(curr->raw, sizeof(curr->raw), "    %s %s, %d",
                             curr->mnemonic, curr->dst_op.raw, abs(combined));
                    if (config.debug) {
                        insert_debug_comment(n2->prev, OPT_PEEPHOLE_IMMEDIATES, n2->raw);
                    }
                    AsmNode *nodes[] = {n2};
                    remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_IMMEDIATES);
                    optimizations++;
                    continue;
                }
            }
        }
        curr = curr->next;
    }
    return optimizations;
}

// ===================================================================
// PEEPHOLE: Strength Reduction
// Replaces expensive operations with cheaper equivalents.
// Note: On Vircon32, all instructions are 1 cycle, so these are
// cost-neutral but may improve code clarity or reduce variety.
//   - IMUL r, 0 → MOV r, 0
//   - IMUL r, 1 → remove (identity)
//   - IMUL r, 2 → IADD r, r (cost-neutral, but idiomatic)
//   - IDIV r, 1 → remove (identity)
// ===================================================================
int peephole_reduce(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        // --- IMUL Strength Reduction ---
        if (curr->type == OP_IMUL && curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float)
        {
            int val = curr->src_op.immediate;

            // --- Multiply by 0 → MOV 0 ---
            // Any value * 0 = 0
            if (val == 0)
            {
                curr->type = OP_MOV;
                safe_str_copy(curr->mnemonic, "MOV", sizeof(curr->mnemonic));
                snprintf(curr->raw, sizeof(curr->raw), "    MOV %s, 0", curr->dst_op.raw);
                optimizations++;
                curr = curr->next;
                continue;
            }

            // --- Multiply by 1 → Remove ---
            // Any value * 1 = itself (identity operation)
            if (val == 1)
            {
                AsmNode *to_remove = curr;
                curr = curr->next;
                remove_node(to_remove);
                optimizations++;
                continue;
            }

            // --- Multiply by 2 → IADD r, r ---
            // x * 2 = x + x (cost-neutral on Vircon32)
            if (val == 2)
            {
                curr->type = OP_IADD;
                safe_str_copy(curr->mnemonic, "IADD", sizeof(curr->mnemonic));
                curr->src_op.mode = MODE_REG;
                safe_str_copy(curr->src_op.reg, curr->dst_op.reg, sizeof(curr->src_op.reg));
                safe_str_copy(curr->src_op.raw, curr->dst_op.raw, sizeof(curr->src_op.raw));
                snprintf(curr->raw, sizeof(curr->raw), "    IADD %s, %s", curr->dst_op.raw, curr->dst_op.raw);
                optimizations++;
                curr = curr->next;
                continue;
            }
        }

        // --- IDIV Strength Reduction ---
        if (curr->type == OP_IDIV && curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float)
        {
            int val = curr->src_op.immediate;

            // --- Divide by 1 → Remove ---
            // Any value / 1 = itself (identity operation)
            if (val == 1)
            {
                AsmNode *to_remove = curr;
                curr = curr->next;
                remove_node(to_remove);
                optimizations++;
                continue;
            }
        }

        curr = curr->next;
    }
    return optimizations;
}

// ===================================================================
// PEEPHOLE: Shift Optimizations
// On Vircon32, SHL with positive value = left shift, negative = right shift.
//   - SHL r, 0 → remove (no-op; applies to both left and right shifts)
//   - SHL r, 1 → IADD r, r (cost-neutral on Vircon32, but idiomatic)
//   - SHL r, -1 → right shift by 1; left unchanged (no benefit to converting to IDIV)
// ===================================================================
int peephole_shifts(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        if (curr->type == OP_SHL &&
            curr->dst_op.mode == MODE_REG &&
            curr->src_op.mode == MODE_IMMEDIATE && !curr->src_op.is_float)
        {
            int shift = curr->src_op.immediate;

            // --- Shift by 0 (No-Op) ---
            // Left or right shift by 0 leaves the register unchanged.
            if (shift == 0)
            {
                AsmNode *to_remove = curr;
                curr = curr->next;
                remove_node(to_remove);
                optimizations++;
                continue;
            }

            // --- Left Shift by 1 ---
            // SHL r, 1 = r * 2 = r + r
            // Cost-neutral on Vircon32 (both SHL and IADD are 1 cycle),
            // but IADD may be preferred for consistency.
            else if (shift == 1)
            {
                curr->type = OP_IADD;
                safe_str_copy(curr->mnemonic, "IADD", sizeof(curr->mnemonic));
                curr->src_op = curr->dst_op; // Source becomes same as destination
                snprintf(curr->src_op.raw, sizeof(curr->src_op.raw), "%s", curr->dst_op.raw);
                snprintf(curr->raw, sizeof(curr->raw), "    IADD %s, %s", curr->dst_op.raw, curr->src_op.raw);
                optimizations++;
            }

            // Note: SHL r, -1 (right shift by 1) is left unchanged.
            // On Vircon32, SHL and IDIV are both 1 cycle, so converting
            // SHL r, -1 to IDIV r, 2 would be cost-neutral but adds no benefit.
        }
        curr = curr->next;
    }
    return optimizations;
}

// ===================================================================
// PEEPHOLE: Dead Store Elimination (DSE) - Upgraded with Liveness!
// Eliminates register writes overwritten OR dead at function exit.
// ===================================================================
int peephole_dead_stores(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        // We only care about pure MOV instructions that define a register
        if (curr->type == OP_MOV && curr->has_dst && curr->dst_op.mode == MODE_REG)
        {
            char *def_reg = curr->dst_op.reg;

            // Never optimize away stack frame manipulations
            if (!str_case_eq(def_reg, "SP") && !str_case_eq(def_reg, "BP"))
            {
                AsmNode *scan = curr->next;
                while (scan)
                {
                    if (scan->type == OP_OTHER && (scan->raw[0] == '\0' || scan->raw[0] == ';')) {
                        scan = scan->next;
                        continue;
                    }

                    // ----------------------------------------------------
                    // CRITICAL CHANGE 1:
                    // Do NOT break on OP_LABEL! It is mathematically safe to scan
                    // across labels for dead stores as long as we check reads.
                    // Only break on branching jumps (JMP) or function calls.
                    // ----------------------------------------------------
                    if (str_case_eq(scan->mnemonic, "JMP")  ||
                        str_case_eq(scan->mnemonic, "CALL") ||
                        str_case_eq(scan->mnemonic, "JT")   ||
                        str_case_eq(scan->mnemonic, "JF")   ||
                        str_case_eq(scan->mnemonic, "HLT"))
                    {
                        break;
                    }

                    // If any instruction READS our register, the store is live! Abort scan.
                    if (is_register_read(scan, def_reg)) {
                        break;
                    }

                    // ----------------------------------------------------
                    // CRITICAL CHANGE 2: Terminal Dead Store Check
                    // If we reach a RET instruction, check if def_reg is live-out!
                    // If it is NOT R0, SP, or BP, nobody will ever read it.
                    // ----------------------------------------------------
                    if (str_case_eq(scan->mnemonic, "RET"))
                    {
                        if (!is_live_out_register(def_reg)) {
                            curr->type = OP_OTHER;
                            snprintf(curr->raw, sizeof(curr->raw), "; optimized out terminal dead store: MOV %s", def_reg);
                            optimizations++;
                        }
                        break; // Stop scanning after RET
                    }

                    // Standard DSE: Another MOV overwrites our register before it was read
                    if (scan->type == OP_MOV && scan->has_dst &&
                        scan->dst_op.mode == MODE_REG && str_case_eq(scan->dst_op.reg, def_reg))
                    {
                        curr->type = OP_OTHER;
                        snprintf(curr->raw, sizeof(curr->raw), "; optimized out dead store: MOV %s", def_reg);
                        optimizations++;
                        break;
                    }

                    scan = scan->next;
                }
            }
        }
        curr = curr->next;
    }

    return optimizations;
}

// ===================================================================
// PEEPHOLE: Redundant Load Elimination
// Replaces a load from an address with a register if the value was
// just loaded into another register:
//   - MOV r1, [r2+off]; MOV r3, [r2+off] → MOV r3, r1
// ===================================================================
int peephole_loads(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr && curr->next)
    {
        AsmNode *n1 = curr;
        AsmNode *n2 = curr->next;

        // Skip if n1 is not a load (MOV from indirect address to register)
        if (n1->type != OP_MOV || n1->dst_op.mode != MODE_REG)
        {
            curr = curr->next;
            continue;
        }

        // Skip if n2 is not also a load
        if (n2->type != OP_MOV || n2->dst_op.mode != MODE_REG)
        {
            curr = curr->next;
            continue;
        }

        // --- Consecutive Loads from Same Address ---
        // If both loads read from the same memory location [reg+offset],
        // the second load can use the first's destination register instead.
        if (n1->src_op.mode == MODE_INDIRECT && n2->src_op.mode == MODE_INDIRECT &&
            str_case_eq(n1->src_op.reg, n2->src_op.reg) &&
            n1->src_op.offset == n2->src_op.offset)
        {
            // Replace n2's source (memory) with n1's destination (register)
            n2->src_op = n1->dst_op;
            snprintf(n2->raw, sizeof(n2->raw), "    MOV %s, %s", n2->dst_op.raw, n2->src_op.raw);
            optimizations++;
        }

        curr = curr->next;
    }
    return optimizations;
}

// ===================================================================
// PEEPHOLE: Immediate Propagation & Constant Folding
// ===================================================================
int peephole_immediate_prop(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        // ----------------------------------------------------------
        // PATTERN 1: Identity Math Elimination (e.g., IADD R1, 0)
        // ----------------------------------------------------------
        if (curr->type == OP_IADD || curr->type == OP_ISUB || curr->type == OP_IMUL || curr->type == OP_IDIV)
        {
            if (curr->has_src && curr->has_dst &&
                curr->src_op.mode == MODE_IMMEDIATE && curr->dst_op.mode == MODE_REG)
            {
                long val = parse_imm_val(curr->src_op.raw);
                bool is_identity = false;

                if ((curr->type == OP_IADD || curr->type == OP_ISUB) && val == 0) {
                    is_identity = true;
                } else if ((curr->type == OP_IMUL || curr->type == OP_IDIV) && val == 1) {
                    is_identity = true;
                }

                if (is_identity) {
                    curr->type = OP_OTHER;
                    snprintf(curr->raw, sizeof(curr->raw), "; optimized out identity: %s", curr->mnemonic);
                    optimizations++;
                    curr = curr->next;
                    continue;
                }
            }
        }

        // ----------------------------------------------------------
        // PATTERN 2: Constant Folding (MOV Reg, Imm -> next ALU Reg, Imm)
        // FIX: Only fold the VERY NEXT instruction (no scanning)
        // ----------------------------------------------------------
        if (curr->type == OP_MOV && curr->has_dst && curr->has_src &&
            curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_IMMEDIATE)
        {
            char *target_reg = curr->dst_op.reg;
            long imm1 = parse_imm_val(curr->src_op.raw);

            // Get the very next non-comment/blank instruction
            AsmNode *next_real = curr->next;
            while (next_real && next_real->type == OP_OTHER &&
                   (next_real->raw[0] == '\0' || next_real->raw[0] == ';')) {
                next_real = next_real->next;
            }

            if (!next_real || next_real->type == OP_OTHER || next_real->type == OP_LABEL) {
                curr = curr->next;
                continue;
            }

            // Only fold if it's IADD/ISUB/IMUL on target_reg with immediate
            if (next_real->has_dst && next_real->has_src &&
                next_real->dst_op.mode == MODE_REG &&
                str_case_eq(next_real->dst_op.reg, target_reg) &&
                next_real->src_op.mode == MODE_IMMEDIATE)
            {
                // Skip POW/ATAN2 (require register operands only)
                if (str_case_eq(next_real->mnemonic, "POW") ||
                    str_case_eq(next_real->mnemonic, "ATAN2")) {
                    curr = curr->next;
                    continue;
                }

                // Only fold IADD/ISUB/IMUL
                if (next_real->type != OP_IADD && next_real->type != OP_ISUB && next_real->type != OP_IMUL) {
                    curr = curr->next;
                    continue;
                }

                long imm2 = parse_imm_val(next_real->src_op.raw);
                long folded_val = 0;
                bool folded = false;

                if (next_real->type == OP_IADD) { folded_val = imm1 + imm2; folded = true; }
                else if (next_real->type == OP_ISUB) { folded_val = imm1 - imm2; folded = true; }
                else if (next_real->type == OP_IMUL) { folded_val = imm1 * imm2; folded = true; }

                if (folded) {
                    next_real->type = OP_MOV;
                    strcpy(next_real->mnemonic, "MOV");
                    next_real->has_dst = true;
                    next_real->has_src = true;
                    next_real->src_op.mode = MODE_IMMEDIATE;
                    next_real->src_op.offset = (int)folded_val;
                    snprintf(next_real->src_op.raw, sizeof(next_real->src_op.raw), "%ld", folded_val);
                    snprintf(next_real->raw, sizeof(next_real->raw), "    MOV %s, %ld",
                             next_real->dst_op.raw, folded_val);
                    optimizations++;
                }
            }
        }

        // ----------------------------------------------------------
        // PATTERN 3: Sequential Math Combining (IADD/ISUB Reg, Imm -> next IADD/ISUB Reg, Imm)
        // FIX: Only combine the VERY NEXT instruction
        // ----------------------------------------------------------
        else if ((curr->type == OP_IADD || curr->type == OP_ISUB) &&
                 curr->has_dst && curr->has_src &&
                 curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_IMMEDIATE)
        {
            char *target_reg = curr->dst_op.reg;
            long imm1 = parse_imm_val(curr->src_op.raw);
            if (curr->type == OP_ISUB) imm1 = -imm1;

            // Get the very next non-comment/blank instruction
            AsmNode *next_real = curr->next;
            while (next_real && next_real->type == OP_OTHER &&
                   (next_real->raw[0] == '\0' || next_real->raw[0] == ';')) {
                next_real = next_real->next;
            }

            if (!next_real || next_real->type == OP_OTHER || next_real->type == OP_LABEL) {
                curr = curr->next;
                continue;
            }

            // Only combine if it's IADD/ISUB on same register with immediate
            if (next_real->has_dst && next_real->has_src &&
                next_real->dst_op.mode == MODE_REG &&
                str_case_eq(next_real->dst_op.reg, target_reg) &&
                next_real->src_op.mode == MODE_IMMEDIATE &&
                (next_real->type == OP_IADD || next_real->type == OP_ISUB))
            {
                long imm2 = parse_imm_val(next_real->src_op.raw);
                if (next_real->type == OP_ISUB) imm2 = -imm2;

                long combined = imm1 + imm2;

                // Update type to match new mnemonic
                if (combined >= 0) {
                    curr->type = OP_IADD;
                    strcpy(curr->mnemonic, "IADD");
                } else {
                    curr->type = OP_ISUB;
                    strcpy(curr->mnemonic, "ISUB");
                    combined = -combined;
                }
                curr->src_op.offset = (int)combined;
                snprintf(curr->src_op.raw, sizeof(curr->src_op.raw), "%ld", combined);
                snprintf(curr->raw, sizeof(curr->raw), "    %s %s, %ld",
                         curr->mnemonic, curr->dst_op.raw, combined);

                next_real->type = OP_OTHER;
                snprintf(next_real->raw, sizeof(next_real->raw), "; optimized out combined math");
                optimizations++;
            }
        }

        curr = curr->next;
    }

    return optimizations;
}

// ===================================================================
// PEEPHOLE: Jump Chain Elimination
// Chains consecutive jumps to avoid indirection:
//   - JMP L1; L1: JMP L2 → JMP L2
// ===================================================================
int peephole_jmp_chain(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr && curr->next)
    {
        if (str_case_eq(curr->mnemonic, "JMP"))
        {
            // Skip over comments/blank lines to find the target label
            AsmNode *target = curr->next;
            while (target && target->type == OP_OTHER &&
                  (target->raw[0] == '\0' || target->raw[0] == ';'))
            {
                target = target->next;
            }

            // --- Jump to Label Followed by Another Jump ---
            if (target && target->type == OP_LABEL)
            {
                // FIX: Extract and verify the JMP's target matches this label
                char lbl[128] = {0};
                safe_str_copy(lbl, target->raw, sizeof(lbl));
                char *colon = strchr(lbl, ':');
                if (colon) *colon = '\0';
                trim(lbl);

                // Only chain if JMP targets this specific label
                if (!str_case_eq(trim(curr->dst_op.raw), lbl)) {
                    curr = curr->next;
                    continue;
                }

                // Find the instruction after the label (skip comments)
                AsmNode *next_after_label = target->next;
                while (next_after_label && next_after_label->type == OP_OTHER &&
                      (next_after_label->raw[0] == '\0' || next_after_label->raw[0] == ';'))
                {
                    next_after_label = next_after_label->next;
                }

                if (next_after_label && str_case_eq(next_after_label->mnemonic, "JMP"))
                {
                    // Update curr to jump directly to next_after_label's target
                    safe_str_copy(curr->dst_op.raw, next_after_label->dst_op.raw, sizeof(curr->dst_op.raw));
                    snprintf(curr->raw, sizeof(curr->raw), "    JMP %s", curr->dst_op.raw);

                    // Don't delete the label - just comment out the intermediate JMP
                    next_after_label->type = OP_OTHER;
                    snprintf(next_after_label->raw, sizeof(next_after_label->raw),
                             "; optimized out jump chain");
                    optimizations++;
                }
            }
        }
        curr = curr->next;
    }
    return optimizations;
}
