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
        // CRITICAL FIX: Immediately skip comments, blank lines, and labels!
        if (curr->type == OP_OTHER || curr->type == OP_LABEL) {
            curr = curr->next;
            continue;
        }

        AsmNode *n1 = curr;
        AsmNode *n2 = curr->next;

        // --- IEQ/INE + CIB Redundancy ---
        // IEQ/INE sets a flag; CIB branches on that flag.
        // If they target the same register, the CIB is redundant because
        // the flag is already set and the branch will use it directly.
        if ((n1->type == OP_IEQ || n1->type == OP_INE) &&
             n2->type == OP_CIB &&
             n1->dst_op.mode == MODE_REG && n2->dst_op.mode == MODE_REG &&
             str_case_eq(n1->dst_op.reg, n2->dst_op.reg))
        {
            remove_node(n2);
            optimizations++;
            continue; // Skip n2, continue from n1->next (which is now n2->next)
        }

        // ----------------------------------------------------------------
        // PATTERN: Self-Inverting Pairs (Involutions)
        // Identical consecutive operations that cancel each other out:
        //   - BNOT R4 ; BNOT R4 -> (Bitwise NOT twice = Identity)
        //   - INEG R4 ; INEG R4 -> (Two's complement negate twice = Identity)
        //   - NOT R4  ; NOT R4  -> (Logical NOT twice = Identity)
        // ----------------------------------------------------------------
        if (str_case_eq(curr->mnemonic, "BNOT") ||
            str_case_eq(curr->mnemonic, "INEG") ||
            str_case_eq(curr->mnemonic, "NEG")  ||
            str_case_eq(curr->mnemonic, "NOT"))
        {
            AsmNode *next = curr->next;
            // Safely skip any inline comments or blank lines between the pair
            while (next && next->type == OP_OTHER) next = next->next;

            if (next && str_case_eq(next->mnemonic, curr->mnemonic))
            {
                // Safely extract the target register regardless of whether your AST
                // stores 1-operand targets in dst_op or src_op!
                char *reg1 = curr->has_dst ? curr->dst_op.reg : (curr->has_src ? curr->src_op.reg : NULL);
                char *reg2 = next->has_dst ? next->dst_op.reg : (next->has_src ? next->src_op.reg : NULL);

                if (reg1 && reg2 && str_case_eq(reg1, reg2))
                {
                    // Both instructions cancel out! Convert both to comments.
                    curr->type = OP_OTHER;
                    snprintf(curr->raw, sizeof(curr->raw), "; optimized out pair: %s %s", curr->mnemonic, reg1);

                    next->type = OP_OTHER;
                    snprintf(next->raw, sizeof(next->raw), "; optimized out pair: %s %s", next->mnemonic, reg2);

                    optimizations += 2;
                    curr = next; // Fast-forward loop past the second instruction
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
        if (str_case_eq(curr->mnemonic, "XOR"))
        {
            AsmNode *next = curr->next;
            while (next && next->type == OP_OTHER) next = next->next;

            if (next && str_case_eq(next->mnemonic, "XOR"))
            {
                // Verify destination registers match
                if (curr->has_dst && next->has_dst && str_case_eq(curr->dst_op.reg, next->dst_op.reg))
                {
                    bool src_match = false;

                    // Check if both XOR with the same register
                    if (curr->src_op.mode == MODE_REG && next->src_op.mode == MODE_REG) {
                        if (str_case_eq(curr->src_op.reg, next->src_op.reg)) src_match = true;
                    }
                    // Check if both XOR with the exact same immediate value
                    else if (curr->src_op.mode == MODE_IMMEDIATE && next->src_op.mode == MODE_IMMEDIATE) {
                        if (curr->src_op.offset == next->src_op.offset &&
                            str_case_eq(curr->src_op.raw, next->src_op.raw)) src_match = true;
                    }

                    if (src_match)
                    {
                        curr->type = OP_OTHER;
                        snprintf(curr->raw, sizeof(curr->raw), "; optimized out pair: XOR toggle");
                        next->type = OP_OTHER;
                        snprintf(next->raw, sizeof(next->raw), "; optimized out pair: XOR toggle");
                        optimizations += 2;
                        curr = next;
                        continue;
                    }
                }
            }
        }

        // --- PUSH/POP Pair Elimination ---
        // PUSH r; POP r → no net effect on the stack or register
        if (n1->type == OP_PUSH && n2->type == OP_POP &&
            n1->dst_op.mode == MODE_REG && n2->dst_op.mode == MODE_REG &&
            str_case_eq(n1->dst_op.reg, n2->dst_op.reg))
        {
            AsmNode *next_iter = n2->next;
            remove_node(n1);
            remove_node(n2);
            curr = next_iter;
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

        curr = next;
    }

    return optimizations;
}

// ===================================================================
// HELPER: Check if an instruction modifies a specific register
// ===================================================================
bool modifies_register(AsmNode *node, const char *reg_name) {
    // CRITICAL FIX: Ignore commented-out instructions!
    if (!node || node->type == OP_OTHER || !reg_name) return false;

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
            // ----------------------------------------------------------
            // RULE 1: Store-to-Load Forwarding
            // Forward value from memory store directly to downstream loads
            // ----------------------------------------------------------
            if (curr->dst_op.mode == MODE_INDIRECT && curr->src_op.mode == MODE_REG)
            {
                char *mem_reg = curr->dst_op.reg;
                int mem_off   = curr->dst_op.offset;
                char *src_reg = curr->src_op.reg;

                AsmNode *scan = curr->next;
                while (scan)
                {
                    if (scan->type == OP_OTHER) {
                        scan = scan->next;
                        continue;
                    }

                    // Stop scanning at control flow boundaries or if registers change
                    if (is_control_flow_boundary(scan)) break;
                    if (modifies_register(scan, mem_reg) || modifies_register(scan, src_reg)) break;

                    // Stop if another store overwrites this exact memory location
                    if (scan->type == OP_MOV && scan->dst_op.mode == MODE_INDIRECT) {
                        if (str_case_eq(scan->dst_op.reg, mem_reg) && scan->dst_op.offset == mem_off) break;
                    }

                    // Match: A load reading from the exact same memory location
                    if (scan->type == OP_MOV &&
                        scan->dst_op.mode == MODE_REG &&
                        scan->src_op.mode == MODE_INDIRECT)
                    {
                        if (str_case_eq(scan->src_op.reg, mem_reg) && scan->src_op.offset == mem_off)
                        {
                            scan->src_op = curr->src_op;
                            snprintf(scan->raw, sizeof(scan->raw), "    MOV %s, %s",
                                     scan->dst_op.raw, scan->src_op.raw);
                            optimizations++;
                        }
                    }
                    scan = scan->next;
                }
            }

            // ----------------------------------------------------------
            // RULE 2: Register & Immediate Copy Propagation
            // Forward registers/immediates to downstream reading instructions
            // ----------------------------------------------------------
            else if (curr->dst_op.mode == MODE_REG &&
                    (curr->src_op.mode == MODE_REG || curr->src_op.mode == MODE_IMMEDIATE))
            {
                char *def_reg = curr->dst_op.reg;

                // Protect stack frame pointers (SP/BP) from being forwarded/mangled
                if (!str_case_eq(def_reg, "SP") && !str_case_eq(def_reg, "BP"))
                {
                    AsmNode *scan = curr->next;
                    while (scan)
                    {
                        if (scan->type == OP_OTHER) {
                            scan = scan->next;
                            continue;
                        }

                        if (is_control_flow_boundary(scan)) break;

                        // If forwarding a register, abort if that source value gets overwritten
                        if (curr->src_op.mode == MODE_REG && modifies_register(scan, curr->src_op.reg)) {
                            break;
                        }

                        // Match: Downstream instruction reads def_reg in its SOURCE operand
                        // CRITICAL: Only match src_op! Never match dst_op!
                        if (scan->has_src && scan->src_op.mode == MODE_REG &&
                            str_case_eq(scan->src_op.reg, def_reg))
                        {
                            // ----------------------------------------------------
                            // ARCHITECTURAL GUARD:
                            // Vircon32 requires at least one real register (MODE_REG) in MOV/ALU instructions.
                            // If the downstream instruction's destination is NOT a direct register (e.g., memory),
                            // we CANNOT forward anything into its source unless what we are forwarding is ALSO a register!
                            // ----------------------------------------------------
                            bool is_illegal_mem_store = (curr->src_op.mode != MODE_REG &&
                                                         scan->has_dst &&
                                                         scan->dst_op.mode != MODE_REG);

                            if (!is_illegal_mem_store) // <-- Using broadened guard
                            {
                                scan->src_op = curr->src_op;

                                // Safely reconstruct the raw assembly text
                                if (scan->has_dst) {
                                    snprintf(scan->raw, sizeof(scan->raw), "    %s %s, %s",
                                             scan->mnemonic, scan->dst_op.raw, scan->src_op.raw);
                                } else {
                                    snprintf(scan->raw, sizeof(scan->raw), "    %s %s",
                                             scan->mnemonic, scan->src_op.raw);
                                }
                                optimizations++;
                            }
                        }

                        // If the instruction overwrites def_reg, it is dead downstream. Stop scanning!
                        if (modifies_register(scan, def_reg)) break;

                        scan = scan->next;
                    }
                }
            }
        }
        curr = curr->next;
    }

    return optimizations;
}

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

            // For fast-forwarding a pointer to the next real instruction:
            while (next_node && next_node->type == OP_OTHER) {
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

            // For fast-forwarding a pointer to the next real instruction:
            while (n2 && n2->type == OP_OTHER) {
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
            // Skip over comments/blank lines to find the next real instruction
            AsmNode *n2 = curr->next;

            // For fast-forwarding a pointer to the next real instruction:
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
                // If the combined value is 0, both operations cancel out
                if (combined == 0)
                {
                    AsmNode *next_iter = n2->next;
                    remove_node(curr);
                    remove_node(n2);
                    curr = next_iter;
                    optimizations += 2;
                    continue;
                }
                // --- Non-Zero Combination ---
                // Replace the first instruction with the combined operation
                else if (combined > 0)
                {
                    curr->type = OP_IADD;
                    safe_str_copy(curr->mnemonic, "IADD", sizeof(curr->mnemonic));
                    curr->src_op.immediate = combined;
                    snprintf(curr->src_op.raw, sizeof(curr->src_op.raw), "%d", combined);
                    snprintf(curr->raw, sizeof(curr->raw), "    IADD %s, %d", curr->dst_op.raw, combined);
                }
                else // combined < 0
                {
                    curr->type = OP_ISUB;
                    safe_str_copy(curr->mnemonic, "ISUB", sizeof(curr->mnemonic));
                    curr->src_op.immediate = -combined; // Make positive for ISUB
                    snprintf(curr->src_op.raw, sizeof(curr->src_op.raw), "%d", -combined);
                    snprintf(curr->raw, sizeof(curr->raw), "    ISUB %s, %d", curr->dst_op.raw, -combined);
                }
                remove_node(n2);
                optimizations++;
                continue;
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
// HELPER: Robustly check if an instruction READS a specific register
// ===================================================================
bool is_register_read(AsmNode *node, const char *reg_name) {
    // CRITICAL FIX: Ignore commented-out instructions!
    if (!node || node->type == OP_OTHER || !reg_name) return false;

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
// HELPER: Check if an instruction is a pure register definition
// ===================================================================
bool is_pure_reg_def(AsmNode *node) {
    // CRITICAL FIX: Ignore instructions that have already been converted to comments!
    if (!node || node->type == OP_OTHER || !node->has_dst || node->dst_op.mode != MODE_REG) return false;

    // Exclude instructions with architectural side effects
    if (node->type == OP_PUSH || node->type == OP_POP || node->type == OP_LABEL) return false;
    if (str_case_eq(node->mnemonic, "CALL") ||
        str_case_eq(node->mnemonic, "JMP")  ||
        str_case_eq(node->mnemonic, "CIB")  ||
        str_case_eq(node->mnemonic, "RET")  ||
        str_case_eq(node->mnemonic, "HLT")) {
        return false;
    }

    return true;
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
        // Check for any pure register definition (MOV, IADD, IMUL, SHL, etc.)
        if (is_pure_reg_def(curr))
        {
            char *def_reg = curr->dst_op.reg;

            // Never optimize away stack frame manipulations
            if (!str_case_eq(def_reg, "SP") && !str_case_eq(def_reg, "BP"))
            {
                AsmNode *scan = curr->next;
                while (scan)
                {
                    if (scan->type == OP_OTHER) {
                        scan = scan->next;
                        continue;
                    }

                    // ----------------------------------------------------
                    // CRITICAL CHANGE 1:
                    // Do NOT break on OP_LABEL! It is mathematically safe to scan
                    // across labels for dead stores as long as we check reads.
                    // Only break on branching jumps (JMP, CIB) or function calls.
                    // ----------------------------------------------------
                    if (str_case_eq(scan->mnemonic, "JMP") ||
                        str_case_eq(scan->mnemonic, "CIB") ||
                        str_case_eq(scan->mnemonic, "CALL") ||
                        str_case_eq(scan->mnemonic, "HLT")) {
                        break;
                    }

                    // If any instruction READS our register, the store is live! Abort scan.
                    if (is_register_read(scan, def_reg)) {
                        break;
                    }

                    // Terminal Dead Store Check (at RET instruction)
                    if (str_case_eq(scan->mnemonic, "RET"))
                    {
                        if (!is_live_out_register(def_reg)) {
                            curr->type = OP_OTHER;
                            snprintf(curr->raw, sizeof(curr->raw), "; optimized out terminal dead store: %s %s", curr->mnemonic, def_reg);
                            optimizations++;
                        }
                        break;
                    }

                    // Standard DSE: Another instruction overwrites our register before it was read
                    if (is_pure_reg_def(scan) && str_case_eq(scan->dst_op.reg, def_reg))
                    {
                        curr->type = OP_OTHER;
                        snprintf(curr->raw, sizeof(curr->raw), "; optimized out dead store: %s %s", curr->mnemonic, def_reg);
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
// HELPER: Safely parse integer immediates (hex, decimal, negative)
// ===================================================================

long parse_imm_val(const char *raw_val) {
    if (!raw_val || raw_val[0] == '\0') return 0;
    return strtol(raw_val, NULL, 0);
}

// ===================================================================
// PEEPHOLE: Immediate Propagation & Constant Folding
// Targets ALU math folding, sequential combining, and identity removal
// ===================================================================
int peephole_immediate_prop(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        // CRITICAL FIX: Immediately skip comments, blank lines, and labels!
        if (curr->type == OP_OTHER || curr->type == OP_LABEL) {
            curr = curr->next;
            continue;
        }

        // ----------------------------------------------------------
        // PATTERN 1: Identity Math Elimination (e.g., IADD R1, 0)
        // ----------------------------------------------------------
        if (curr->has_src && curr->src_op.mode == MODE_IMMEDIATE &&
            curr->has_dst && curr->dst_op.mode == MODE_REG)
        {
            long val = parse_imm_val(curr->src_op.raw);
            bool is_identity = false;

            if ((str_case_eq(curr->mnemonic, "IADD") || str_case_eq(curr->mnemonic, "ISUB")) && val == 0) {
                is_identity = true;
            } else if ((str_case_eq(curr->mnemonic, "IMUL") || str_case_eq(curr->mnemonic, "IDIV")) && val == 1) {
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

        // ----------------------------------------------------------
        // PATTERN 2: Constant Folding (MOV Reg, Imm1 -> ... -> ALU Reg, Imm2)
        // ----------------------------------------------------------
        if (curr->type == OP_MOV && curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_IMMEDIATE)
        {
            char *target_reg = curr->dst_op.reg;
            long imm1 = parse_imm_val(curr->src_op.raw);

            AsmNode *scan = curr->next;
            while (scan)
            {
                if (scan->type == OP_OTHER) {
                    scan = scan->next;
                    continue;
                }
                if (is_control_flow_boundary(scan)) break;

                // If target_reg is read as a SOURCE (e.g., in MOV [SP], R1), we cannot fold
                // across it without altering that intermediate read! Stop scanning.
                if (scan->has_src && scan->src_op.mode == MODE_REG && str_case_eq(scan->src_op.reg, target_reg)) {
                    break;
                }

                // Match: An ALU instruction modifying our target_reg with an immediate
                if (scan->has_dst && scan->dst_op.mode == MODE_REG &&
                    str_case_eq(scan->dst_op.reg, target_reg) &&
                    scan->has_src && scan->src_op.mode == MODE_IMMEDIATE)
                {
                    long imm2 = parse_imm_val(scan->src_op.raw);
                    long folded_val = 0;
                    bool folded = false;

                    if (str_case_eq(scan->mnemonic, "IADD")) { folded_val = imm1 + imm2; folded = true; }
                    else if (str_case_eq(scan->mnemonic, "ISUB")) { folded_val = imm1 - imm2; folded = true; }
                    else if (str_case_eq(scan->mnemonic, "IMUL")) { folded_val = imm1 * imm2; folded = true; }

                    if (folded) {
                        // Transform downstream ALU op directly into a MOV with the new result!
                        scan->type = OP_MOV;
                        strcpy(scan->mnemonic, "MOV");
                        scan->src_op.offset = (int)folded_val;
                        snprintf(scan->src_op.raw, sizeof(scan->src_op.raw), "%ld", folded_val);
                        snprintf(scan->raw, sizeof(scan->raw), "    MOV %s, %ld", scan->dst_op.raw, folded_val);

                        optimizations++;
                        break; // The math is folded; stop scanning for this MOV
                    }
                }

                // If anything else modifies target_reg, our immediate is dead. Stop scanning.
                if (modifies_register(scan, target_reg)) break;

                scan = scan->next;
            }
        }

        // ----------------------------------------------------------
        // PATTERN 3: Sequential Math Combining (IADD Reg, Imm1 -> IADD Reg, Imm2)
        // ----------------------------------------------------------
        else if ((str_case_eq(curr->mnemonic, "IADD") || str_case_eq(curr->mnemonic, "ISUB")) &&
                 curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_IMMEDIATE)
        {
            char *target_reg = curr->dst_op.reg;
            long imm1 = parse_imm_val(curr->src_op.raw);
            if (str_case_eq(curr->mnemonic, "ISUB")) imm1 = -imm1; // Normalize to addition

            AsmNode *scan = curr->next;
            while (scan)
            {
                if (scan->type == OP_OTHER) {
                    scan = scan->next;
                    continue;
                }
                if (is_control_flow_boundary(scan)) break;
                if (scan->has_src && scan->src_op.mode == MODE_REG && str_case_eq(scan->src_op.reg, target_reg)) break;

                if ((str_case_eq(scan->mnemonic, "IADD") || str_case_eq(scan->mnemonic, "ISUB")) &&
                    scan->dst_op.mode == MODE_REG && str_case_eq(scan->dst_op.reg, target_reg) &&
                    scan->src_op.mode == MODE_IMMEDIATE)
                {
                    long imm2 = parse_imm_val(scan->src_op.raw);
                    if (str_case_eq(scan->mnemonic, "ISUB")) imm2 = -imm2;

                    long combined = imm1 + imm2;

                    // Update curr to hold the combined value
                    strcpy(curr->mnemonic, "IADD");
                    if (combined < 0) {
                        strcpy(curr->mnemonic, "ISUB");
                        combined = -combined;
                    }
                    curr->src_op.offset = (int)combined;
                    snprintf(curr->src_op.raw, sizeof(curr->src_op.raw), "%ld", combined);
                    snprintf(curr->raw, sizeof(curr->raw), "    %s %s, %ld", curr->mnemonic, curr->dst_op.raw, combined);

                    // Convert the downstream redundant math into a comment
                    scan->type = OP_OTHER;
                    snprintf(scan->raw, sizeof(scan->raw), "; optimized out combined math");

                    optimizations++;
                    break;
                }

                if (modifies_register(scan, target_reg)) break;
                scan = scan->next;
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

            // For fast-forwarding a pointer to the next real instruction:
            while (target && target->type == OP_OTHER) {
                target = target->next;
            }

            // --- Jump to Label Followed by Another Jump ---
            // If the target is a label and the next instruction after it is a JMP,
            // we can chain the jumps: JMP L1; L1: JMP L2 → JMP L2
            if (target && target->type == OP_LABEL)
            {
                // Find the instruction after the label (skip comments)
                AsmNode *next_after_label = target->next;

                // For fast-forwarding a pointer to the next real instruction:
                while (next_after_label && next_after_label->type == OP_OTHER) {
                    next_after_label = next_after_label->next;
                }

                if (next_after_label && str_case_eq(next_after_label->mnemonic, "JMP"))
                {
                    // Update curr to jump directly to next_after_label's target
                    safe_str_copy(curr->dst_op.raw, next_after_label->dst_op.raw, sizeof(curr->dst_op.raw));
                    snprintf(curr->raw, sizeof(curr->raw), "    JMP %s", curr->dst_op.raw);

                    // Remove the intermediate label and JMP
                    remove_node(target);
                    remove_node(next_after_label);
                    optimizations += 2;
                }
            }
        }
        curr = curr->next;
    }
    return optimizations;
}
