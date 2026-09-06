#include "v32opt.h"

// ===================================================================
// PEEPHOLE: Algebraic Simplification
//
// Removes or replaces instructions that are algebraically redundant
// or can be simplified to cheaper equivalents.
//
// Patterns handled:
//   - MOV r, r → remove (no-op, self-move)
//   - IADD/ISUB r, 0 → remove (identity operation)
//   - IMUL r, 2 → replace with IADD r, r (cost-neutral, more idiomatic)
//   - IMUL r, 0 → replace with MOV r, 0
//   - IMUL r, 1 → remove (identity)
//   - IDIV r, 1 → remove (identity)
//
// Example:
//   Input:  MOV R1, R1
//   Output: (removed)
//
//   Input:  IMUL R2, 2
//   Output: IADD R2, R2
//
// Returns: Number of optimizations applied
// ===================================================================
int peephole_algebra(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr != NULL)
    {
        AsmNode *next = curr->next;

        // --- MOV r, r (Self-Move Elimination) ---
        if (curr->type == OP_MOV &&
            curr->dst_op.mode == MODE_REG && curr->src_op.mode == MODE_REG &&
            str_case_eq(curr->dst_op.reg, curr->src_op.reg))
        {
            // remove_with_debug handles the debug comment
            AsmNode *nodes[] = {curr};
            if (remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_ALGEBRA)) optimizations++;
            continue;
        }

        // --- IADD/ISUB with Immediate 0 ---
        if ((curr->type == OP_IADD || curr->type == OP_ISUB) &&
            is_numeric_immediate(&curr->src_op) &&
            curr->src_op.immediate == 0)
        {
            // remove_with_debug handles the debug comment
            AsmNode *nodes[] = {curr};
            if (remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_ALGEBRA)) optimizations++;
            continue;
        }

        // --- IMUL by 2 Strength Reduction ---
        if (curr->type == OP_IMUL &&
            curr->dst_op.mode == MODE_REG &&
            is_numeric_immediate(&curr->src_op) &&
            curr->src_op.immediate == 2 &&
            trigger_allowed())
        {
            // Keep this debug comment, as we are mutating, not removing
            insert_debug_comment(curr->prev, OPT_PEEPHOLE_ALGEBRA, curr->raw);
            curr->type = OP_IADD;
            strcpy(curr->mnemonic, "IADD");
            curr->src_op = curr->dst_op;
            snprintf(curr->raw, sizeof(curr->raw), "    IADD %s, %s",
                     curr->dst_op.raw, curr->src_op.raw);
            optimizations++;
        }

        // --- IMUL by 0 → MOV r, 0 ---
        if (curr->type == OP_IMUL &&
            curr->dst_op.mode == MODE_REG &&
            is_numeric_immediate(&curr->src_op) &&
            curr->src_op.immediate == 0 &&
            trigger_allowed())
        {
            // Keep this debug comment, as we are mutating, not removing
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
            is_numeric_immediate(&curr->src_op) &&
            curr->src_op.immediate == 1)
        {
            // remove_with_debug handles the debug comment
            AsmNode *nodes[] = {curr};
            if (remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_ALGEBRA)) optimizations++;
            continue;
        }

        // --- IDIV by 1 → Remove (identity) ---
        if (curr->type == OP_IDIV &&
            curr->dst_op.mode == MODE_REG &&
            is_numeric_immediate(&curr->src_op) &&
            curr->src_op.immediate == 1)
        {
            // remove_with_debug handles the debug comment
            AsmNode *nodes[] = {curr};
            if (remove_with_debug(&curr, nodes, 1, OPT_PEEPHOLE_ALGEBRA)) optimizations++;
            continue;
        }

        // --- LUA MODE: drop the impossible-Nil half of a falsy-check ---
        // v32lua boxes a comparison result as a Lua boolean via
        // "IADD Rd, BOXED_BOOLEAN" -- after that, Rd can only ever hold
        // BOXED_TRUE or BOXED_FALSE, never BOXED_NIL. But every consumer
        // that only wants truthiness (emit_falsy_jump() in v32lua.c) always
        // re-derives it with a fixed, unconditional 6-instruction template
        // that ALSO tests for Nil first:
        //   MOV Rs,Rd ; IEQ Rs,BOXED_NIL ; JT Rs,L ; MOV Rs,Rd ; IEQ Rs,BOXED_FALSE ; JT Rs,L
        // When we can prove Rd was JUST produced by the boxed-boolean IADD
        // (nothing in between writes Rd), the first 3 instructions test for
        // a value Rd provably can't hold, so they're dead code -- remove
        // them and keep only the BOXED_FALSE check.
        //
        // NOTE: this also transparently covers emit_truthy_jump()'s shape.
        // Despite having a 3rd distinct target (the trailing JMP's
        // short-circuit target), emit_truthy_jump()'s own two JT branches
        // -- like emit_falsy_jump()'s -- both target the SAME label (its
        // internal "not truthy, go evaluate the right operand" label); the
        // JMP to the 3rd target only runs if both JTs fall through. Since
        // this match only looks at the first six instructions (both JTs'
        // shared target), it fires identically for both templates -- the
        // trailing JMP+label a truthy_jump leaves behind afterwards is
        // irrelevant to this match and untouched by it. Verified against
        // real v32lua output: fires on both emit_falsy_jump() call sites
        // (and/or short-circuit chains) and emit_truthy_jump() call sites
        // (e.g. "..._truthy_fail_N" labels) alike.
        //
        if (is_lua_mode() && curr->type == OP_IADD && curr->dst_op.mode == MODE_REG &&
            is_boxed_type_operand(&curr->src_op) &&
            str_case_eq(curr->src_op.raw, "BOXED_BOOLEAN"))
        {
            char *reg_name = curr->dst_op.reg;

            AsmNode *mov1 = skip_other_nodes(curr->next);
            if (mov1 && mov1->type == OP_MOV && mov1->dst_op.mode == MODE_REG &&
                mov1->src_op.mode == MODE_REG && str_case_eq(mov1->src_op.reg, reg_name) &&
                !str_case_eq(mov1->dst_op.reg, reg_name))
            {
                char *scratch_reg = mov1->dst_op.reg;

                AsmNode *ieq1 = skip_other_nodes(mov1->next);
                AsmNode *jt1  = ieq1 ? skip_other_nodes(ieq1->next) : NULL;
                AsmNode *mov2 = jt1  ? skip_other_nodes(jt1->next)  : NULL;
                AsmNode *ieq2 = mov2 ? skip_other_nodes(mov2->next) : NULL;

                if (ieq1 && ieq1->type == OP_IEQ && ieq1->dst_op.mode == MODE_REG &&
                    str_case_eq(ieq1->dst_op.reg, scratch_reg) &&
                    is_boxed_type_operand(&ieq1->src_op) &&
                    str_case_eq(ieq1->src_op.raw, "BOXED_NIL") &&

                    jt1 && jt1->type == OP_JT && jt1->dst_op.mode == MODE_REG &&
                    str_case_eq(jt1->dst_op.reg, scratch_reg) &&

                    mov2 && mov2->type == OP_MOV && mov2->dst_op.mode == MODE_REG &&
                    str_case_eq(mov2->dst_op.reg, scratch_reg) &&
                    mov2->src_op.mode == MODE_REG && str_case_eq(mov2->src_op.reg, reg_name) &&

                    ieq2 && ieq2->type == OP_IEQ && ieq2->dst_op.mode == MODE_REG &&
                    str_case_eq(ieq2->dst_op.reg, scratch_reg) &&
                    is_boxed_type_operand(&ieq2->src_op) &&
                    str_case_eq(ieq2->src_op.raw, "BOXED_FALSE"))
                {
                    // Both JT targets must match, confirming this really is
                    // emit_falsy_jump()'s single-target shape.
                    AsmNode *jt2 = skip_other_nodes(ieq2->next);
                    if (jt2 && jt2->type == OP_JT && jt2->dst_op.mode == MODE_REG &&
                        str_case_eq(jt2->dst_op.reg, scratch_reg) &&
                        operands_equal(&jt1->src_op, &jt2->src_op))
                    {
                        AsmNode *nodes[] = {mov1, ieq1, jt1};
                        if (remove_with_debug(&curr, nodes, 3, OPT_PEEPHOLE_ALGEBRA)) optimizations += 3;
                        continue;
                    }
                }
            }
        }

        curr = next;
    }

    return optimizations;
}
