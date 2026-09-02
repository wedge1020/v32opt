#include "v32opt.h"

// ---------------------------------------------------------------
// Helper: does this trimmed label LINE (including its trailing colon,
// e.g. "__function_foo:", "__start:", "__literal_string_3:",
// "__global_scope_initialization:") mark a genuine top-level function
// or data boundary -- as opposed to an internal control-flow label
// (__if_N_start:, __for_N_start:, __start:, a function's own _return:,
// etc.) that belongs to whatever code textually surrounds it?
//
// BUG FIX: this used to special-case "__global_scope_initialization:"
// out of the boundary set, on the theory that "it's real code, not a
// boundary to stop at." That's backwards -- it *is* a boundary (the
// start of its own function), and excluding it from this check meant
// that whichever function happened to be textually defined just before
// it in the .asm (position varies per program) would never stop its own
// "find my end" scan there, silently swallowing the entire
// __global_scope_initialization body into that unrelated function.
// Concretely, in nostalgick.asm this merged __global_scope_initialization
// into __function_game_loop's range, and since __function_game_loop was
// itself never proven reachable (see the boot-vector fix below), that
// merge dragged global-scope init down with it into the sweep too.
// __global_scope_initialization is still correctly recognized and
// registered as ITS OWN function -- see the is_global_scope_init check
// below -- it just also needs to behave like every other boundary label
// when some OTHER function's body is being measured.
// ---------------------------------------------------------------
static bool is_function_boundary_label(const char *lbl) {
    size_t lbl_len = strlen(lbl);
    bool is_return_label = (lbl_len >= 8 && str_case_eq(lbl + lbl_len - 8, "_return:"));
    return (strncmp(lbl, "__function_", 11) == 0 && !is_return_label) ||
           strncmp(lbl, "__literal_", 10) == 0 ||
           strncmp(lbl, "__global_", 9) == 0; // includes __global_scope_initialization: on purpose
}

