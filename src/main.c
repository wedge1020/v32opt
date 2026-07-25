#include "v32opt.h"

int   g_inline_call_limit          = -1;
int   g_inline_calls_so_far        = 0;
char  g_inline_exclude_name[1024]  = {0};

// -------------------------------------------------------------------
// Main Entry Point
// -------------------------------------------------------------------
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf (stdout, "Usage: %s <input.asm> [-o output.asm] [options]\n", argv[0]);
        fprintf (stdout, "Options:\n");
        fprintf (stdout, "  -v               Verbose output (show pass statistics)\n");
        fprintf (stdout, "  --dot <cfg.dot>  Export Control Flow Graph to DOT format\n");
        fprintf (stdout, "  -o <file>        Specify output assembly file name\n");
        fprintf (stdout, "  -O0              Disable all optimizations [default]\n");
        fprintf (stdout, "  -O1              Enables first level of optimizations:\n");
        fprintf (stdout, "                       peephole,  algebraic, forwarding,\n");
        fprintf (stdout, "                       jump_next, redundant_movs,\n");
        fprintf (stdout, "                       combine_immediates, strength_reduction\n");
        fprintf (stdout, "  -O2              Enables second level (could break):\n");
        fprintf (stdout, "                       ALL included in -O1,\n");
        fprintf (stdout, "                       dce, constant_folding\n");
        fprintf (stdout, "  -O3              Enables aggressive optimizations (could break):\n");
        fprintf (stdout, "                       ALL included in -O1 and -O2,\n");
        fprintf (stdout, "                       inline\n\n");
        fprintf (stdout, "Individual Optimization Toggles:\n");
        fprintf (stdout, "  -fopt_<name>     Enable specific pass (e.g., -fopt_promote_regs)\n");
        fprintf (stdout, "  -fno_opt_<name>  Disable specific pass (e.g., -fno_opt_inline)\n\n");
        fprintf (stdout, "Available pass names:\n");
        fprintf (stdout, "  peephole, algebraic, forwarding, jump_next, redundant_movs,\n");
        fprintf (stdout, "  combine_immediates, strength_reduction, inline, dce,\n");
        fprintf (stdout, "  constant_folding, promote_regs, promote_leaf, promote_loops\n\n");
        fprintf (stdout, "Diagnostic Flags:\n");
        fprintf (stdout, "  -finline-max=N   Cap the number of inlined CALL sites to N\n");
        fprintf (stdout, "  -fmax_passes=N   Cap the maximum iterative optimization passes to N\n\n");
        fprintf (stdout, "NOTE: promote_regs, promote_leaf, and promote_loops not yet\n");
        fprintf (stdout, "connected to any optimization category. Test and bugfix first\n\n");
        return (1);
    }

    char *inFile = argv[1];
    char outFile[256] = {0};
    char dotFile[256] = {0};
    int  max_passes   = 1000;

    OptConfig config = {
        .verbose = false,
        .enable_peephole = false,
        .enable_algebraic = false,
        .enable_forwarding = false,
        .enable_inline = false,
        .enable_dce = false,
        .enable_constant_folding = false,
        .enable_jump_next = false,
        .enable_redundant_movs = false,
        .enable_combine_immediates = false,
        .enable_strength_reduction = false,
        .enable_promote_regs = false,
        .enable_promote_leaf = false,
        .enable_promote_loops = false
    };

    int out_idx = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            safe_str_copy(outFile, argv[i+1], sizeof(outFile));
            out_idx = i;
            i++; // Consume filename argument
        } else if (strcmp(argv[i], "--dot") == 0 && i + 1 < argc) {
            safe_str_copy(dotFile, argv[i+1], sizeof(dotFile));
            i++;
        } else if (strcmp(argv[i], "-v") == 0) {
            config.verbose = true;
        } else if (strcmp(argv[i], "-O0") == 0) {
            config.enable_peephole = false;
            config.enable_algebraic = false;
            config.enable_forwarding = false;
            config.enable_inline = false;
            config.enable_dce = false;
            config.enable_constant_folding = false;
            config.enable_jump_next = false;
            config.enable_redundant_movs = false;
            config.enable_combine_immediates = false;
            config.enable_strength_reduction = false;
            config.enable_promote_regs = false;
            config.enable_promote_leaf = false;
            config.enable_promote_loops = false;
        } else if (strcmp(argv[i], "-O1") == 0) {
            config.enable_peephole = true;
            config.enable_algebraic = true;
            config.enable_forwarding = true;
            config.enable_jump_next = true;
            config.enable_redundant_movs = true;
            config.enable_combine_immediates = true;
            config.enable_strength_reduction = true;
            config.enable_inline = false;
            config.enable_dce = false;
            config.enable_constant_folding = false;
            config.enable_promote_regs = false;
            config.enable_promote_leaf = false;
            config.enable_promote_loops = false;
        } else if (strcmp(argv[i], "-O2") == 0) {
            config.enable_peephole = true;
            config.enable_algebraic = true;
            config.enable_forwarding = true;
            config.enable_jump_next = true;
            config.enable_redundant_movs = true;
            config.enable_combine_immediates = true;
            config.enable_strength_reduction = true;
            config.enable_inline = false;
            config.enable_dce = true;
            config.enable_constant_folding = true;
            config.enable_promote_regs = false;
            config.enable_promote_leaf = false;
            config.enable_promote_loops = false;
        } else if (strcmp(argv[i], "-O3") == 0) {
            config.enable_peephole = true;
            config.enable_algebraic = true;
            config.enable_forwarding = true;
            config.enable_jump_next = true;
            config.enable_redundant_movs = true;
            config.enable_combine_immediates = true;
            config.enable_strength_reduction = true;
            config.enable_inline = true;
            config.enable_dce = true;
            config.enable_constant_folding = true;
            config.enable_promote_regs = false;
            config.enable_promote_leaf = false;
            config.enable_promote_loops = false;
        // Positive (-fopt_NAME) Flags:
        }
        else if (strcmp(argv[i], "-fopt_promote_regs") == 0)       config.enable_promote_regs = true;
        else if (strcmp(argv[i], "-fopt_promote_leaf") == 0)       config.enable_promote_leaf = true;
        else if (strcmp(argv[i], "-fopt_promote_loops") == 0)      config.enable_promote_loops = true;
        else if (strcmp(argv[i], "-fopt_jump_next") == 0)          config.enable_jump_next = true;
        else if (strcmp(argv[i], "-fopt_redundant_movs") == 0)     config.enable_redundant_movs = true;
        else if (strcmp(argv[i], "-fopt_combine_immediates") == 0) config.enable_combine_immediates = true;
        else if (strcmp(argv[i], "-fopt_strength_reduction") == 0) config.enable_strength_reduction = true;
        else if (strcmp(argv[i], "-fopt_peephole") == 0)           config.enable_peephole = true;
        else if (strcmp(argv[i], "-fopt_algebraic") == 0)          config.enable_algebraic = true;
        else if (strcmp(argv[i], "-fopt_forwarding") == 0)         config.enable_forwarding = true;
        else if (strcmp(argv[i], "-fopt_inline") == 0)             config.enable_inline = true;
        else if (strcmp(argv[i], "-fopt_dce") == 0)                config.enable_dce = true;
        else if (strcmp(argv[i], "-fopt_constant_folding") == 0)   config.enable_constant_folding = true;
        // Negative (-fno_opt_NAME) Flags:
        else if (strcmp(argv[i], "-fno_opt_promote_regs") == 0)       config.enable_promote_regs = false;
        else if (strcmp(argv[i], "-fno_opt_promote_leaf") == 0)       config.enable_promote_leaf = false;
        else if (strcmp(argv[i], "-fno_opt_promote_loops") == 0)      config.enable_promote_loops = false;
        else if (strcmp(argv[i], "-fno_opt_jump_next") == 0)          config.enable_jump_next = false;
        else if (strcmp(argv[i], "-fno_opt_redundant_movs") == 0)     config.enable_redundant_movs = false;
        else if (strcmp(argv[i], "-fno_opt_combine_immediates") == 0) config.enable_combine_immediates = false;
        else if (strcmp(argv[i], "-fno_opt_strength_reduction") == 0) config.enable_strength_reduction = false;
        else if (strcmp(argv[i], "-fno_opt_peephole") == 0)           config.enable_peephole = false;
        else if (strcmp(argv[i], "-fno_opt_algebraic") == 0)          config.enable_algebraic = false;
        else if (strcmp(argv[i], "-fno_opt_forwarding") == 0)         config.enable_forwarding = false;
        else if (strcmp(argv[i], "-fno_opt_inline") == 0)             config.enable_inline = false;
        else if (strcmp(argv[i], "-fno_opt_dce") == 0)                config.enable_dce = false;
        else if (strcmp(argv[i], "-fno_opt_constant_folding") == 0)   config.enable_constant_folding = false;
        // Diagnostic Flags:
        else if (strncmp(argv[i], "-finline-max=", 13) == 0) {
            g_inline_call_limit = atoi(argv[i] + 13);
        }
        else if (strncmp(argv[i], "-finline-exclude=", 17) == 0) {
            safe_str_copy(g_inline_exclude_name, argv[i] + 17, sizeof(g_inline_exclude_name));
        }
        else if (strncmp(argv[i], "-fmax_passes=", 13) == 0) {
            max_passes = atoi(argv[i] + 13);
        // Positional Output File Fallback:
        }
        else if (out_idx == 0 && argv[i][0] != '-') {
            safe_str_copy(outFile, argv[i], sizeof(outFile));
            out_idx = i;
        }
		else
		{
			fprintf (stderr, "ERROR: '%s' unrecognized option\n", argv[i]);
		}
    }

    // Dynamically calculate actual enabled optimization count
    int opt_count = 0;
    if (config.enable_peephole)           opt_count++;
    if (config.enable_algebraic)          opt_count++;
    if (config.enable_forwarding)         opt_count++;
    if (config.enable_inline)             opt_count++;
    if (config.enable_dce)                opt_count++;
    if (config.enable_constant_folding)   opt_count++;
    if (config.enable_jump_next)          opt_count++;
    if (config.enable_redundant_movs)     opt_count++;
    if (config.enable_combine_immediates) opt_count++;
    if (config.enable_strength_reduction) opt_count++;
    if (config.enable_promote_regs)       opt_count++;
    if (config.enable_promote_leaf)       opt_count++;
    if (config.enable_promote_loops)      opt_count++;

    if (config.verbose) {
        if (opt_count == 0) {
            fprintf (stdout, "--- Configuration: NO OPTIMIZATIONS ENABLED ---\n");
        } else {
            fprintf (stdout, "--- Configuration: Enabled Optimizations (%d total) ---\n", opt_count);
        }

        if (config.enable_peephole)           fprintf(stdout, "  - peephole\n");
        if (config.enable_algebraic)          fprintf(stdout, "  - algebraic\n");
        if (config.enable_forwarding)         fprintf(stdout, "  - forwarding\n");
        if (config.enable_inline)             fprintf(stdout, "  - inline\n");
        if (config.enable_dce)                fprintf(stdout, "  - dce\n");
        if (config.enable_constant_folding)   fprintf(stdout, "  - constant_folding\n");
        if (config.enable_jump_next)          fprintf(stdout, "  - jump_next\n");
        if (config.enable_redundant_movs)     fprintf(stdout, "  - redundant_movs\n");
        if (config.enable_combine_immediates) fprintf(stdout, "  - combine_immediates\n");
        if (config.enable_strength_reduction) fprintf(stdout, "  - strength_reduction\n");
        if (config.enable_promote_regs)       fprintf(stdout, "  - promote_regs\n");
        if (config.enable_promote_leaf)       fprintf(stdout, "  - promote_leaf\n");
        if (config.enable_promote_loops)      fprintf(stdout, "  - promote_loops\n");
    }

    if (strlen(outFile) == 0) {
        safe_str_copy(outFile, inFile, sizeof(outFile));
        char *ext = strrchr(outFile, '.');
        if (ext && strcmp(ext, ".asm") == 0) {
            *ext = '\0';
        }
        snprintf(outFile + strlen(outFile), sizeof(outFile) - strlen(outFile), "Opt.asm");
    }

    AsmNode *program_ast = parse_vircon32_asm(inFile);

    int passes = 0;
    int total_opts = 0;
    int opts_in_pass = 0;

    if (config.verbose) printf("--- Starting Optimization Phase 1: Local Passes ---\n");

    // --- PHASE 1: Iterative Local & AST Optimizations ---
    int pass_count = 0;
    do {
        opts_in_pass = 0;
        int p_opts = 0, a_opts = 0, f_opts = 0, i_opts = 0, d_opts = 0;
        int j_opts = 0, m_opts = 0, c_opts = 0, s_opts = 0, r_opts = 0;
        int pl_opts = 0, lp_opts = 0;
        pass_count++;

        // 1. Interprocedural & Structural (exposes new code to the current iteration)
        if (config.enable_inline)             i_opts = pass_inline_trivial_functions(program_ast);

        // 2. Memory-to-Register Promotion (converts stack/memory to registers)
        if (config.enable_promote_regs)       r_opts = pass_promote_stack_slots(program_ast);
        if (config.enable_promote_leaf)       pl_opts = pass_promote_stack_slots(program_ast);
        if (config.enable_promote_loops)      lp_opts = pass_promote_loop_registers(program_ast);

        // 3. Data-Flow Forwarding & Immediate Folding
        if (config.enable_forwarding)         f_opts = pass_store_to_load_forwarding(program_ast);
        if (config.enable_combine_immediates) c_opts = pass_combine_immediates(program_ast);
        if (config.enable_strength_reduction) s_opts = pass_strength_reduction(program_ast);

        // 4. Algebraic & Local Instruction Cleanup
        if (config.enable_algebraic)          a_opts = pass_algebraic_simplifications(program_ast);
        if (config.enable_peephole)           p_opts = pass_peephole_window2(program_ast);
        if (config.enable_redundant_movs)     m_opts = pass_redundant_movs(program_ast);

        // 5. Control Flow & Dead Code Cleanup (removes dead bodies before next loop)
        if (config.enable_jump_next)          j_opts = pass_redundant_jumps(program_ast);
        if (config.enable_dce)                d_opts = pass_dead_function_elimination(program_ast);

        opts_in_pass = p_opts + a_opts + f_opts + j_opts + m_opts + c_opts + s_opts + r_opts + pl_opts + lp_opts + i_opts + d_opts;

        total_opts += opts_in_pass;
        passes++;

        if (config.verbose && opts_in_pass > 0) {
            printf("Pass %d applied %d optimizations:\n", passes, opts_in_pass);
            if (p_opts  > 0) printf ("  - Peephole: %d\n", p_opts);
            if (a_opts  > 0) printf ("  - Algebraic: %d\n", a_opts);
            if (f_opts  > 0) printf ("  - Forwarding: %d\n", f_opts);
            if (j_opts  > 0) printf ("  - Redundant jumps removed: %d\n", j_opts);
            if (m_opts  > 0) printf ("  - Redundant moves removed: %d\n", m_opts);
            if (c_opts  > 0) printf ("  - Immediates combined: %d\n", c_opts);
            if (s_opts  > 0) printf ("  - Strength reductions: %d\n", s_opts);
            if (i_opts  > 0) printf ("  - Inlined funcs: %d\n", i_opts);
            if (d_opts  > 0) printf ("  - Dead funcs removed: %d\n", d_opts);
            if (pl_opts > 0) printf ("  - Leaf stack slots promoted: %d\n", pl_opts);
            if (lp_opts > 0) printf ("  - Loop stack slots promoted: %d\n", lp_opts);
            if (r_opts  > 0) printf ("  - Stack slots promoted to regs: %d\n", r_opts);
        }

    } while (opts_in_pass > 0 && pass_count < max_passes);

    // --- PHASE 2: Global Data-Flow & CFG Optimizations ---
    int global_folds = 0;
    ControlFlowGraph *cfg = NULL;

    if (config.enable_constant_folding) {
        if (config.verbose) printf("--- Starting Optimization Phase 2: Global Data-Flow ---\n");
        cfg = build_cfg(program_ast);
        propagate_constants_cfg(cfg);

        global_folds = fold_constants_cfg(cfg);
        total_opts += global_folds;

        if (config.verbose && global_folds > 0) {
            printf("  - Global constant folds: %d\n", global_folds);
        }
    }

    // --- PHASE 3: Lightweight Post-CFG Cleanup ---
    // Sweeps away algebraic identities, dead moves, and redundant jumps exposed by Phase 2.
    int cleanup_opts;
    do {
        cleanup_opts = 0;
        int c_opts = 0, s_opts = 0, a_opts = 0, p_opts = 0, m_opts = 0, j_opts = 0;

        if (config.enable_combine_immediates) c_opts = pass_combine_immediates(program_ast);
        if (config.enable_strength_reduction) s_opts = pass_strength_reduction(program_ast);
        if (config.enable_algebraic)          a_opts = pass_algebraic_simplifications(program_ast);
        if (config.enable_peephole)           p_opts = pass_peephole_window2(program_ast);
        if (config.enable_redundant_movs)     m_opts = pass_redundant_movs(program_ast);
        if (config.enable_jump_next)          j_opts = pass_redundant_jumps(program_ast);

        cleanup_opts  = c_opts + s_opts + a_opts + p_opts + m_opts + j_opts;
        total_opts += cleanup_opts;

        if (config.verbose && opts_in_pass > 0) {
            printf("Cleanup Pass applied %d follow-up optimizations:\n", cleanup_opts);
            if (p_opts  > 0) printf ("  - Peephole: %d\n", p_opts);
            if (a_opts  > 0) printf ("  - Algebraic: %d\n", a_opts);
            if (j_opts  > 0) printf ("  - Redundant jumps removed: %d\n", j_opts);
            if (m_opts  > 0) printf ("  - Redundant moves removed: %d\n", m_opts);
            if (c_opts  > 0) printf ("  - Immediates combined: %d\n", c_opts);
            if (s_opts  > 0) printf ("  - Strength reductions: %d\n", s_opts);
        }
    } while (cleanup_opts > 0);

    if (strlen(dotFile) > 0) {
        if (!cfg) cfg = build_cfg(program_ast); 
        export_cfg_to_dot(dotFile, cfg);
        if (config.verbose) printf("CFG exported to '%s'.\n", dotFile);
    }

    if (config.verbose) {
        printf("\nOptimization complete: %d total optimizations applied.\n", total_opts);
    }

    write_vircon32_asm(outFile, program_ast);

    if (cfg) free_cfg(cfg);
    free(program_ast);
    return 0;
}
