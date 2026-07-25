#include "v32opt.h"

// -------------------------------------------------------------------
// OPTIMIZATION CATEGORY: Trivial Leaf Function Inlining
// Replaces CALL sites with the body of small, non-recursive functions
// that have no side effects (no stack manipulation, no calls).
// -------------------------------------------------------------------

// ===================================================================
// TRIVIAL LEAF FUNCTION INLINING
//
// Identifies and inlines "trivial" functions that:
//   - Have <= MAX_BODY_INS (8) instructions
//   - Do not call other functions
//   - Do not modify SP (stack pointer) directly
//   - Do not use [BP-N] (negative offsets, i.e., local variables)
//   - End with RET
//
// Inlining process:
//   1. Scan for function labels and collect candidates
//   2. At each CALL site, replace with the candidate's body
//   3. Rewrite [BP+N] operands to [SP+(N-2)] to account for the missing prologue
//   4. Skip inlining before the first label (to avoid %define ordering issues)
//
// Note: On Vircon32, ALL instructions are 1 cycle, so inlining is purely for
// code size reduction and does not affect performance. No cycle-count
// assumptions are made.
//
// Destructive comparisons (IEQ/INE/etc.) are NOT an issue here because:
//   - This pass only clones/inlines existing instructions
//   - It does not modify or eliminate comparison operations
// ===================================================================
int inline_trivial_functions(AsmNode *head) {
    // --- PHASE 1: COLLECT INLINING CANDIDATES ---
    // Scan the assembly for functions that qualify as "trivial" leaf functions.
    InlineCandidate candidates[MAX_INLINE_CANDIDATES];
    int candidate_count = 0;

    AsmNode *curr = head ? head->next : NULL;

    while (curr) {
        // --- Candidate Detection: Look for Function Labels ---
        if (curr->type == OP_LABEL && candidate_count < MAX_INLINE_CANDIDATES) {
            // Extract function name from label (e.g., "my_func:" -> "my_func")
            char func_name[128] = {0};
            char line_copy[8192];
            safe_str_copy(line_copy, curr->raw, sizeof(line_copy));
            char *trimmed_lbl = trim(line_copy);

            safe_str_copy(func_name, trimmed_lbl, sizeof(func_name));
            char *colon = strchr(func_name, ':');
            if (colon) *colon = '\0';
            trim(func_name);

            // --- Skip Prologue (BP setup) ---
            // Standard Vircon32 prologue: PUSH BP; MOV BP, SP
            AsmNode *scan = curr->next;
            while (scan && (scan->type == OP_OTHER || scan->type == OP_LABEL)) 
                scan = scan->next;

            // Skip PUSH BP
            if (scan && scan->type == OP_PUSH && str_case_eq(scan->dst_op.reg, "BP")) {
                scan = scan->next;
                // Skip MOV BP, SP
                if (scan && scan->type == OP_MOV && str_case_eq(scan->dst_op.reg, "BP") && str_case_eq(scan->src_op.reg, "SP")) {
                    scan = scan->next;
                }
            }

            // --- Collect Function Body ---
            AsmNode *core_nodes[MAX_BODY_INS];
            int core_count = 0;
            bool valid_candidate = true;

            while (scan) {
                // Skip nested labels
                if (scan->type == OP_LABEL) {
                    scan = scan->next;
                    continue;
                }

                // --- Epilogue Detection: MOV SP, BP; POP BP; RET ---
                // Stop at function end
                if (scan->type == OP_MOV && str_case_eq(scan->dst_op.reg, "SP") && str_case_eq(scan->src_op.reg, "BP")) 
                    break;
                if (str_case_eq(scan->mnemonic, "RET")) 
                    break;

                // --- Disqualification Checks ---
                // Reject if function contains calls or jumps (not a leaf)
                if (str_case_eq(scan->mnemonic, "CALL") || str_case_eq(scan->mnemonic, "JMP") ||
                    str_case_eq(scan->mnemonic, "JT")   || str_case_eq(scan->mnemonic, "JF")) {
                    valid_candidate = false;
                    break;
                }

                // FIX: Reject any candidate that touches SP directly.
                // PUSH/POP or any instruction using SP as an operand changes
                // the stack layout, which the BP->SP offset rewrite below
                // cannot account for.
                if (str_case_eq(scan->mnemonic, "PUSH") || str_case_eq(scan->mnemonic, "POP")) {
                    valid_candidate = false;
                    break;
                }
                if (str_case_eq(scan->dst_op.reg, "SP") || str_case_eq(scan->src_op.reg, "SP")) {
                    valid_candidate = false;
                    break;
                }

                // FIX: Reject any [BP-N] (negative offset) reference.
                // This indicates a local variable slot that the callee's
                // prologue allocated. The inliner cannot handle these because
                // it doesn't emit a new prologue for the inlined body.
                if ((scan->dst_op.mode == MODE_INDIRECT && str_case_eq(scan->dst_op.reg, "BP") && scan->dst_op.offset < 0) ||
                    (scan->src_op.mode == MODE_INDIRECT && str_case_eq(scan->src_op.reg, "BP") && scan->src_op.offset < 0)) {
                    valid_candidate = false;
                    break;
                }

                // --- Collect Instruction (if under limit) ---
                if (core_count < MAX_BODY_INS) {
                    core_nodes[core_count++] = scan;
                } else {
                    // Body exceeds maximum size
                    valid_candidate = false;
                    break;
                }

                scan = scan->next;
            }

            // --- Register Valid Candidate ---
            if (valid_candidate && core_count > 0 && core_count <= MAX_BODY_INS) {
                safe_str_copy(candidates[candidate_count].name, func_name, sizeof(candidates[candidate_count].name));
                candidates[candidate_count].body_count = core_count;
                for (int i = 0; i < core_count; i++) {
                    candidates[candidate_count].body_nodes[i] = core_nodes[i];
                }
                candidate_count++;
            }
        }
        curr = curr->next;
    }

    // --- PHASE 2: PERFORM INLINING AT CALL SITES ---
    int inlined_calls = 0;
    curr = head ? head->next : NULL;

    // FIX: Do not inline before the first label.
    // Vircon32 assembly files start with a "program start section"
    // (CALLs + HLT) before the "%define global_X N" block that defines
    // global variable addresses. If we inline a function called from
    // this section, its body (which may reference "[global_X]") would be
    // placed before the %define lines, causing assembler errors.
    // Solution: Track if we've seen the first label, and skip inlining
    // until we have.
    bool seen_first_label = false;

    while (curr) {
        AsmNode *next_node = curr->next;

        // Track if we've passed the initial non-label section
        if (curr->type == OP_LABEL) {
            seen_first_label = true;
        }

        // --- Process CALL Instructions (After First Label) ---
        if (str_case_eq(curr->mnemonic, "CALL") && seen_first_label) {
            char target_label[128] = {0};
            safe_str_copy(target_label, trim(curr->dst_op.raw), sizeof(target_label));

            // DIAGNOSTIC: Skip if inlining limit reached
            // Allows bisecting inlined call sites for debugging
            if (g_inline_call_limit >= 0 && g_inline_calls_so_far >= g_inline_call_limit) {
                curr = next_node;
                continue;
            }

            // DIAGNOSTIC: Skip if target is in exclude list
            // Allows testing one specific function in isolation
            if (g_inline_exclude_name[0] != '\0') {
                char list_copy[1024];
                safe_str_copy(list_copy, g_inline_exclude_name, sizeof(list_copy));
                bool excluded = false;
                char *tok = strtok(list_copy, ",");
                while (tok) {
                    if (str_case_eq(trim(tok), target_label)) { 
                        excluded = true; 
                        break; 
                    }
                    tok = strtok(NULL, ",");
                }
                if (excluded) {
                    curr = next_node;
                    continue;
                }
            }

            // --- Find Matching Candidate ---
            for (int c = 0; c < candidate_count; c++) {
                if (str_case_eq(target_label, candidates[c].name)) {
                    // Insert inlined body BEFORE the CALL
                    AsmNode *insertion_point = curr->prev;

                    // --- Clone and Rewrite Each Instruction ---
                    for (int b = 0; b < candidates[c].body_count; b++) {
                        AsmNode *inlined_ins = clone_node(candidates[c].body_nodes[b]);

                        // FIX: Rewrite [BP+N] to [SP+(N-2)]
                        // In the callee, [BP+N] refers to the Nth argument
                        // (N=2 is first argument, N=3 is second, etc.)
                        // At the call site, these same arguments are at
                        // [SP+0], [SP+1], etc. (pushed before CALL)
                        // So we subtract 2 from the offset and change BP to SP.
                        if (inlined_ins->dst_op.mode == MODE_INDIRECT &&
                            str_case_eq(inlined_ins->dst_op.reg, "BP") &&
                            inlined_ins->dst_op.offset >= 2) {
                            inlined_ins->dst_op.offset -= 2;
                            safe_str_copy(inlined_ins->dst_op.reg, "SP", sizeof(inlined_ins->dst_op.reg));
                            snprintf(inlined_ins->dst_op.raw, sizeof(inlined_ins->dst_op.raw),
                                     "[SP+%d]", inlined_ins->dst_op.offset);
                        }
                        if (inlined_ins->src_op.mode == MODE_INDIRECT &&
                            str_case_eq(inlined_ins->src_op.reg, "BP") &&
                            inlined_ins->src_op.offset >= 2) {
                            inlined_ins->src_op.offset -= 2;
                            safe_str_copy(inlined_ins->src_op.reg, "SP", sizeof(inlined_ins->src_op.reg));
                            snprintf(inlined_ins->src_op.raw, sizeof(inlined_ins->src_op.raw),
                                     "[SP+%d]", inlined_ins->src_op.offset);
                        }

                        // Regenerate raw text from rewritten operands
                        // (write_vircon32_asm emits raw directly, not parsed structs)
                        if (inlined_ins->has_dst && inlined_ins->has_src) {
                            snprintf(inlined_ins->raw, sizeof(inlined_ins->raw), "    %s %s, %s",
                                     inlined_ins->mnemonic, inlined_ins->dst_op.raw, inlined_ins->src_op.raw);
                        } else if (inlined_ins->has_dst) {
                            snprintf(inlined_ins->raw, sizeof(inlined_ins->raw), "    %s %s",
                                     inlined_ins->mnemonic, inlined_ins->dst_op.raw);
                        }

                        // Insert into list
                        inlined_ins->prev = insertion_point;
                        inlined_ins->next = curr;
                        if (insertion_point) insertion_point->next = inlined_ins;
                        curr->prev = inlined_ins;
                        insertion_point = inlined_ins;
                    }

                    // Remove the original CALL instruction
                    remove_node(curr);
                    inlined_calls++;
                    g_inline_calls_so_far++;
                    break;
                }
            }
        }

        curr = next_node;
    }

    return inlined_calls;
}
