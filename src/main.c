#include "v32opt.h"
#include <getopt.h>

// Global variables
int   g_inline_call_limit          = -1;
int   g_inline_calls_so_far        = 0;
char  g_inline_exclude_name[1024]  = {0};

// -------------------------------------------------------------------
// Optimization Configuration
// -------------------------------------------------------------------
OptConfig config = {
    .verbose                      = false,
    .testing                      = false,
    .debug                        = false,
    .opt_peephole_algebra         = false,
    .opt_peephole_compiler_myopia = false,
    .opt_peephole_dead_stores     = false,
    .opt_peephole_forwarding      = false,
    .opt_peephole_immediate_prop  = false,
    .opt_peephole_immediates      = false,
    .opt_peephole_jmp_chain       = false,
    .opt_peephole_loads           = false,
    .opt_peephole_jumps           = false,
    .opt_peephole_movs            = false,
    .opt_peephole_pairs           = false,
    .opt_peephole_reduce          = false,
    .opt_peephole_shifts          = false,
    .opt_constant_folding         = false,
    .opt_cse                      = false,
    .opt_dce                      = false,
    .opt_omit_frame_pointers      = false,
    .opt_inline                   = false,
    .opt_inline_call_limit        = -1,
    .opt_inline_max_body_ins      = 8,
    .opt_promote_leaf             = false,
    .opt_promote_loops            = false,
    .opt_promote_regs             = false
};

// Helper macro for aligned optimization output
#define PRINT_OPT_LINE(name, count) \
    printf("  - %s:%-*s%d\n", (name), 24 - (int)strlen(name), "", (count))

