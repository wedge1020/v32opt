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
    if (!node || node->type == OP_OTHER || !reg_name) return false;

    // ------------------------------------------------------------------
    // CRITICAL VIRCON32 HARDWARE GUARD:
    // String instructions implicitly mutate DR, SR, and CR during execution!
    // Never allow copy propagation or constant folding to scan across them!
    // ------------------------------------------------------------------
    if (str_case_eq(node->mnemonic, "MOVS") ||
        str_case_eq(node->mnemonic, "SETS") ||
        str_case_eq(node->mnemonic, "CMPS"))
    {
        if (str_case_eq(reg_name, "DR") ||
            str_case_eq(reg_name, "SR") ||
            str_case_eq(reg_name, "CR")) {
            return true; // Stop scanning immediately!
        }
    }

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

                    // ----------------------------------------------------
                    // CRITICAL FIX: Add the missing global memory barrier here!
                    // Abort if MOVS, SETS, or CALL silently mutates memory!
                    // ----------------------------------------------------
                    if (is_global_memory_clobber(scan)) break;

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
                            // ARCHITECTURAL GUARD (UPGRADED):
                            // Check both mode AND raw syntax text ('[' or '_') to prevent
                            // forwarding non-registers into memory operations, protecting against
                            // AST parser default zero-initialization (MODE_REG == 0).
                            // ----------------------------------------------------
                            bool dst_is_mem = (scan->has_dst &&
                                              (scan->dst_op.mode == MODE_INDIRECT || strchr(scan->dst_op.raw, '[') != NULL));
                            bool src_is_not_reg = (curr->src_op.mode != MODE_REG ||
                                                  strchr(curr->src_op.raw, '_') != NULL ||
                                                  strchr(curr->src_op.raw, '[') != NULL);

                            bool is_illegal_mem_store = (dst_is_mem && src_is_not_reg);

                            if (!is_illegal_mem_store) // <-- Using upgraded syntax-aware guard
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
// PEEPHOLE: Redundant & Mirror Move Elimination (Upgraded Forward-Scanning)
// Eliminates redundant MOV instructions across basic blocks:
//   - Duplicate Moves: MOV r1, X; ... MOV r1, X
//   - Mirror Moves:    MOV r1, r2; ... MOV r2, r1
//   - Load-to-Store:   MOV r1, [mem]; ... MOV [mem], r1
// ===================================================================
int peephole_movs(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        if (curr->type == OP_MOV)
        {
            // Check for self-referential load (e.g. MOV R1, [R1])
            bool self_referential_load =
                (curr->src_op.mode == MODE_INDIRECT) &&
                str_case_eq(curr->src_op.reg, curr->dst_op.reg);

            AsmNode *scan = curr->next;
            while (scan)
            {
                if (scan->type == OP_OTHER || scan->type == OP_LABEL) {
                    scan = scan->next;
                    continue;
                }

                // Stop at control flow boundaries or global memory clobbers
                if (is_control_flow_boundary(scan)) break;
                if (is_global_memory_clobber(scan)) break;

                // --- 1. Duplicate Move Elimination ---
                // MOV dst, src; ... MOV dst, src
                if (!self_referential_load &&
                    scan->type == OP_MOV &&
                    str_case_eq(curr->dst_op.raw, scan->dst_op.raw) &&
                    str_case_eq(curr->src_op.raw, scan->src_op.raw))
                {
                    AsmNode *to_remove = scan;
                    scan = scan->next;
                    remove_node(to_remove);
                    optimizations++;
                    continue;
                }

                // --- 2. Mirror Move Elimination ---
                // MOV r1, r2; ... MOV r2, r1
                if (curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_REG &&
                    scan->type == OP_MOV &&
                    scan->dst_op.mode == MODE_REG && scan->src_op.mode == MODE_REG)
                {
                    if (str_case_eq(curr->dst_op.reg, scan->src_op.reg) &&
                        str_case_eq(curr->src_op.reg, scan->dst_op.reg))
                    {
                        AsmNode *to_remove = scan;
                        scan = scan->next;
                        remove_node(to_remove);
                        optimizations++;
                        continue;
                    }
                }

                // --- 3. Load-to-Store Elimination ---
                // MOV r1, [mem]; ... MOV [mem], r1
                if (curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_INDIRECT &&
                    scan->type == OP_MOV &&
                    scan->dst_op.mode == MODE_INDIRECT && scan->src_op.mode == MODE_REG)
                {
                    if (str_case_eq(curr->dst_op.reg, scan->src_op.reg) &&
                        str_case_eq(curr->src_op.reg, scan->dst_op.reg) &&
                        curr->src_op.offset == scan->dst_op.offset)
                    {
                        AsmNode *to_remove = scan;
                        scan = scan->next;
                        remove_node(to_remove);
                        optimizations++;
                        continue;
                    }
                }

                // Abort scanning if either operand of curr is modified downstream
                if (curr->dst_op.mode == MODE_REG && modifies_register(scan, curr->dst_op.reg)) break;
                if (curr->src_op.mode == MODE_REG && modifies_register(scan, curr->src_op.reg)) break;
                
                // If curr involves memory, abort if ANY direct store writes to memory
                if ((curr->dst_op.mode == MODE_INDIRECT || curr->src_op.mode == MODE_INDIRECT) &&
                    scan->type == OP_MOV && scan->dst_op.mode == MODE_INDIRECT) {
                    break;
                }

                scan = scan->next;
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
// HELPER: Check string instructions without triggering -Waddress warnings
// ===================================================================
bool is_string_instruction(AsmNode *node) {
    // Fixed -Waddress warning: mnemonic is an array, so check element [0]
    if (!node || node->mnemonic[0] == '\0') return false;

    // If an instruction was converted to a comment, it is no longer active
    const char *p = node->raw;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == ';' || *p == '\0') return false;

    return str_case_eq(node->mnemonic, "MOVS") ||
           str_case_eq(node->mnemonic, "SETS") ||
           str_case_eq(node->mnemonic, "CMPS");
}

// ===================================================================
// HELPER: Accurately identify comments or blank lines without accidentally
// skipping zero-operand string instructions like MOVS that may be tagged OP_OTHER.
// ===================================================================
bool is_comment_or_empty(AsmNode *node) {
    if (!node) return true;
    if (node->type == OP_LABEL) return false;

    // 1. Check if the raw assembly line is commented out or blank
    const char *p = node->raw;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == ';' || *p == '\0') return true;

    // 2. If tagged as OP_OTHER, it is a comment UNLESS it is an active string instruction
    if (node->type == OP_OTHER && !is_string_instruction(node)) return true;

    return false;
}

// ===================================================================
// HELPER: Check if an instruction reads a register
// ===================================================================
bool is_register_read(AsmNode *node, const char *reg_name) {
    if (!node || !reg_name || reg_name[0] == '\0') return false;

    // 1. ARCHITECTURAL GUARD: Check string instructions BEFORE ignoring anything!
    // String instructions implicitly read (and modify) DR, SR, and CR in hardware.
    if (is_string_instruction(node)) {
        if (str_case_eq(reg_name, "DR") ||
            str_case_eq(reg_name, "SR") ||
            str_case_eq(reg_name, "CR")) {
            return true;
        }
    }

    // 2. Ignore comments, blank lines, or labels
    if (is_comment_or_empty(node) || node->type == OP_LABEL) return false;

    // 3. Explicit source operand check
    if (node->has_src && node->src_op.mode == MODE_REG && node->src_op.reg[0] != '\0') {
        if (str_case_eq(node->src_op.reg, reg_name)) return true;
    }

    // 4. Memory dereferences (e.g., MOV R1, [R0] or MOV [R0], R1 both READ pointer R0)
    if (node->has_src && node->src_op.mode == MODE_INDIRECT && node->src_op.reg[0] != '\0') {
        if (str_case_eq(node->src_op.reg, reg_name)) return true;
    }
    if (node->has_dst && node->dst_op.mode == MODE_INDIRECT && node->dst_op.reg[0] != '\0') {
        if (str_case_eq(node->dst_op.reg, reg_name)) return true;
    }

    // 5. CRITICAL FIX: Destination Register Read-Modify-Write / Branch Testing!
    // If dst_op is a register, it is READ unless the instruction is a pure overwrite (MOV, IN, POP).
    // This protects math (IADD R1, R2), shifts (SHL R1, 2), and conditional branches (JT R1, label)!
    if (node->has_dst && node->dst_op.mode == MODE_REG && node->dst_op.reg[0] != '\0') {
        if (node->type != OP_MOV &&
            !str_case_eq(node->mnemonic, "IN") &&
            !str_case_eq(node->mnemonic, "POP")) {
            if (str_case_eq(node->dst_op.reg, reg_name)) return true;
        }
    }

    // 6. PUSH instructions read whatever register they push onto the stack
    if (node->type == OP_PUSH) {
        if (node->has_dst && node->dst_op.mode == MODE_REG && str_case_eq(node->dst_op.reg, reg_name)) return true;
        if (node->has_src && node->src_op.mode == MODE_REG && str_case_eq(node->src_op.reg, reg_name)) return true;
    }

    return false;
}

// ===================================================================
// HELPER: Check if an instruction is a pure register definition
// ===================================================================
bool is_pure_reg_def(AsmNode *node) {
    if (!node || is_comment_or_empty(node) || !node->has_dst || node->dst_op.mode != MODE_REG || node->dst_op.reg[0] == '\0') return false;

    if (is_string_instruction(node)) return false;

    // Do not treat stack ops, labels, or hardware I/O as dead stores
    if (node->type == OP_PUSH || node->type == OP_POP || node->type == OP_LABEL) return false;
    if (str_case_eq(node->mnemonic, "IN") || str_case_eq(node->mnemonic, "OUT")) return false;

    // Do not treat branches, calls, or halts as register definitions
    if (is_control_flow_boundary(node) ||
        str_case_eq(node->mnemonic, "CALL") ||
        str_case_eq(node->mnemonic, "JMP")  ||
        str_case_eq(node->mnemonic, "JT")   ||
        str_case_eq(node->mnemonic, "JF")   ||
        str_case_eq(node->mnemonic, "CIB")  ||
        str_case_eq(node->mnemonic, "RET")  ||
        str_case_eq(node->mnemonic, "HLT")) {
        return false;
    }

    return true;
}

// ===================================================================
// PEEPHOLE: Dead Store Elimination (DSE) - Upgraded & Safe!
// Eliminates register writes overwritten OR dead at function exit.
// ===================================================================
int peephole_dead_stores(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        if (is_pure_reg_def(curr))
        {
            char *def_reg = curr->dst_op.reg;

            // Never optimize away stack frame manipulations
            if (!str_case_eq(def_reg, "SP") && !str_case_eq(def_reg, "BP"))
            {
                AsmNode *scan = curr->next;
                while (scan)
                {
                    if (is_comment_or_empty(scan) || scan->type == OP_LABEL) {
                        scan = scan->next;
                        continue;
                    }

                    // 1. CRITICAL FIX: Stop scanning at ANY control flow boundary!
                    // This prevents DSE from scanning across JT/JF branches and deleting
                    // registers that are required on the branched-to path.
                    if (is_control_flow_boundary(scan) ||
                        str_case_eq(scan->mnemonic, "JMP") ||
                        str_case_eq(scan->mnemonic, "JT")  ||
                        str_case_eq(scan->mnemonic, "JF")  ||
                        str_case_eq(scan->mnemonic, "CIB") ||
                        str_case_eq(scan->mnemonic, "CALL") ||
                        str_case_eq(scan->mnemonic, "RET") ||
                        str_case_eq(scan->mnemonic, "HLT")) {

                        // Terminal Dead Store Check (at RET instruction)
                        if (str_case_eq(scan->mnemonic, "RET"))
                        {
                            // CRITICAL FIX: Never delete R0 (return value register), SP, or BP at function exit!
                            if (!str_case_eq(def_reg, "R0") && !str_case_eq(def_reg, "SP") && !str_case_eq(def_reg, "BP")) {
                                if (!is_live_out_register(def_reg)) {
                                    curr->type = OP_OTHER;
                                    curr->has_dst = false;
                                    curr->has_src = false;
                                    curr->dst_op.reg[0] = '\0';
                                    snprintf(curr->raw, sizeof(curr->raw), "; optimized out terminal dead store: %s %s", curr->mnemonic, def_reg);
                                    optimizations++;
                                }
                            }
                        }
                        break; // Stop scanning at the boundary!
                    }

                    // 2. If any instruction READS our register, the store is live! Abort scan.
                    if (is_register_read(scan, def_reg)) {
                        break;
                    }

                    // 3. Standard DSE: Another instruction overwrites our register before it was read
                    if (is_pure_reg_def(scan) && str_case_eq(scan->dst_op.reg, def_reg))
                    {
                        curr->type = OP_OTHER;
                        curr->has_dst = false;
                        curr->has_src = false;
                        curr->dst_op.reg[0] = '\0';
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
// PEEPHOLE: Redundant Load Elimination (Upgraded Forward-Scanning)
// Replaces a load from an address with a register if the value was
// just loaded into another register:
//   - MOV r1, [r2+off]; ... MOV r3, [r2+off] → MOV r3, r1
// ===================================================================
int peephole_loads(AsmNode *head)
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

        // Match: An initial load from memory into a register (MOV Reg, [Mem])
        if (curr->type == OP_MOV &&
            curr->dst_op.mode == MODE_REG &&
            curr->src_op.mode == MODE_INDIRECT)
        {
            char *loaded_reg = curr->dst_op.reg;
            char *mem_reg    = curr->src_op.reg;
            int mem_off      = curr->src_op.offset;

            AsmNode *scan = curr->next;
            while (scan)
            {
                if (scan->type == OP_OTHER) {
                    scan = scan->next;
                    continue;
                }

                // 1. Stop at control flow boundaries (labels, jumps, returns)
                if (is_control_flow_boundary(scan)) break;

                // ----------------------------------------------------
                // 2. ARCHITECTURAL MEMORY BARRIER:
                // Abort if MOVS, SETS, or CALL silently mutates memory!
                // ----------------------------------------------------
                if (is_global_memory_clobber(scan)) break;

                // 3. Abort if the address pointer or our cached register is overwritten
                if (modifies_register(scan, mem_reg) || modifies_register(scan, loaded_reg)) break;

                // 4. Abort if ANY direct store writes to memory (to prevent aliasing hazards)
                if (scan->type == OP_MOV && scan->dst_op.mode == MODE_INDIRECT) {
                    break;
                }

                // 5. Match: A downstream load reading from the exact same memory location!
                if (scan->type == OP_MOV &&
                    scan->dst_op.mode == MODE_REG &&
                    scan->src_op.mode == MODE_INDIRECT)
                {
                    if (str_case_eq(scan->src_op.reg, mem_reg) && scan->src_op.offset == mem_off)
                    {
                        // Rewrite: MOV new_reg, [mem] -> MOV new_reg, loaded_reg
                        scan->src_op = curr->dst_op;
                        snprintf(scan->raw, sizeof(scan->raw), "    MOV %s, %s",
                                 scan->dst_op.raw, scan->src_op.raw);
                        optimizations++;
                    }
                }

                scan = scan->next;
            }
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
// PEEPHOLE: Jump Chain Elimination (FIXED & SAFE)
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
            AsmNode *target = curr->next;
            while (target && target->type == OP_OTHER) {
                target = target->next;
            }

            if (target && target->type == OP_LABEL)
            {
                AsmNode *next_after_label = target->next;
                while (next_after_label && next_after_label->type == OP_OTHER) {
                    next_after_label = next_after_label->next;
                }

                if (next_after_label && str_case_eq(next_after_label->mnemonic, "JMP"))
                {
                    // Guard against infinite compiler loops if a label jumps to itself!
                    if (!str_case_eq(curr->dst_op.raw, next_after_label->dst_op.raw)) 
                    {
                        // Rewrite: JMP L1 -> JMP L2
                        safe_str_copy(curr->dst_op.raw, next_after_label->dst_op.raw, sizeof(curr->dst_op.raw));
                        snprintf(curr->raw, sizeof(curr->raw), "    JMP %s", curr->dst_op.raw);
                        optimizations++;
                        
                        // CRITICAL FIX: We DO NOT remove target (L1) or next_after_label (JMP L2)!
                        // Other branches in the program may still rely on L1 existing.
                    }
                }
            }
        }
        curr = curr->next;
    }
    return optimizations;
}
