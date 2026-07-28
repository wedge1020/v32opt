#include "v32opt.h"

// ================================================================================
// OPTIMIZATION: Trivial Leaf Function Inlining (inline)
// --------------------------------------------------------------------------------
// REPLACES: CALL <func> → <func_body> (for small, non-recursive functions)
// GOAL:     Eliminate CALL/RET overhead, reduce code size, expose new peephole ops.
//
// VIRCON32 NOTES:
//   - All instructions are 1 cycle → Inlining is purely for SIZE reduction.
//   - Focus on register-only ops; avoid inlining functions with stack/indirection.
//   - BP-based offsets ([BP+N]) are rewritten to SP-based ([SP+N-2]) at call sites.
//
// EXAMPLES:
//   ; BEFORE               ; AFTER (inlined)
//   CALL __add_one         IADD R1, 1
//   ...                    ...
//   __add_one:             __add_one:
//     IADD R1, 1             IADD R1, 1
//     RET                    RET
//
// AGGRESSIVENESS LEVELS (via -finline-max=N):
//   - N=8:  Default (inline functions ≤8 instructions)
//   - N=16: Aggressive (inline functions ≤16 instructions)
//   - N=0:  Disable inlining entirely
// ================================================================================

// --- Debug Infrastructure ---
// Uses `insert_debug_comment` to log:
//   - Inlined calls (e.g., "; [DEBUG inline] Replaced CALL __add_one with body")
//   - Skipped candidates (e.g., "; [DEBUG inline] Skipped __complex: too large (12 insns)")
// Enable with `-fdebug` flag (sets `config.debug = true` in main.c).