// -------------------------------------------------------------------
// Main Entry Point
// -------------------------------------------------------------------
int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    // --- Initialize defaults ---
    char     inFile[256]                         = {0};
    char     outFile[256]                        = {0};
    char     dotFile[256]                        = {0};
    int      max_passes                          = 1000;
    OptType  tally[MAX_OPTIMIZATION_ALGORITHMS]  = { 0 };

    // --- Process command-line arguments ---
    // Update the call to include inFile and sizeof(inFile)
    process_args(argc, argv, &config,
                 inFile, sizeof(inFile),
                 outFile, sizeof(outFile),
                 dotFile, sizeof(dotFile), &max_passes);

    // --- Tally up and display enabled optimizations (verbose mode) ---
    if (config.verbose) {
        fprintf(stdout, "--- Configuration: Enabled Optimizations ---\n");
        int opt_count = 0;

        // Map OptType -> config field for this loop
        bool opt_states[MAX_OPTIMIZATION_ALGORITHMS] = {
            [OPT_PEEPHOLE_PAIRS]           = config.opt_peephole_pairs,
            [OPT_PEEPHOLE_ALGEBRA]         = config.opt_peephole_algebra,
            [OPT_PEEPHOLE_COMPILER_MYOPIA] = config.opt_peephole_compiler_myopia,
            [OPT_PEEPHOLE_FORWARDING]      = config.opt_peephole_forwarding,
            [OPT_PEEPHOLE_JUMPS]           = config.opt_peephole_jumps,
            [OPT_PEEPHOLE_MOVS]            = config.opt_peephole_movs,
            [OPT_PEEPHOLE_IMMEDIATES]      = config.opt_peephole_immediates,
            [OPT_PEEPHOLE_REDUCE]          = config.opt_peephole_reduce,
            [OPT_PEEPHOLE_SHIFTS]          = config.opt_peephole_shifts,
            [OPT_PEEPHOLE_DEAD_STORES]     = config.opt_peephole_dead_stores,
            [OPT_PEEPHOLE_LOADS]           = config.opt_peephole_loads,
            [OPT_PEEPHOLE_IMMEDIATE_PROP]  = config.opt_peephole_immediate_prop,
            [OPT_PEEPHOLE_JMP_CHAIN]       = config.opt_peephole_jmp_chain,
            [OPT_CONSTANT_FOLDING]         = config.opt_constant_folding,
            [OPT_CSE]                      = config.opt_cse,
            [OPT_DCE]                      = config.opt_dce,
            [OPT_OMIT_FRAME_POINTERS]      = config.opt_omit_frame_pointers,
            [OPT_INLINE]                   = config.opt_inline,
            [OPT_PROMOTE_REGS]             = config.opt_promote_regs,
            [OPT_PROMOTE_LEAF]             = config.opt_promote_leaf,
            [OPT_PROMOTE_LOOPS]            = config.opt_promote_loops
        };

        for (OptType t = 0; t < MAX_OPTIMIZATION_ALGORITHMS; t++) {
            if (opt_states[t]) {
                fprintf(stdout, "  - %s\n", opt_type_names[t]);
                opt_count++;
            }
        }
        fprintf(stdout, "    (%d total enabled)\n", opt_count);
    }

    // --- Set Output File ---
    if (strlen(outFile) == 0) {
        safe_str_copy(outFile, inFile, sizeof(outFile));
        char *ext = strrchr(outFile, '.');
        if (ext && strcmp(ext, ".asm") == 0) {
            *ext = '\0';
        }
        size_t remaining = sizeof(outFile) - strlen(outFile) - 1;
        if (remaining > 0) {
            strncat(outFile, "Opt.asm", remaining);
        }
    }

    // --- Parse Input Assembly File ---
    AsmNode *program_ast = parse_vircon32_asm(inFile);

    int passes = 0;
    int total_opts = 0;
    int opts_in_pass = 0;

    if (config.verbose) {
        printf("--- Starting Optimization Phase 1: Local Passes ---\n");
    }

    // ===================================================================
    // PHASE 1: Iterative Local & AST Optimizations
    // ===================================================================
    int pass_count = 0;
    do {
        opts_in_pass = 0;
        OptType opts[MAX_OPTIMIZATION_ALGORITHMS] = {0};
        pass_count++;

        // 1. Interprocedural & Structural
        if (config.opt_inline) {
            opts[OPT_INLINE] = inline_trivial_functions(program_ast);
        }

        // 2. Memory-to-Register Promotion
        if (config.opt_promote_regs) {
            opts[OPT_PROMOTE_REGS] = pass_promote_stack_slots(program_ast);
        }
        if (config.opt_promote_leaf) {
            opts[OPT_PROMOTE_LEAF] = pass_promote_stack_slots(program_ast);
        }
        if (config.opt_promote_loops) {
            opts[OPT_PROMOTE_LOOPS] = pass_promote_loop_registers(program_ast);
        }

        // compiler myopia
        if (config.opt_peephole_compiler_myopia) {
            opts[OPT_PEEPHOLE_COMPILER_MYOPIA] = peephole_compiler_myopia(program_ast);
        }
        // 3. Data-Flow Forwarding & Immediate Folding
        if (config.opt_peephole_forwarding) {
            opts[OPT_PEEPHOLE_FORWARDING] = peephole_forwarding(program_ast);
        }
        if (config.opt_peephole_immediates) {
            opts[OPT_PEEPHOLE_IMMEDIATES] = peephole_immediates(program_ast);
        }
        if (config.opt_peephole_reduce) {
            opts[OPT_PEEPHOLE_REDUCE] = peephole_reduce(program_ast);
        }

        // 4. Algebraic & Local Instruction Cleanup
        if (config.opt_peephole_algebra) {
            opts[OPT_PEEPHOLE_ALGEBRA] = peephole_algebra(program_ast);
        }
        if (config.opt_peephole_pairs) {
            opts[OPT_PEEPHOLE_PAIRS] = peephole_pairs(program_ast);
        }
        if (config.opt_peephole_movs) {
            opts[OPT_PEEPHOLE_MOVS] = peephole_movs(program_ast);
        }

        // 5. Additional Peephole Optimizations
        if (config.opt_peephole_dead_stores) {
            opts[OPT_PEEPHOLE_DEAD_STORES] = peephole_dead_stores(program_ast);
        }
        if (config.opt_peephole_loads) {
            opts[OPT_PEEPHOLE_LOADS] = peephole_loads(program_ast);
        }
        if (config.opt_peephole_immediate_prop) {
            opts[OPT_PEEPHOLE_IMMEDIATE_PROP] = peephole_immediate_prop(program_ast);
        }
        if (config.opt_peephole_jmp_chain) {
            opts[OPT_PEEPHOLE_JMP_CHAIN] = peephole_jmp_chain(program_ast);
        }
        if (config.opt_peephole_shifts) {
            opts[OPT_PEEPHOLE_SHIFTS] = peephole_shifts(program_ast);
        }

        // 6. Control Flow & Dead Code Cleanup
        if (config.opt_peephole_jumps) {
            opts[OPT_PEEPHOLE_JUMPS] = peephole_jumps(program_ast);
        }
        if (config.opt_cse) {
            opts[OPT_CSE] = opt_cse(program_ast);
        }
        if (config.opt_dce) {
            opts[OPT_DCE] = opt_dce(program_ast);
        }
        if (config.opt_omit_frame_pointers) {
            opts[OPT_OMIT_FRAME_POINTERS] = omit_frame_pointers(program_ast);
        }

        // Calculate totals
        for (int index = 0; index < MAX_OPTIMIZATION_ALGORITHMS; index++) {
            opts_in_pass += opts[index];
            tally[index] += opts[index];
        }
        total_opts += opts_in_pass;
        passes++;

        // Verbose output for this pass
        if (config.verbose && opts_in_pass > 0) {
            printf("Pass %d applied %d optimizations:\n", passes, opts_in_pass);
            for (int index = 0; index < MAX_OPTIMIZATION_ALGORITHMS; index++) {
                if (opts[index] > 0) {
                    PRINT_OPT_LINE(opt_type_names[index], opts[index]);
                }
            }
        }
    } while (opts_in_pass > 0 && pass_count < max_passes);

    // ===================================================================
    // PHASE 2: Global Data-Flow & CFG Optimizations
    // ===================================================================
    int global_folds = 0;
    ControlFlowGraph *cfg = NULL;

    if (config.opt_constant_folding) {
        if (config.verbose) {
            printf("--- Starting Optimization Phase 2: Global Data-Flow ---\n");
        }
        cfg = build_cfg(program_ast);
        propagate_constants_cfg(cfg);
        global_folds = fold_constants_cfg(cfg);
        total_opts += global_folds;
        tally[OPT_CONSTANT_FOLDING] += global_folds;

        if (config.verbose && global_folds > 0) {
            printf("  - Global constant folds: %d\n", global_folds);
        }
    }

    // ===================================================================
    // PHASE 3: Lightweight Post-CFG Cleanup
    // ===================================================================
    int cleanup_opts;
    do {
        cleanup_opts = 0;
        OptType opts[MAX_OPTIMIZATION_ALGORITHMS] = {0};

        // compiler myopia
        if (config.opt_peephole_compiler_myopia) {
            opts[OPT_PEEPHOLE_COMPILER_MYOPIA] = peephole_compiler_myopia(program_ast);
        }
        if (config.opt_peephole_immediates) {
            opts[OPT_PEEPHOLE_IMMEDIATES] = peephole_immediates(program_ast);
        }
        if (config.opt_peephole_reduce) {
            opts[OPT_PEEPHOLE_REDUCE] = peephole_reduce(program_ast);
        }
        if (config.opt_peephole_algebra) {
            opts[OPT_PEEPHOLE_ALGEBRA] = peephole_algebra(program_ast);
        }
        if (config.opt_peephole_pairs) {
            opts[OPT_PEEPHOLE_PAIRS] = peephole_pairs(program_ast);
        }
        if (config.opt_peephole_movs) {
            opts[OPT_PEEPHOLE_MOVS] = peephole_movs(program_ast);
        }
        if (config.opt_peephole_jumps) {
            opts[OPT_PEEPHOLE_JUMPS] = peephole_jumps(program_ast);
        }
        if (config.opt_peephole_dead_stores) {
            opts[OPT_PEEPHOLE_DEAD_STORES] = peephole_dead_stores(program_ast);
        }
        if (config.opt_peephole_loads) {
            opts[OPT_PEEPHOLE_LOADS] = peephole_loads(program_ast);
        }
        if (config.opt_peephole_immediate_prop) {
            opts[OPT_PEEPHOLE_IMMEDIATE_PROP] = peephole_immediate_prop(program_ast);
        }
        if (config.opt_peephole_jmp_chain) {
            opts[OPT_PEEPHOLE_JMP_CHAIN] = peephole_jmp_chain(program_ast);
        }
        if (config.opt_peephole_shifts) {
            opts[OPT_PEEPHOLE_SHIFTS] = peephole_shifts(program_ast);
        }
        if (config.opt_omit_frame_pointers) {
            opts[OPT_OMIT_FRAME_POINTERS] = omit_frame_pointers(program_ast);
        }

        for (int index = 0; index < MAX_OPTIMIZATION_ALGORITHMS; index++) {
            cleanup_opts += opts[index];
            tally[index] += opts[index];
        }
        total_opts += cleanup_opts;

        if (config.verbose && cleanup_opts > 0) {
            printf("Cleanup Pass applied %d follow-up optimizations:\n", cleanup_opts);
            for (int index = 0; index < MAX_OPTIMIZATION_ALGORITHMS; index++) {
                if (opts[index] > 0) {
                    PRINT_OPT_LINE(opt_type_names[index], opts[index]);
                }
            }
        }
    } while (cleanup_opts > 0);

    // --- Export CFG if requested ---
    if (strlen(dotFile) > 0) {
        if (!cfg) cfg = build_cfg(program_ast);
        export_cfg_to_dot(dotFile, cfg);
        if (config.verbose) {
            printf("CFG exported to '%s'.\n", dotFile);
        }
    }

    // --- Final Statistics ---
    if (config.verbose) {
        printf("\nOptimization complete: %d total optimizations applied.\n", total_opts);
    }

    // --- Testing Results ---
    if (config.testing) {
        for (int index = 0; index < MAX_OPTIMIZATION_ALGORITHMS; index++) {
            if (tally[index] > 0) {
                printf("%s:%d\n", opt_type_names[index], tally[index]);
            }
        }
        printf("total:%d\n", total_opts);
    }

    // --- Write Output File ---
    write_vircon32_asm(outFile, program_ast);

    // --- Cleanup ---
    if (cfg) free_cfg(cfg);
    free(program_ast);
    return 0;
}
