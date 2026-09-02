#include "v32opt.h"

int peephole_dead_stores(AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        // Target: MOV instruction that writes to a destination (register or memory)
        if (curr->type == OP_MOV && curr->has_dst)
        {
            bool is_dead = false;
            bool has_call_in_block = false;
            AsmNode *scan = curr->next;

            // --- PRE-SCAN: Check if there's a CALL in this basic block ---
            // If so, be conservative - don't eliminate stores that might feed function arguments
            AsmNode *pre_scan = scan;
            int pre_scan_distance = 0;
            while (pre_scan) {
                if (is_dead_store_scan_boundary(pre_scan)) break;
                if (++pre_scan_distance > PEEPHOLE_MAX_SCAN_DISTANCE) {
                    has_call_in_block = true;  // conservative: didn't prove otherwise
                    break;
                }
                if (pre_scan->type == OP_OTHER) {
                    pre_scan = pre_scan->next;
                    continue;
                }
                if (pre_scan->type == OP_CALL) {
                    has_call_in_block = true;
                    break;
                }
                pre_scan = pre_scan->next;
            }

            int scan_distance = 0;
            while (scan)
            {
                if (is_dead_store_scan_boundary(scan)) {
                    break;
                }

                // Same cap as the pre-scan above, same reasoning: giving up
                // here just means "not proven dead", the safe default.
                if (++scan_distance > PEEPHOLE_MAX_SCAN_DISTANCE) {
                    break;
                }

                // Skip pure comments and blank lines natively
                if (scan->type == OP_OTHER) {
                    scan = scan->next;
                    continue;
                }

                // =================================================================
                // CASE 1: Tracking a Dead Register Store (e.g., MOV R1, 5)
                // =================================================================
                if (curr->dst_op.mode == MODE_REG)
                {
                    char *dst_reg = curr->dst_op.reg;

                    // --- GUARD: Never eliminate SP or BP stores (stack frame critical) ---
                    if (str_case_eq(dst_reg, "SP") || str_case_eq(dst_reg, "BP")) {
                        break;
                    }

                    // --- GUARD: If there's a CALL in this block, don't eliminate stores
                    // that might feed function arguments (R0-R3 typically used for args) ---
                    if (has_call_in_block) {
                        // Conservative: don't eliminate any register store before a CALL
                        break;
                    }

                    // If the register is READ before being overwritten, it's NOT a dead store.
                    if (is_register_read(scan, dst_reg)) {
                        break;
                    }

                    // If the register is OVERWRITTEN without being read, the original store is DEAD.
                    // modifies_register safely catches direct writes and CALL clobbers.
                    if (modifies_register(scan, dst_reg)) {
                        is_dead = true;
                        break;
                    }
                }

                // =================================================================
                // CASE 2: Tracking a Dead Memory Store (e.g., MOV [BP-4], R1)
                // =================================================================
                else if (curr->dst_op.mode == MODE_INDIRECT)
                {
                    char *mem_reg = curr->dst_op.reg;
                    int mem_off   = curr->dst_op.offset;

                    // Did we find an EXACT overwrite of the tracked memory address?
                    if (scan->has_dst && scan->dst_op.mode == MODE_INDIRECT &&
                        str_case_eq(scan->dst_op.reg, mem_reg) && scan->dst_op.offset == mem_off)
                    {
                        is_dead = true;
                        break;
                    }

                    // Otherwise, verify this intervening instruction doesn't clobber state:
                    bool clobbers = false;

                    // A. Modifies our base address register (we lose track of where memory points)
                    if (modifies_register(scan, mem_reg)) clobbers = true;

                    // B. Reads from memory (Anti-Aliasing Check)
                    if (scan->has_src && scan->src_op.mode == MODE_INDIRECT) {
                        // The ONLY safe read is one sharing the same base register but a DIFFERENT offset.
                        // Any other read (exact match, or alien base register) might alias.
                        if (!(str_case_eq(scan->src_op.reg, mem_reg) && scan->src_op.offset != mem_off)) {
                            clobbers = true;
                        }
                    }

                    // C. Writes to memory (Anti-Aliasing Check)
                    if (scan->has_dst && scan->dst_op.mode == MODE_INDIRECT) {
                        // We already caught the exact overwrite above.
                        // Any write using a DIFFERENT base register might alias our location.
                        if (!str_case_eq(scan->dst_op.reg, mem_reg)) {
                            clobbers = true;
                        }
                    }

                    // D. Broad Memory Modifiers
                    if (scan->type == OP_PUSH || scan->type == OP_POP || scan->type == OP_CALL) {
                        clobbers = true;
                    }

                    if (clobbers) {
                        break;
                    }
                }

                scan = scan->next;
            }

            // Remove the dead store and advance curr properly
            if (is_dead)
            {
                insert_debug_comment(curr->prev, OPT_PEEPHOLE_DEAD_STORES, curr->raw);
                AsmNode *to_remove = curr;

                // Advance curr BEFORE destroying its memory
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