int inline_trivial_functions(AsmNode *head) {
    // --- PHASE 1: COLLECT INLINING CANDIDATES ---
    // Scan for functions that qualify as "trivial":
    //   - ≤ config.opt_inline_max (default: 8) instructions
    //   - No CALL/JMP/JT/JF (leaf functions only)
    //   - No SP/BP modification (stack-neutral)
    //   - No [BP-N] (local variables)
    //   - Ends with RET
    InlineCandidate candidates[MAX_INLINE_CANDIDATES];
    int candidate_count = 0;

    AsmNode *curr = head ? head->next : NULL;
    while (curr) {
        if (curr->type == OP_LABEL && candidate_count < MAX_INLINE_CANDIDATES) {
            // Extract function name (e.g., "__add_one:" → "__add_one")
            char func_name[128] = {0};
            safe_str_copy(func_name, trim(curr->raw), sizeof(func_name));
            char *colon = strchr(func_name, ':');
            if (colon) *colon = '\0';
            trim(func_name);

            // --- Skip Prologue (PUSH BP; MOV BP, SP) ---
            AsmNode *scan = curr->next;
            while (scan && (scan->type == OP_OTHER || scan->type == OP_LABEL))
                scan = scan->next;

            // Standard Vircon32 prologue
            if (scan && scan->type == OP_PUSH && str_case_eq(scan->dst_op.reg, "BP")) {
                scan = scan->next;
                if (scan && scan->type == OP_MOV && str_case_eq(scan->dst_op.reg, "BP") &&
                    str_case_eq(scan->src_op.reg, "SP")) {
                    scan = scan->next;
                }
            }

            // --- Collect Function Body ---
            AsmNode *core_nodes[config.opt_inline_max];
            int core_count = 0;
            bool valid_candidate = true;

            while (scan) {
                if (scan->type == OP_LABEL) {  // Skip nested labels
                    scan = scan->next;
                    continue;
                }

                // --- Epilogue Detection ---
                if ((scan->type == OP_MOV && str_case_eq(scan->dst_op.reg, "SP") &&
                     str_case_eq(scan->src_op.reg, "BP")) ||
                    str_case_eq(scan->mnemonic, "RET")) {
                    break;
                }

				// --- Disqualification Checks ---
				// Reject if function contains calls or jumps (not a leaf)
				if (str_case_eq(scan->mnemonic, "CALL") ||
					str_case_eq(scan->mnemonic, "JMP") ||
					str_case_eq(scan->mnemonic, "JT") ||
					str_case_eq(scan->mnemonic, "JF")) {
					valid_candidate = false;
					if (config.debug) {
						AsmNode *debug_node = create_node(NULL, OP_OTHER, NULL, NULL, NULL);
						snprintf(debug_node->raw, sizeof(debug_node->raw),
								 "; [DEBUG inline] Skipped %s: contains control flow", func_name);
						debug_node->prev = curr;
						debug_node->next = curr->next;
						if (curr->next) curr->next->prev = debug_node;
						curr->next = debug_node;
					}
					break;
				}

                // Reject stack manipulation (SP/BP ops)
                if (str_case_eq(scan->mnemonic, "PUSH") || str_case_eq(scan->mnemonic, "POP") ||
                    str_case_eq(scan->dst_op.reg, "SP") || str_case_eq(scan->src_op.reg, "SP")) {
                    if (config.debug) {
                        AsmNode *debug_node = create_node(
                            NULL, OP_OTHER, NULL, NULL, NULL);
                        snprintf(debug_node->raw, sizeof(debug_node->raw),
                                 "; [DEBUG inline] Skipped %s: modifies SP/BP", func_name);
                        debug_node->prev = curr;
                        debug_node->next = curr->next;
                        if (curr->next) curr->next->prev = debug_node;
                        curr->next = debug_node;
                    }
                    valid_candidate = false;
                    break;
                }

                // Reject local variables ([BP-N])
                if ((scan->dst_op.mode == MODE_INDIRECT && str_case_eq(scan->dst_op.reg, "BP") &&
                     scan->dst_op.offset < 0) ||
                    (scan->src_op.mode == MODE_INDIRECT && str_case_eq(scan->src_op.reg, "BP") &&
                     scan->src_op.offset < 0)) {
                    if (config.debug) {
                        AsmNode *debug_node = create_node(
                            NULL, OP_OTHER, NULL, NULL, NULL);
                        snprintf(debug_node->raw, sizeof(debug_node->raw),
                                 "; [DEBUG inline] Skipped %s: uses local vars [BP-N]", func_name);
                        debug_node->prev = curr;
                        debug_node->next = curr->next;
                        if (curr->next) curr->next->prev = debug_node;
                        curr->next = debug_node;
                    }
                    valid_candidate = false;
                    break;
                }

                // Collect instruction if under limit
                if (core_count < config.opt_inline_max) {
                    core_nodes[core_count++] = scan;
                } else {
                    if (config.debug) {
                        AsmNode *debug_node = create_node(
                            NULL, OP_OTHER, NULL, NULL, NULL);
                        snprintf(debug_node->raw, sizeof(debug_node->raw),
                                 "; [DEBUG inline] Skipped %s: too large (%d insns)", func_name, core_count);
                        debug_node->prev = curr;
                        debug_node->next = curr->next;
                        if (curr->next) curr->next->prev = debug_node;
                        curr->next = debug_node;
                    }
                    valid_candidate = false;
                    break;
                }
                scan = scan->next;
            }

            // --- Register Valid Candidate ---
            if (valid_candidate && core_count > 0 && core_count <= config.opt_inline_max) {
                safe_str_copy(candidates[candidate_count].name, func_name, sizeof(candidates[candidate_count].name));
                candidates[candidate_count].body_count = core_count;
                for (int i = 0; i < core_count; i++) {
                    candidates[candidate_count].body_nodes[i] = core_nodes[i];
                }
                candidate_count++;
                if (config.debug) {
                    AsmNode *debug_node = create_node(
                        NULL, OP_OTHER, NULL, NULL, NULL);
                    snprintf(debug_node->raw, sizeof(debug_node->raw),
                             "; [DEBUG inline] Candidate: %s (%d insns)", func_name, core_count);
                    debug_node->prev = curr;
                    debug_node->next = curr->next;
                    if (curr->next) curr->next->prev = debug_node;
                    curr->next = debug_node;
                }
            }
        }
        curr = curr->next;
    }

	// --- PHASE 2: PERFORM INLINING AT CALL SITES ---
	int inlined_calls = 0;
	curr = head ? head->next : NULL;
	bool seen_first_label = false;  // Initialize to false

	while (curr) {
		AsmNode *next_node = curr->next;

		// Set seen_first_label ONLY after encountering the first OP_LABEL
		if (curr->type == OP_LABEL) {
			seen_first_label = true;
		}

		// --- Process CALL Instructions (ONLY after first label) ---
		if (str_case_eq(curr->mnemonic, "CALL") && seen_first_label) {
            char target_label[128] = {0};
            safe_str_copy(target_label, trim(curr->dst_op.raw), sizeof(target_label));

            // --- Diagnostic Flags ---
            // Skip if inlining limit reached (for debugging)
            if (config.opt_inline_max >= 0 && g_inline_calls_so_far >= config.opt_inline_max) {
                if (config.debug) {
                    AsmNode *debug_node = create_node(
                        NULL, OP_OTHER, NULL, NULL, NULL);
                    snprintf(debug_node->raw, sizeof(debug_node->raw),
                             "; [DEBUG inline] Skipped CALL %s: hit -finline-max=%d",
                             target_label, config.opt_inline_max);
                    debug_node->prev = curr->prev;
                    debug_node->next = curr;
                    if (curr->prev) curr->prev->next = debug_node;
                    curr->prev = debug_node;
                }
                curr = next_node;
                continue;
            }

            // Skip if target is in exclude list (for debugging)
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
                    if (config.debug) {
                        AsmNode *debug_node = create_node(
                            NULL, OP_OTHER, NULL, NULL, NULL);
                        snprintf(debug_node->raw, sizeof(debug_node->raw),
                                 "; [DEBUG inline] Skipped CALL %s: excluded by -finline-exclude",
                                 target_label);
                        debug_node->prev = curr->prev;
                        debug_node->next = curr;
                        if (curr->prev) curr->prev->next = debug_node;
                        curr->prev = debug_node;
                    }
                    curr = next_node;
                    continue;
                }
            }

            // --- Find Matching Candidate ---
            for (int c = 0; c < candidate_count; c++) {
                if (str_case_eq(target_label, candidates[c].name)) {
                    if (config.debug) {
                        AsmNode *debug_node = create_node(
                            NULL, OP_OTHER, NULL, NULL, NULL);
                        snprintf(debug_node->raw, sizeof(debug_node->raw),
                                 "; [DEBUG inline] Inlined CALL %s (%d insns)",
                                 target_label, candidates[c].body_count);
                        debug_node->prev = curr->prev;
                        debug_node->next = curr;
                        if (curr->prev) curr->prev->next = debug_node;
                        curr->prev = debug_node;
                    }

                    // Insert inlined body BEFORE the CALL
                    AsmNode *insertion_point = curr->prev;

                    // --- Clone and Rewrite Each Instruction ---
                    for (int b = 0; b < candidates[c].body_count; b++) {
                        AsmNode *inlined_ins = clone_node(candidates[c].body_nodes[b]);

                        // Rewrite [BP+N] → [SP+(N-2)] to account for missing prologue
                        // In callee: [BP+N] = Nth argument (N=2: first arg)
                        // At call site: arguments are at [SP+0], [SP+1], etc.
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

    if (config.debug && inlined_calls > 0) {
        printf("[DEBUG inline] Inlined %d call(s) total\n", inlined_calls);
    }
    return inlined_calls;
}
