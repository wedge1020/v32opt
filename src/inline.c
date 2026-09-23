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
//   - BP-based offsets ([BP+N], N >= 2) are rewritten to SP-based ([SP+N-2]) at
//     call sites. This mapping is a frame-layout INVARIANT (the callee's BP is
//     always SP_at_call - 2: one word for the return address CALL pushes, one
//     for the saved BP the prologue pushes), so it holds regardless of how the
//     caller arranged its arguments -- v32lua PUSHes them, the v32 C compiler
//     STOREs them at [SP+k]; both land at callee [BP+2], [BP+3], ... It is
//     only sound because candidate bodies are forbidden from touching SP or
//     BP themselves, so the spliced body runs with SP exactly where the CALL
//     would have left it.
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
//   (N is clamped to 0..MAX_BODY_INS, the candidate body array capacity.)
// ================================================================================

// ---------------------------------------------------------------
// Is this label a genuine function/data boundary, as opposed to an
// internal control-flow label of whatever function contains it?
// Mirrors dce.c's boundary predicate: "__function_<name>:" (but not
// a function's internal "<name>_return:" label), "__literal_*:"
// data, and any "__global_*:" label. Used to keep this pass's
// function-body scans from running past the end of one function
// into the next one (or into a data section).
// ---------------------------------------------------------------
static bool is_inline_boundary_label(const char *lbl) {
    size_t lbl_len = strlen(lbl);
    bool is_return_label = (lbl_len >= 8 && str_case_eq(lbl + lbl_len - 8, "_return:"));
    return (strncmp(lbl, "__function_", 11) == 0 && !is_return_label) ||
           strncmp(lbl, "__literal_", 10) == 0 ||
           strncmp(lbl, "__global_", 9) == 0;
}

