#include "v32opt.h"

// -------------------------------------------------------------------
// Pass: Trivial Leaf Function Inlining
// -------------------------------------------------------------------
int pass_inline_trivial_functions(AsmNode *head) {
    InlineCandidate candidates[MAX_INLINE_CANDIDATES];
    int candidate_count = 0;

    AsmNode *curr = head ? head->next : NULL;

    while (curr) {
        if (curr->type == OP_LABEL && candidate_count < MAX_INLINE_CANDIDATES) {
            char func_name[128] = {0}; 
            char line_copy[8192];
            safe_str_copy(line_copy, curr->raw, sizeof(line_copy));
            char *trimmed_lbl = trim(line_copy);

            safe_str_copy(func_name, trimmed_lbl, sizeof(func_name));
            char *colon = strchr(func_name, ':');
            if (colon) *colon = '\0';
            trim(func_name);

            AsmNode *scan = curr->next;
            while (scan && (scan->type == OP_OTHER || scan->type == OP_LABEL)) scan = scan->next;

            if (scan && scan->type == OP_PUSH && str_case_eq(scan->dst_op.reg, "BP")) {
                scan = scan->next;
                if (scan && scan->type == OP_MOV && str_case_eq(scan->dst_op.reg, "BP") && str_case_eq(scan->src_op.reg, "SP")) {
                    scan = scan->next;
                }
            }

            AsmNode *core_nodes[MAX_BODY_INS];
            int core_count = 0;
            bool valid_candidate = true;

            while (scan) {
                if (scan->type == OP_LABEL) {
                    scan = scan->next;
                    continue;
                }

                if (scan->type == OP_MOV && str_case_eq(scan->dst_op.reg, "SP") && str_case_eq(scan->src_op.reg, "BP")) break;
                if (str_case_eq(scan->mnemonic, "RET")) break;

                if (str_case_eq(scan->mnemonic, "CALL") || str_case_eq(scan->mnemonic, "JMP") ||
                    str_case_eq(scan->mnemonic, "JT")   || str_case_eq(scan->mnemonic, "JF")) {
                    valid_candidate = false;
                    break;
                }

                // FIX: reject any candidate whose body touches SP
                // directly (PUSH/POP, or any instruction with SP as an
                // operand register) - these change the stack layout
                // mid-body, which the BP->SP offset rewrite below can't
                // account for. Also reject any [BP-N] (negative offset)
                // reference, which would indicate a local-variable slot
                // the callee's own (skipped) prologue reserved space
                // for - not something this rewrite handles either.
                if (str_case_eq(scan->mnemonic, "PUSH") || str_case_eq(scan->mnemonic, "POP")) {
                    valid_candidate = false;
                    break;
                }
                if (str_case_eq(scan->dst_op.reg, "SP") || str_case_eq(scan->src_op.reg, "SP")) {
                    valid_candidate = false;
                    break;
                }
                if ((scan->dst_op.mode == MODE_INDIRECT && str_case_eq(scan->dst_op.reg, "BP") && scan->dst_op.offset < 0) ||
                    (scan->src_op.mode == MODE_INDIRECT && str_case_eq(scan->src_op.reg, "BP") && scan->src_op.offset < 0)) {
                    valid_candidate = false;
                    break;
                }

                if (core_count < MAX_BODY_INS) {
                    core_nodes[core_count++] = scan;
                } else {
                    valid_candidate = false;
                    break;
                }

                scan = scan->next;
            }

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

    int inlined_calls = 0;
    curr = head ? head->next : NULL;

    // FIX (ported from v32opt-fixed round 8): the compiler always
    // emits the program's actual entry sequence ("; program start
    // section" - a handful of CALLs plus HLT) as the very first thing
    // in the file, before the "%define global_X N" block that gives
    // every global variable's memory address a name. Those %define
    // lines aren't real instructions - they're pass-through text this
    // tool never reorders - so if this pass inlines a candidate
    // called from the program-start section, the callee's body
    // (referencing "[global_X]" by name) ends up spliced in *before*
    // the point in the file where "global_X" is ever defined,
    // producing an assembler error on an otherwise valid program.
    // Confirmed directly against BasicPlatformer
    // (github.com/vircon32/ConsoleSoftware): its program-start section
    // calls __global_scope_initialization, which sets four "[global_X]"
    // variables and easily qualifies as a trivial leaf function under
    // every other rule this pass checks - inlining it moved those four
    // assignments above their own %define block. Tracking whether the
    // first label has been seen yet and refusing to inline anything
    // before it covers this exactly: everything in the program-start
    // section keeps calling out to a real function instead, which is
    // unaffected by define ordering since CALL just jumps to wherever
    // the label ends up in the file.
    bool seen_first_label = false;

    while (curr) {
        AsmNode *next_node = curr->next;

        if (curr->type == OP_LABEL) {
            seen_first_label = true;
        }

        if (str_case_eq(curr->mnemonic, "CALL") && seen_first_label) {
            char target_label[128] = {0}; 
            safe_str_copy(target_label, trim(curr->dst_op.raw), sizeof(target_label));

            // DIAGNOSTIC ONLY (not a correctness fix): g_inline_call_limit,
            // set via -finline-max=N, caps how many CALL sites get
            // inlined, in file order, so a real build can be bisected by
            // testing N values against actual hardware/emulator - added
            // to narrow down which of many inlined call sites causes a
            // runtime-only symptom (confirmed-silent audio in a full
            // -O3 build) that static tracing of the obvious candidates
            // didn't explain. Default -1 means no limit (normal
            // behavior, unchanged).
            if (g_inline_call_limit >= 0 && g_inline_calls_so_far >= g_inline_call_limit) {
                curr = next_node;
                continue;
            }
            // DIAGNOSTIC ONLY: g_inline_exclude_name, set via
            // -finline-exclude=NAME, skips inlining just that one named
            // function (everything else inlines normally) - for testing
            // one specific function's inlining in isolation against a
            // real, otherwise-full -O3 build.
            if (g_inline_exclude_name[0] != '\0') {
                // Comma-separated list support: check target_label
                // against each token.
                char list_copy[1024];
                safe_str_copy(list_copy, g_inline_exclude_name, sizeof(list_copy));
                bool excluded = false;
                char *tok = strtok(list_copy, ",");
                while (tok) {
                    if (str_case_eq(trim(tok), target_label)) { excluded = true; break; }
                    tok = strtok(NULL, ",");
                }
                if (excluded) {
                    curr = next_node;
                    continue;
                }
            }

            for (int c = 0; c < candidate_count; c++) {
                if (str_case_eq(target_label, candidates[c].name)) {
                    AsmNode *insertion_point = curr->prev;

                    for (int b = 0; b < candidates[c].body_count; b++) {
                        AsmNode *inlined_ins = clone_node(candidates[c].body_nodes[b]);

                        // FIX: rewrite any [BP+N] (N>=2) parameter
                        // reference in the cloned instruction to
                        // [SP+(N-2)] instead - [BP+N] was only valid
                        // relative to the callee's own prologue-
                        // established BP, which no longer exists once
                        // this body is spliced into the caller with no
                        // new prologue of its own. At the call site,
                        // [BP+N] (inside the callee) and [SP+(N-2)] (at
                        // the call site, before the call executes) refer
                        // to the exact same stack slot - confirmed
                        // empirically: a single-argument callee reads
                        // its parameter at [BP+2], and the caller pushes
                        // that same argument to [SP+0] immediately
                        // before the call.
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
                        // write_vircon32_asm() emits raw directly, not
                        // the parsed operand structs - regenerate it
                        // from the (possibly rewritten) operands above,
                        // preserving the original instruction's own
                        // mnemonic case, or the offset fix never reaches
                        // the actual output.
                        if (inlined_ins->has_dst && inlined_ins->has_src) {
                            snprintf(inlined_ins->raw, sizeof(inlined_ins->raw), "  %s %s, %s",
                                     inlined_ins->mnemonic, inlined_ins->dst_op.raw, inlined_ins->src_op.raw);
                        } else if (inlined_ins->has_dst) {
                            snprintf(inlined_ins->raw, sizeof(inlined_ins->raw), "  %s %s",
                                     inlined_ins->mnemonic, inlined_ins->dst_op.raw);
                        }

                        inlined_ins->prev = insertion_point;
                        inlined_ins->next = curr;
                        if (insertion_point) insertion_point->next = inlined_ins;
                        curr->prev = inlined_ins;

                        insertion_point = inlined_ins;
                    }

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
