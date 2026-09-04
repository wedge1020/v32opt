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

// ---------------------------------------------------------------------
// Name-sorted lookup index for FunctionDef.name, used by opt_dce() to
// turn "is this operand text the name of one of our known functions"
// from an O(func_count) linear scan into an O(log func_count) binary
// search. See the "1C. BUILD A NAME-SORTED LOOKUP INDEX" comment in
// opt_dce() for why this matters.
// ---------------------------------------------------------------------
typedef struct {
    const char *name;
    int         idx;
} FuncNameIdx;

static int cmp_func_name_idx(const void *a, const void *b) {
    const FuncNameIdx *fa = (const FuncNameIdx *)a;
    const FuncNameIdx *fb = (const FuncNameIdx *)b;
    return strcasecmp(fa->name, fb->name);
}

// Returns the funcs[] index whose name matches 'name' (case-insensitive),
// or -1 if none match. 'name_idx' must be sorted with cmp_func_name_idx.
static int find_func_by_name(FuncNameIdx *name_idx, int func_count, const char *name) {
    if (!name_idx || !name || name[0] == '\0') return -1;
    int lo = 0, hi = func_count - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        int cmp = strcasecmp(name_idx[mid].name, name);
        if (cmp == 0) return name_idx[mid].idx;
        else if (cmp < 0) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}
int opt_dce (AsmNode *head)
{
    // --- STEP 1: COLLECT ALL FUNCTION DEFINITIONS ---
    //
    // BUG FIX: funcs[] and worklist[] used to be fixed-size stack arrays
    // capped at MAX_FUNCTIONS (4096). On a real program with more
    // functions than that (a genuine C-mode program was seen with 8784),
    // registration silently stopped at the 4096th function -- no error,
    // no warning -- and every function after that point was invisible to
    // this pass for the rest of the run. That's not just "those specific
    // functions never get DCE'd" (which would be merely a missed
    // optimization) -- it's actively unsound: an invisible function's own
    // CALL instructions are never scanned in step 4 (WORKLIST TRAVERSAL),
    // so if some registered, otherwise-genuinely-reachable function X is
    // only ever called BY one of the invisible functions, that edge is
    // never discovered and X gets swept as "unreachable" even though it's
    // very much still called somewhere in the program. Confirmed exactly
    // this on the 8784-function program: registration hit the 4096 cap,
    // and afterward real, called function definitions (e.g.
    // __function_rupComposeMapByte, called twice) were removed while
    // their CALL sites were left behind -- dangling references into
    // deleted code.
    //
    // Fix: size funcs[] and worklist[] to the program's actual label
    // count (a cheap single pass, and a safe upper bound since every
    // function boundary is a label, though not every label is a
    // function), heap-allocated instead of a fixed stack array. No more
    // arbitrary cap.
    int label_count = 1; // +1 for the synthetic "__boot_vector" entry
    for (AsmNode *n = head; n != NULL; n = n->next) {
        if (n->type == OP_LABEL) label_count++;
    }

    FunctionDef *funcs = malloc(sizeof(FunctionDef) * (size_t)(label_count + 1));
    int func_count = 0;
    int funcs_cap  = label_count + 1;
    if (!funcs) return 0; // allocation failure: bail out safely, no changes made

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

        if (func_count < funcs_cap) {
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
            if (func_count < funcs_cap) {
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

    if (func_count == 0) { free(funcs); return 0; }

    // ----------------------------------------------------------------
    // 1C. BUILD A NAME-SORTED LOOKUP INDEX
    // ----------------------------------------------------------------
    // PERF FIX: steps 3 and 4 below used to do a full O(func_count)
    // linear scan, per operand, per instruction, to test "does this
    // operand's text name one of our known functions". On a real
    // program with thousands of functions and a function body that is
    // itself hundreds of thousands of instructions long (a large
    // sequential data/struct initializer is a completely normal thing
    // for a compiler to emit as one giant branch-free block), that's
    // O(node_count * func_count) -- with node_count and func_count in
    // the hundreds of thousands and thousands respectively, that is
    // billions of string compares for a single function, which in
    // practice never finishes ("hangs after first pass, never writes
    // output"). Sort a small (name, index) index once and binary-search
    // it instead: O(func_count log func_count) to build, O(log
    // func_count) per lookup. This does not change what DCE decides is
    // reachable -- it is the exact same name-equality test, just no
    // longer paying for it func_count times per instruction.
    FuncNameIdx *name_idx = malloc(sizeof(FuncNameIdx) * func_count);
    if (name_idx) {
        for (int i = 0; i < func_count; i++) {
            name_idx[i].name = funcs[i].name;
            name_idx[i].idx  = i;
        }
        qsort(name_idx, func_count, sizeof(FuncNameIdx), cmp_func_name_idx);
    }

    // ----------------------------------------------------------------
    // 2. SEED REACHABILITY WORKLIST
    // ----------------------------------------------------------------
    // Known entry points that are always reachable
    // Same fix as funcs[] above: size to the actual (now known) function
    // count instead of a fixed MAX_FUNCTIONS cap.
    char (*worklist)[128] = malloc(sizeof(char[128]) * (size_t)(func_count + 1));
    int worklist_cap = func_count + 1;
    if (!worklist) { free(funcs); return 0; }
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
            if (worklist_size < worklist_cap) {
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
                    int f = find_func_by_name(name_idx, func_count, token);
                    if (f >= 0 && !funcs[f].reachable) {
                        funcs[f].reachable = true;
                        if (worklist_size < worklist_cap) {
                            safe_str_copy(worklist[worklist_size++], funcs[f].name, sizeof(worklist[0]));
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
        int fn_idx = find_func_by_name(name_idx, func_count, current_label);
        if (fn_idx >= 0) fn = &funcs[fn_idx];
        if (!fn) continue;

        // Scan the function body for calls to other functions
        for (AsmNode *node = fn->start_node; node != NULL; node = node->next) {
            if (node->type == OP_LABEL || (node->type == OP_OTHER && node->raw[0] == ';')) {
                if (node == fn->end_node) break;
                continue;
            }

            char *op_dst = trim(node->dst_op.raw);
            char *op_src = trim(node->src_op.raw);

            // Check if this instruction references another function.
            // (Was an O(func_count) linear scan per operand per node --
            // see the "1C." comment above for why that's fatal on a
            // large program. Same name-equality test, O(log func_count).)
            int f_dst = (op_dst[0] != '\0') ? find_func_by_name(name_idx, func_count, op_dst) : -1;
            if (f_dst >= 0 && !funcs[f_dst].reachable) {
                funcs[f_dst].reachable = true;
                if (worklist_size < worklist_cap) {
                    safe_str_copy(worklist[worklist_size++], funcs[f_dst].name, sizeof(worklist[0]));
                }
            }
            int f_src = (op_src[0] != '\0') ? find_func_by_name(name_idx, func_count, op_src) : -1;
            if (f_src >= 0 && !funcs[f_src].reachable) {
                funcs[f_src].reachable = true;
                if (worklist_size < worklist_cap) {
                    safe_str_copy(worklist[worklist_size++], funcs[f_src].name, sizeof(worklist[0]));
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
        if (!funcs[i].reachable && trigger_allowed()) {
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

    free(name_idx);
    free(worklist);
    free(funcs);

    return eliminated_funcs;
}