int inline_trivial_functions(AsmNode *head) {
    // --- PHASE 0: COLLECT NON-LEAF FUNCTIONS ---
    // Track functions that contain CALL/JMP/JT/JF (non-leaf)
    char non_leaf_functions[MAX_FUNCTIONS][128] = {{0}};
    int non_leaf_count = 0;

    AsmNode *curr = head ? head->next : NULL;
    while (curr) {
        if (curr->type == OP_LABEL) {
            char func_name[128] = {0};
            safe_str_copy(func_name, trim(curr->raw), sizeof(func_name));
            char *colon = strchr(func_name, ':');
            if (colon) *colon = '\0';
            trim(func_name);

            // Check if this function is non-leaf (contains CALL/JMP/JT/JF)
            AsmNode *scan = curr->next;
            bool is_non_leaf = false;
            while (scan) {
                if (scan->type == OP_LABEL) {
                    // BUG FIX: a genuine function/data boundary label ends
                    // THIS function, full stop. The scan used to skip ALL
                    // labels and keep going, so a function that fell off
                    // its own end (no RET/epilogue) silently swallowed the
                    // NEXT function's body into its own "non-leaf" scan --
                    // usually just a wasted optimization, but wrong.
                    char line_copy[8192];
                    safe_str_copy(line_copy, scan->raw, sizeof(line_copy));
                    char *lbl = trim(line_copy);
                    if (is_inline_boundary_label(lbl)) break;
                    scan = scan->next;
                    continue;
                }
                if (str_case_eq(scan->mnemonic, "CALL") ||
                    str_case_eq(scan->mnemonic, "JMP") ||
                    str_case_eq(scan->mnemonic, "JT") ||
                    str_case_eq(scan->mnemonic, "JF")) {
                    is_non_leaf = true;
                    break;
                }
                if ((scan->type == OP_MOV && str_case_eq(scan->dst_op.reg, "SP") &&
                     str_case_eq(scan->src_op.reg, "BP")) ||
                    str_case_eq(scan->mnemonic, "RET")) {
                    break;
                }
                // BUG FIX: HLT also ends a function's execution (a
                // noreturn helper, e.g. a fatal-error exit). Without this
                // the scan ran past it into whatever followed.
                if (str_case_eq(scan->mnemonic, "HLT")) {
                    break;
                }
                scan = scan->next;
            }

            if (is_non_leaf && non_leaf_count < MAX_FUNCTIONS) {
                safe_str_copy(non_leaf_functions[non_leaf_count++], func_name, 128);
            }
        }
        curr = curr->next;
    }

    // --- PHASE 1: COLLECT INLINING CANDIDATES ---
    InlineCandidate candidates[MAX_INLINE_CANDIDATES];
    int candidate_count = 0;

    curr = head ? head->next : NULL;
    while (curr) {
        if (curr->type == OP_LABEL && candidate_count < MAX_INLINE_CANDIDATES) {
            char func_name[128] = {0};
            safe_str_copy(func_name, trim(curr->raw), sizeof(func_name));
            char *colon = strchr(func_name, ':');
            if (colon) *colon = '\0';
            trim(func_name);

            // Skip if this function is non-leaf (already marked)
            bool is_non_leaf = false;
            for (int i = 0; i < non_leaf_count; i++) {
                if (str_case_eq(func_name, non_leaf_functions[i])) {
                    is_non_leaf = true;
                    break;
                }
            }
            if (is_non_leaf) {
                curr = curr->next;
                continue;
            }

            // --- Skip Prologue (PUSH BP; MOV BP, SP) ---
            AsmNode *scan = curr->next;
            while (scan && (scan->type == OP_OTHER || scan->type == OP_LABEL))
                scan = scan->next;

            if (scan && scan->type == OP_PUSH && str_case_eq(scan->dst_op.reg, "BP")) {
                scan = scan->next;
                if (scan && scan->type == OP_MOV && str_case_eq(scan->dst_op.reg, "BP") &&
                    str_case_eq(scan->src_op.reg, "SP")) {
                    scan = scan->next;
                }
            }

            // --- Collect Function Body ---
            AsmNode *core_nodes[MAX_BODY_INS]; // sized for the clamped -finline-max
            int core_count = 0;
            bool valid_candidate = true;

            while (scan) {
                if (scan->type == OP_LABEL) {
                    // BUG FIX: a function/data boundary label ends this
                    // function's body. Skipping ALL labels (the old
                    // behavior) let a function with no RET/epilogue (e.g.
                    // one ending in HLT, or falling off its end) swallow
                    // the NEXT function's instructions -- and any data
                    // directives -- into its collected body, which then
                    // got spliced into every call site. Internal labels
                    // (loop leftovers, "<name>_return:") are still skipped:
                    // a branch-free candidate can't be jumped into.
                    char line_copy[8192];
                    safe_str_copy(line_copy, scan->raw, sizeof(line_copy));
                    char *lbl = trim(line_copy);
                    if (is_inline_boundary_label(lbl)) {
                        valid_candidate = false;  // fell off the end: no epilogue
                        break;
                    }
                    scan = scan->next;
                    continue;
                }

                // --- Epilogue Detection ---
                if ((scan->type == OP_MOV && str_case_eq(scan->dst_op.reg, "SP") &&
                     str_case_eq(scan->src_op.reg, "BP")) ||
                    str_case_eq(scan->mnemonic, "RET")) {
                    break;
                }

                // BUG FIX: a body ending in HLT is a noreturn function;
                // it has no epilogue, and collecting past it would swallow
                // whatever follows. Disqualify entirely (inlining a halt
                // is defensible, but such functions are rare and this
                // keeps the splice strictly epilogue-terminated).
                if (str_case_eq(scan->mnemonic, "HLT")) {
                    valid_candidate = false;
                    break;
                }

                // --- Disqualification Checks ---
                // Reject if function contains calls or jumps (redundant now due to Phase 0, but kept for safety)
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
                        AsmNode *debug_node = create_node(NULL, OP_OTHER, NULL, NULL, NULL);
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

                // BUG FIX: reject ANY direct reference to the BP register
                // (read or write). The old check only rejected SP --
                // "MOV BP, R1" (clobbering the caller's frame pointer) and
                // "MOV R1, BP" (capturing it) both slipped through.
                if ((scan->has_dst && scan->dst_op.mode == MODE_REG &&
                     str_case_eq(scan->dst_op.reg, "BP")) ||
                    (scan->has_src && scan->src_op.mode == MODE_REG &&
                     str_case_eq(scan->src_op.reg, "BP"))) {
                    valid_candidate = false;
                    break;
                }

                // Reject local variables ([BP-N])
                if ((scan->dst_op.mode == MODE_INDIRECT && str_case_eq(scan->dst_op.reg, "BP") &&
                     scan->dst_op.offset < 0) ||
                    (scan->src_op.mode == MODE_INDIRECT && str_case_eq(scan->src_op.reg, "BP") &&
                     scan->src_op.offset < 0)) {
                    if (config.debug) {
                        AsmNode *debug_node = create_node(NULL, OP_OTHER, NULL, NULL, NULL);
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

                // BUG FIX: reject [BP+0] and [BP+1] (offset < 2). Those
                // slots are the callee's SAVED BP and RETURN ADDRESS --
                // neither of which exists at the call site after the CALL
                // is spliced away. The [BP+N] -> [SP+N-2] rewrite below
                // only covers N >= 2 (real parameter slots), so a body
                // touching [BP] / [BP+1] would read the CALLER's frame
                // unrewritten.
                if ((scan->dst_op.mode == MODE_INDIRECT && str_case_eq(scan->dst_op.reg, "BP") &&
                     scan->dst_op.offset >= 0 && scan->dst_op.offset < 2) ||
                    (scan->src_op.mode == MODE_INDIRECT && str_case_eq(scan->src_op.reg, "BP") &&
                     scan->src_op.offset >= 0 && scan->src_op.offset < 2)) {
                    valid_candidate = false;
                    break;
                }

                // Collect instruction if under the configurable limit
                if (core_count < config.opt_inline_max_body_ins) {
                    core_nodes[core_count++] = scan;
                } else {
                    if (config.debug) {
                        AsmNode *debug_node = create_node(NULL, OP_OTHER, NULL, NULL, NULL);
                        snprintf(debug_node->raw, sizeof(debug_node->raw),
                                 "; [DEBUG inline] Skipped %s: too large (%d insns, max=%d)",
                                 func_name, core_count, config.opt_inline_max_body_ins);
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

            // Register valid candidate
            if (valid_candidate && core_count > 0 && core_count <= config.opt_inline_max_body_ins) {
                safe_str_copy(candidates[candidate_count].name, func_name, sizeof(candidates[candidate_count].name));
                candidates[candidate_count].body_count = core_count;
                for (int i = 0; i < core_count; i++) {
                    candidates[candidate_count].body_nodes[i] = core_nodes[i];
                }
                candidate_count++;
                if (config.debug) {
                    AsmNode *debug_node = create_node(NULL, OP_OTHER, NULL, NULL, NULL);
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
    bool seen_first_label = false;

    while (curr) {
        AsmNode *next_node = curr->next;

        // Treat OP_LABEL or %define as the first label boundary
        if (curr->type == OP_LABEL ||
            (curr->type == OP_OTHER && strstr(curr->raw, "%define"))) {
            seen_first_label = true;
        }

        // --- Process CALL Instructions (ONLY after first label) ---
        if (str_case_eq(curr->mnemonic, "CALL") && seen_first_label) {
            char target_label[128] = {0};
            safe_str_copy(target_label, trim(curr->dst_op.raw), sizeof(target_label));

            // --- Skip if target is non-leaf ---
            bool target_is_non_leaf = false;
            for (int i = 0; i < non_leaf_count; i++) {
                if (str_case_eq(target_label, non_leaf_functions[i])) {
                    target_is_non_leaf = true;
                    break;
                }
            }
            if (target_is_non_leaf) {
                if (config.debug) {
                    AsmNode *debug_node = create_node(NULL, OP_OTHER, NULL, NULL, NULL);
                    snprintf(debug_node->raw, sizeof(debug_node->raw),
                             "; [DEBUG inline] Skipped CALL %s: target is non-leaf", target_label);
                    debug_node->prev = curr->prev;
                    debug_node->next = curr;
                    if (curr->prev) curr->prev->next = debug_node;
                    curr->prev = debug_node;
                }
                curr = next_node;
                continue;
            }

            // --- Diagnostic Flags ---
            // Skip if inlining limit reached (for debugging)
            if (config.opt_inline_call_limit >= 0 && g_inline_calls_so_far >= config.opt_inline_call_limit) {
                if (config.debug) {
                    AsmNode *debug_node = create_node(NULL, OP_OTHER, NULL, NULL, NULL);
                    snprintf(debug_node->raw, sizeof(debug_node->raw),
                             "; [DEBUG inline] Skipped CALL %s: hit -finline-call-limit=%d",
                             target_label, config.opt_inline_call_limit);
                    debug_node->prev = curr->prev;
                    debug_node->next = curr;
                    if (curr->prev) curr->prev->next = debug_node;
                    curr->prev = debug_node;
                }
                curr = next_node;
                continue;
            }

            // Skip if target is in exclude list
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
                        AsmNode *debug_node = create_node(NULL, OP_OTHER, NULL, NULL, NULL);
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
                if (str_case_eq(target_label, candidates[c].name) && trigger_allowed()) {
                    if (config.debug) {
                        AsmNode *debug_node = create_node(NULL, OP_OTHER, NULL, NULL, NULL);
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

                        // Rewrite [BP+N] → [SP+(N-2)]
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

                        // Regenerate raw text
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
