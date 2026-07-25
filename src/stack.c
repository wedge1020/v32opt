#include "v32opt.h"

// ===================================================================
// HELPER: Check if an instruction references the BP register
// ===================================================================
bool references_bp(AsmNode *node) {
    if (!node) return false;
    
    // Check destination operand for BP or [BP +/- offset]
    if (node->has_dst) {
        if (str_case_eq(node->dst_op.reg, "BP")) return true;
        if (node->dst_op.raw[0] != '\0' && strstr(node->dst_op.raw, "BP")) return true;
    }
    
    // Check source operand for BP or [BP +/- offset]
    if (node->has_src) {
        if (str_case_eq(node->src_op.reg, "BP")) return true;
        if (node->src_op.raw[0] != '\0' && strstr(node->src_op.raw, "BP")) return true;
    }
    
    return false;
}

// ===================================================================
// PASS: Frame Pointer Elimination (Stack Frame Elision)
// Scans entire functions. If BP is never referenced in the body,
// strips the standard prologue (push BP / mov BP, SP) and 
// epilogue (mov SP, BP / pop BP).
// ===================================================================
int omit_frame_pointers (AsmNode *head)
{
    int optimizations = 0;
    AsmNode *curr = head ? head->next : NULL;

    while (curr)
    {
        // 1. Detect standard Function Prologue: PUSH BP followed by MOV BP, SP
        if (curr->type == OP_PUSH && curr->has_src && str_case_eq(curr->src_op.reg, "BP"))
        {
            AsmNode *push_bp = curr;
            AsmNode *mov_bp_sp = push_bp->next;
            
            // Skip comments/blank lines between PUSH BP and MOV BP, SP
            while (mov_bp_sp && mov_bp_sp->type == OP_OTHER) mov_bp_sp = mov_bp_sp->next;

            if (mov_bp_sp && mov_bp_sp->type == OP_MOV && 
                mov_bp_sp->has_dst && str_case_eq(mov_bp_sp->dst_op.reg, "BP") &&
                mov_bp_sp->has_src && str_case_eq(mov_bp_sp->src_op.reg, "SP"))
            {
                // We found a valid prologue! Now scan the function body until RET.
                bool bp_used_in_body = false;
                AsmNode *epilogue_mov = NULL;
                AsmNode *epilogue_pop = NULL;
                
                AsmNode *scan = mov_bp_sp->next;
                while (scan)
                {
                    if (scan->type == OP_OTHER && (scan->raw[0] == '\0' || scan->raw[0] == ';')) {
                        scan = scan->next;
                        continue;
                    }

                    // If we hit another function label before RET, abort this scan
                    if (scan->type == OP_LABEL && strncmp(scan->raw, "__function_", 11) == 0 && 
                        strstr(scan->raw, "_return:") == NULL) {
                        break;
                    }

                    // 2. Check if we hit the Epilogue: MOV SP, BP followed by POP BP
                    if (scan->type == OP_MOV && 
                        scan->has_dst && str_case_eq(scan->dst_op.reg, "SP") &&
                        scan->has_src && str_case_eq(scan->src_op.reg, "BP"))
                    {
                        AsmNode *next_node = scan->next;
                        while (next_node && next_node->type == OP_OTHER) next_node = next_node->next;

                        if (next_node && next_node->type == OP_POP && 
                            next_node->has_dst && str_case_eq(next_node->dst_op.reg, "BP"))
                        {
                            epilogue_mov = scan;
                            epilogue_pop = next_node;
                            scan = next_node->next;
                            continue; // Do not flag the epilogue itself as "using BP"!
                        }
                    }

                    // Stop scanning when we reach the return instruction
                    if (str_case_eq(scan->mnemonic, "RET")) {
                        break;
                    }

                    // 3. If any real body instruction references BP, we cannot eliminate!
                    if (references_bp(scan)) {
                        bp_used_in_body = true;
                        break;
                    }

                    scan = scan->next;
                }

                // 4. The Verdict: If BP was never touched in the body, eliminate the frame!
                if (!bp_used_in_body && epilogue_mov && epilogue_pop)
                {
                    push_bp->type = OP_OTHER;
                    snprintf(push_bp->raw, sizeof(push_bp->raw), "; optimized out frame: push BP");
                    
                    mov_bp_sp->type = OP_OTHER;
                    snprintf(mov_bp_sp->raw, sizeof(mov_bp_sp->raw), "; optimized out frame: mov BP, SP");
                    
                    epilogue_mov->type = OP_OTHER;
                    snprintf(epilogue_mov->raw, sizeof(epilogue_mov->raw), "; optimized out frame: mov SP, BP");
                    
                    epilogue_pop->type = OP_OTHER;
                    snprintf(epilogue_pop->raw, sizeof(epilogue_pop->raw), "; optimized out frame: pop BP");

                    optimizations += 4;
                    curr = epilogue_pop; // Fast-forward our outer loop
                }
            }
        }
        curr = curr->next;
    }

    return optimizations;
}