// -------------------------------------------------------------------
// OPTIMIZATION: Dead Code Elimination
// Removes functions that are never called (unreachable) from the program.
// -------------------------------------------------------------------
// ===================================================================
// DEAD CODE ELIMINATION
//
// Identifies and removes unreachable functions using a reachability analysis:
//   1. Captures the boot vector (unlabeled preamble) as a special entry point
//   2. Discovers function boundaries by finding labels with the compiler's
//      function prefix (__function_) or special entry points
//   3. Seeds the worklist with known entry points (main, _start, ISRs, etc.)
//   4. Propagates reachability through CALL instructions and pointer directives
//   5. Sweeps away any function not marked as reachable
//
// FIXES APPLIED:
//   - Only treats __function_ prefixed labels as real function boundaries
//     (internal labels like __if_N_start, __for_N_start, _return: are NOT functions)
//   - Explicitly handles __global_scope_initialization (called by program-start)
//   - Excludes __literal_string_N and __global_NAME data labels from function set
//   - Properly handles _return labels (internal to functions, not separate entries)
//
// Note: On Vircon32, all instructions are 1 cycle, so this optimization
// only reduces code size, not execution time.
// ===================================================================
int opt_dce (AsmNode *head)
{
    // --- STEP 1: COLLECT ALL FUNCTION DEFINITIONS ---
    FunctionDef funcs[MAX_FUNCTIONS];
    int func_count = 0;

    AsmNode *curr = head ? head->next : NULL;

    // ----------------------------------------------------------------
    // 1A. Capture Unlabeled Preamble (Boot Vector)
    // ----------------------------------------------------------------
    // The program may start with a sequence of instructions before
    // any label (e.g., CALLs to initialization routines). Treat this
    // as a special "__boot_vector" entry point.
    AsmNode *preamble_start = curr;
    while (preamble_start && preamble_start->type == OP_OTHER &&
          (preamble_start->raw[0] == '\0' || preamble_start->raw[0] == ';')) {
        preamble_start = preamble_start->next;
    }

    if (preamble_start && preamble_start->type != OP_LABEL) {
        AsmNode *preamble_end = preamble_start;

        // BUG FIX: this used to stop at the FIRST label, full stop --
        // but real "entry vector" preambles routinely contain their own
        // internal loop-back label (e.g. nostalgick.asm's
        // "__start:  CALL __function_game_loop / WAIT / JMP __start"),
        // which is not a callable function, just a jump target inside
        // the same entry-vector code. Stopping there cut the CALL to
        // __function_game_loop out of __boot_vector's tracked range
        // entirely -- it was never a member of ANY FunctionDef, so the
        // reachability scan below (step 4) never saw that CALL, so
        // __function_game_loop (and everything only reachable through
        // it -- in practice, nearly the whole program) was wrongly swept
        // as "unreachable." Now we keep extending through any label that
        // ISN'T a genuine function/data boundary, so the whole entry
        // vector -- labels, loop-back jumps, and all -- stays inside
        // __boot_vector where its CALLs are actually scanned.
        while (preamble_end->next) {
            AsmNode *look = preamble_end->next;
            if (look->type == OP_LABEL) {
                char line_copy[8192];
                safe_str_copy(line_copy, look->raw, sizeof(line_copy));
                char *lbl = trim(line_copy);
                if (is_function_boundary_label(lbl)) break;
            }
            preamble_end = look;
        }

        if (func_count < MAX_FUNCTIONS) {
            safe_str_copy(funcs[func_count].name, "__boot_vector", sizeof(funcs[func_count].name));
            funcs[func_count].start_node = preamble_start;
            funcs[func_count].end_node = preamble_end;
            funcs[func_count].reachable = false;
            func_count++;
        }
    }

    // ----------------------------------------------------------------
    // 1B. Discover Standard Function Boundaries
    // ----------------------------------------------------------------
    while (curr) {
        if (curr->type == OP_LABEL) {
            char line_copy[8192];
            safe_str_copy(line_copy, curr->raw, sizeof(line_copy));
            char *lbl = trim(line_copy);

            // FIX: Only __function_ prefixed labels are real functions.
            // Internal compiler labels (__if_N_start, __for_N_start, etc.)
            // and _return labels are NOT separate functions.
            size_t lbl_len = strlen(lbl);
            bool is_return_label = (lbl_len >= 8 && str_case_eq(lbl + lbl_len - 8, "_return:"));
            bool is_global_scope_init = str_case_eq(lbl, "__global_scope_initialization:");

            // Skip non-function labels (internal control flow, data labels)
            if ((strncmp(lbl, "__function_", 11) != 0 && !is_global_scope_init) || is_return_label) {
                curr = curr->next;
                continue;
            }

            // Extract function name (remove trailing colon)
            char func_name[128] = {0};
            safe_str_copy(func_name, lbl, sizeof(func_name));
            char *colon = strchr(func_name, ':');
            if (colon) *colon = '\0';
            trim(func_name);

            // Find the end of this function (next function label or data label)
            AsmNode *scan = curr->next;
            AsmNode *end_of_func = curr;

            while (scan) {
                if (scan->type == OP_LABEL) {
                    char next_copy[8192];
                    safe_str_copy(next_copy, scan->raw, sizeof(next_copy));
                    char *next_lbl = trim(next_copy);

                    // FIX: use the shared, corrected boundary predicate --
                    // __global_scope_initialization: now stops this scan
                    // like any other real boundary (see the comment on
                    // is_function_boundary_label() above).
                    if (is_function_boundary_label(next_lbl)) {
                        break;
                    }
                }
                end_of_func = scan;
                scan = scan->next;
            }

            // Register this function
            if (func_count < MAX_FUNCTIONS) {
                safe_str_copy(funcs[func_count].name, func_name, sizeof(funcs[func_count].name));
                funcs[func_count].start_node = curr;
                funcs[func_count].end_node = end_of_func;
                funcs[func_count].reachable = false;
                func_count++;

                curr = scan; // Skip to next function
                continue;
            }
        }
        curr = curr->next;
    }

    if (func_count == 0) return 0;

    // ----------------------------------------------------------------
    // 2. SEED REACHABILITY WORKLIST
    // ----------------------------------------------------------------
    // Known entry points that are always reachable
    char worklist[MAX_FUNCTIONS][128];
    int worklist_size = 0;

    for (int i = 0; i < func_count; i++) {
        if (str_case_eq(funcs[i].name, "__boot_vector") ||
            str_case_eq(funcs[i].name, "__function_main") ||
            str_case_eq(funcs[i].name, "main") ||
            str_case_eq(funcs[i].name, "_start") ||
            str_case_eq(funcs[i].name, "start") ||
            str_case_eq(funcs[i].name, "__start") ||
            str_case_eq(funcs[i].name, "__init_globals") ||
            str_case_eq(funcs[i].name, "__function_init") ||
            str_case_eq(funcs[i].name, "__global_scope_initialization") ||
            strstr(funcs[i].name, "global_scope") != NULL ||
            strstr(funcs[i].name, "ISR") != NULL ||
            strstr(funcs[i].name, "interrupt") != NULL)
        {
            funcs[i].reachable = true;
            if (worklist_size < MAX_FUNCTIONS) {
                safe_str_copy(worklist[worklist_size++], funcs[i].name, sizeof(worklist[0]));
            }
        }
    }

    // Fallback: if no known entries found, mark first function as reachable
    if (worklist_size == 0 && func_count > 0) {
        funcs[0].reachable = true;
        safe_str_copy(worklist[worklist_size++], funcs[0].name, sizeof(worklist[0]));
    }

    // ----------------------------------------------------------------
    // 3. SCAN FOR `pointer` DIRECTIVES
    // ----------------------------------------------------------------
    // The compiler emits `pointer func1, func2, ...` directives for
    // function pointers. Any function listed here is reachable.
    for (AsmNode *node = head; node != NULL; node = node->next) {
        if (str_case_eq(node->mnemonic, "pointer")) {
            char args_copy[8192];
            safe_str_copy(args_copy, node->raw, sizeof(args_copy));

            char *token = strtok(args_copy, " ,\t\n\r");
            if (token && str_case_eq(token, "pointer")) {
                token = strtok(NULL, " ,\t\n\r");
                while (token != NULL) {
                    for (int f = 0; f < func_count; f++) {
                        if (str_case_eq(funcs[f].name, token) && !funcs[f].reachable) {
                            funcs[f].reachable = true;
                            if (worklist_size < MAX_FUNCTIONS) {
                                safe_str_copy(worklist[worklist_size++], funcs[f].name, sizeof(worklist[0]));
                            }
                        }
                    }
                    token = strtok(NULL, " ,\t\n\r");
                }
            }
        }
    }

    // ----------------------------------------------------------------
    // 4. WORKLIST TRAVERSAL
    // ----------------------------------------------------------------
    // Propagate reachability: if a reachable function calls another,
    // the callee becomes reachable.
    while (worklist_size > 0) {
        char current_label[128];
        safe_str_copy(current_label, worklist[--worklist_size], sizeof(current_label));

        FunctionDef *fn = NULL;
        for (int i = 0; i < func_count; i++) {
            if (str_case_eq(funcs[i].name, current_label)) {
                fn = &funcs[i];
                break;
            }
        }
        if (!fn) continue;

        // Scan the function body for calls to other functions
        for (AsmNode *node = fn->start_node; node != NULL; node = node->next) {
            if (node->type == OP_LABEL || (node->type == OP_OTHER && node->raw[0] == ';')) {
                if (node == fn->end_node) break;
                continue;
            }

            char *op_dst = trim(node->dst_op.raw);
            char *op_src = trim(node->src_op.raw);

            // Check if this instruction references another function
            for (int f = 0; f < func_count; f++) {
                if (funcs[f].reachable) continue; // Already reachable

                if ((strlen(op_dst) > 0 && str_case_eq(funcs[f].name, op_dst)) ||
                    (strlen(op_src) > 0 && str_case_eq(funcs[f].name, op_src))) {
                    funcs[f].reachable = true;
                    if (worklist_size < MAX_FUNCTIONS) {
                        safe_str_copy(worklist[worklist_size++], funcs[f].name, sizeof(worklist[0]));
                    }
                }
            }

            if (node == fn->end_node) break;
        }
    }

    // ----------------------------------------------------------------
    // 5. SWEEP UNREACHABLE FUNCTIONS
    // ----------------------------------------------------------------
    int eliminated_funcs = 0;
    for (int i = 0; i < func_count; i++) {
        if (!funcs[i].reachable) {
            // Remove all nodes in this function's range
            AsmNode *sweep_curr = funcs[i].start_node;
            AsmNode *sweep_end = funcs[i].end_node;

            while (sweep_curr) {
                AsmNode *next = sweep_curr->next;
                bool is_last = (sweep_curr == sweep_end);
                remove_node(sweep_curr);
                if (is_last) break;
                sweep_curr = next;
            }
            eliminated_funcs++;
        }
    }

    return eliminated_funcs;
}
