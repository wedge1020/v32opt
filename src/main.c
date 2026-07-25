#include "v32opt.h"

int   g_inline_call_limit          = -1;
int   g_inline_calls_so_far        = 0;
char  g_inline_exclude_name[1024]  = {0};

// -------------------------------------------------------------------
// Main Entry Point
// -------------------------------------------------------------------
int  main (int  argc, char **argv)
{
    if (argc <  2)
	{
        fprintf(stdout, "Usage: %s <input.asm> [-o output.asm] [options]\n",
                argv[0]);
        fprintf(stdout, "Options:\n");
        fprintf(stdout, "  -v               Verbose output (show pass statistics)\n");
        fprintf(stdout, "  --dot <cfg.dot>  Export Control Flow Graph to DOT format\n");
        fprintf(stdout, "  -o <file>        Specify output assembly file name\n");
        fprintf(stdout, "  -O0              Disable all optimizations [default]\n");
        fprintf(stdout, "  -O1              Enables first level of optimizations:\n");
        fprintf(stdout, "                       peephole_pairs, peephole_algebra,\n");
		fprintf(stdout, "                       peephole_forwarding, peephole_jumps,\n");
		fprintf(stdout, "                       peephole_movs, peephole_immediates,\n");
		fprintf(stdout, "                       peephole_reduce, peephole_shifts,\n");
		fprintf(stdout, "                       peephole_dead_stores, peephole_loads,\n");
		fprintf(stdout, "                       peephole_immediate_prop, peephole_jump_chain\n");
        fprintf(stdout, "  -O2              Enables second level (could break):\n");
        fprintf(stdout, "                       ALL included in -O1,\n");
        fprintf(stdout, "                       dce, constant_folding,\n");
        fprintf(stdout, "                       omit_frame_pointers\n");
        fprintf(stdout, "  -O3              Enables aggressive optimizations (could break):\n");
        fprintf(stdout, "                       ALL included in -O1 and -O2,\n");
        fprintf(stdout, "                       inline\n\n");
        fprintf(stdout, "Individual Optimization Toggles:\n");
        fprintf(stdout, "  -fopt_<name>     Enable specific pass (e.g., -fopt_promote_regs)\n");
        fprintf(stdout, "  -fno_opt_<name>  Disable specific pass (e.g., -fno_opt_inline)\n\n");
        fprintf(stdout, "Available optimization names:\n");
        fprintf(stdout, "  peephole_pairs, peephole_algebra, peephole_forwarding,\n");
		fprintf(stdout, "  peephole_jumps, peephole_movs, peephole_immediates,\n");
		fprintf(stdout, "  peephole_reduce, peephole_shifts, peephole_dead_stores,\n");
		fprintf(stdout, "  peephole_loads, peephole_immediate_prop, peephole_jump_chain,\n");
        fprintf(stdout, "  inline, dce, constant_folding, promote_regs, promote_leaf,\n");
        fprintf(stdout, "  promote_loops, omit_frame_pointers\n\n");
        fprintf(stdout, "Diagnostic Flags:\n");
        fprintf(stdout, "  -finline-max=N   Cap the number of inlined CALL sites to N\n");
        fprintf(stdout, "  -fmax_passes=N   Cap the maximum iterative optimization passes to N\n\n");
        fprintf(stdout, "NOTE: promote_regs, promote_leaf, and promote_loops not yet\n");
        fprintf(stdout, "connected to any optimization category. Test and bugfix first\n\n");
        return (1);
    }

    char *inFile = argv[1];
    char outFile[256] = {0};
    char dotFile[256] = {0};
    int  max_passes   = 1000;

    // -------------------------------------------------------------------
    // Optimization Configuration
    // -------------------------------------------------------------------
    OptConfig config = {
        .verbose = false,
        .enable_peephole_pairs           = false,
        .enable_peephole_algebra         = false,
        .enable_peephole_forwarding      = false,
		.enable_peephole_jumps           = false,
		.enable_peephole_movs            = false,
		.enable_peephole_immediates      = false,
		.enable_peephole_reduce          = false,
		.enable_peephole_shifts          = false,
		.enable_peephole_dead_stores     = false,
		.enable_peephole_loads           = false,
		.enable_peephole_immediate_prop  = false,
		.enable_peephole_jmp_chain       = false,
        .enable_dce                      = false,
        .enable_constant_folding         = false,
        .enable_inline                   = false,
        .enable_promote_regs             = false,
        .enable_promote_leaf             = false,
        .enable_promote_loops            = false,
        .enable_omit_frame_pointers      = false
    };

    // -------------------------------------------------------------------
    // Parse Command Line Arguments
    // -------------------------------------------------------------------
    int out_idx = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            safe_str_copy(outFile, argv[i+1], sizeof(outFile));
            out_idx = i;
            i++;
        } else if (strcmp(argv[i], "--dot") == 0 && i + 1 < argc) {
            safe_str_copy(dotFile, argv[i+1], sizeof(dotFile));
            i++;
        } else if (strcmp(argv[i], "-v") == 0) {
            config.verbose = true;
        } else if (strcmp(argv[i], "-O0") == 0) {
			config.enable_peephole_pairs           = false;
			config.enable_peephole_algebra         = false;
			config.enable_peephole_forwarding      = false;
			config.enable_peephole_jumps           = false;
			config.enable_peephole_movs            = false;
			config.enable_peephole_immediates      = false;
			config.enable_peephole_reduce          = false;
			config.enable_peephole_shifts          = false;
			config.enable_peephole_dead_stores     = false;
			config.enable_peephole_loads           = false;
			config.enable_peephole_immediate_prop  = false;
			config.enable_peephole_jmp_chain       = false;
			config.enable_dce                      = false;
			config.enable_constant_folding         = false;
			config.enable_inline                   = false;
			config.enable_promote_regs             = false;
			config.enable_promote_leaf             = false;
			config.enable_promote_loops            = false;
			config.enable_omit_frame_pointers      = false;
        } else if (strcmp(argv[i], "-O1") == 0) {
			config.enable_peephole_pairs           = true;
			config.enable_peephole_algebra         = true;
			config.enable_peephole_forwarding      = true;
			config.enable_peephole_jumps           = true;
			config.enable_peephole_movs            = true;
			config.enable_peephole_immediates      = true;
			config.enable_peephole_reduce          = true;
			config.enable_peephole_shifts          = true;
			config.enable_peephole_dead_stores     = true;
			config.enable_peephole_loads           = true;
			config.enable_peephole_immediate_prop  = true;
			config.enable_peephole_jmp_chain       = true;
			config.enable_dce                      = false;
			config.enable_constant_folding         = false;
			config.enable_inline                   = false;
			config.enable_promote_regs             = false;
			config.enable_promote_leaf             = false;
			config.enable_promote_loops            = false;
			config.enable_omit_frame_pointers      = false;
        } else if (strcmp(argv[i], "-O2") == 0) {
			config.enable_peephole_pairs           = true;
			config.enable_peephole_algebra         = true;
			config.enable_peephole_forwarding      = true;
			config.enable_peephole_jumps           = true;
			config.enable_peephole_movs            = true;
			config.enable_peephole_immediates      = true;
			config.enable_peephole_reduce          = true;
			config.enable_peephole_shifts          = true;
			config.enable_peephole_dead_stores     = true;
			config.enable_peephole_loads           = true;
			config.enable_peephole_immediate_prop  = true;
			config.enable_peephole_jmp_chain       = true;
			config.enable_dce                      = true;
			config.enable_constant_folding         = true;
			config.enable_inline                   = false;
			config.enable_promote_regs             = false;
			config.enable_promote_leaf             = false;
			config.enable_promote_loops            = false;
			config.enable_omit_frame_pointers      = true;
        } else if (strcmp(argv[i], "-O3") == 0) {
			config.enable_peephole_pairs           = true;
			config.enable_peephole_algebra         = true;
			config.enable_peephole_forwarding      = true;
			config.enable_peephole_jumps           = true;
			config.enable_peephole_movs            = true;
			config.enable_peephole_immediates      = true;
			config.enable_peephole_reduce          = true;
			config.enable_peephole_shifts          = true;
			config.enable_peephole_dead_stores     = true;
			config.enable_peephole_loads           = true;
			config.enable_peephole_immediate_prop  = true;
			config.enable_peephole_jmp_chain       = true;
			config.enable_dce                      = true;
			config.enable_constant_folding         = true;
			config.enable_inline                   = true;
			config.enable_promote_regs             = false;
			config.enable_promote_leaf             = false;
			config.enable_promote_loops            = false;
			config.enable_omit_frame_pointers      = true;
        } else if (strcmp(argv[i], "-fopt_peephole_pairs")      == 0) {
            config.enable_peephole_pairs           = true;
        } else if (strcmp(argv[i], "-fopt_peephole_algebra")    == 0) {
            config.enable_peephole_algebra         = true;
        } else if (strcmp(argv[i], "-fopt_peephole_forwarding") == 0) {
            config.enable_peephole_forwarding      = true;
        } else if (strcmp(argv[i], "-fopt_peephole_jumps") == 0) {
            config.enable_peephole_jumps           = true;
        } else if (strcmp(argv[i], "-fopt_peephole_movs") == 0) {
            config.enable_peephole_movs            = true;
        } else if (strcmp(argv[i], "-fopt_peephole_immediates") == 0) {
            config.enable_peephole_immediates      = true;
        } else if (strcmp(argv[i], "-fopt_peephole_reduce") == 0) {
            config.enable_peephole_reduce          = true;
        } else if (strcmp(argv[i], "-fopt_peephole_shifts") == 0) {
            config.enable_peephole_shifts          = true;
        } else if (strcmp(argv[i], "-fopt_peephole_dead_stores") == 0) {
            config.enable_peephole_dead_stores     = true;
        } else if (strcmp(argv[i], "-fopt_peephole_loads") == 0) {
            config.enable_peephole_loads           = true;
        } else if (strcmp(argv[i], "-fopt_peephole_immediate_prop") == 0) {
            config.enable_peephole_immediate_prop  = true;
        } else if (strcmp(argv[i], "-fopt_peephole_jmp_chain") == 0) {
            config.enable_peephole_jmp_chain      = true;
        } else if (strcmp(argv[i], "-fopt_dce") == 0) {
            config.enable_dce = true;
        } else if (strcmp(argv[i], "-fopt_constant_folding") == 0) {
            config.enable_constant_folding = true;
        } else if (strcmp(argv[i], "-fopt_omit_frame_pointers") == 0) {
			config.enable_omit_frame_pointers      = true;
        } else if (strcmp(argv[i], "-fopt_inline") == 0) {
            config.enable_inline = true;
        } else if (strcmp(argv[i], "-fopt_promote_regs") == 0) {
            config.enable_promote_regs             = true;
        } else if (strcmp(argv[i], "-fopt_promote_leaf") == 0) {
            config.enable_promote_leaf             = true;
        } else if (strcmp(argv[i], "-fopt_promote_loops") == 0) {
            config.enable_promote_loops            = true;
        } else if (strcmp(argv[i], "-fno_opt_peephole_jumps") == 0) {
            config.enable_peephole_jumps = false;
        } else if (strcmp(argv[i], "-fno_opt_peephole_movs") == 0) {
            config.enable_peephole_movs = false;
        } else if (strcmp(argv[i], "-fno_opt_peephole_immediates") == 0) {
            config.enable_peephole_immediates = false;
        } else if (strcmp(argv[i], "-fno_opt_peephole_reduce") == 0) {
            config.enable_peephole_reduce = false;
        } else if (strcmp(argv[i], "-fno_opt_peephole_pairs") == 0) {
            config.enable_peephole_pairs = false;
        } else if (strcmp(argv[i], "-fno_opt_peephole_algebra") == 0) {
            config.enable_peephole_algebra = false;
        } else if (strcmp(argv[i], "-fno_opt_peephole_forwarding") == 0) {
            config.enable_peephole_forwarding = false;
        } else if (strcmp(argv[i], "-fno_opt_peephole_jumps") == 0) {
            config.enable_peephole_jumps = false;
        } else if (strcmp(argv[i], "-fno_opt_peephole_movs") == 0) {
            config.enable_peephole_movs = false;
        } else if (strcmp(argv[i], "-fno_opt_dce") == 0) {
            config.enable_dce = false;
        } else if (strcmp(argv[i], "-fno_opt_constant_folding") == 0) {
            config.enable_constant_folding = false;
        } else if (strcmp(argv[i], "-fno_opt_omit_frame_pointers") == 0) {
			config.enable_omit_frame_pointers      = false;
        } else if (strcmp(argv[i], "-fno_opt_inline") == 0) {
            config.enable_inline = false;
        } else if (strcmp(argv[i], "-fno_opt_promote_regs") == 0) {
            config.enable_promote_regs = false;
        } else if (strcmp(argv[i], "-fno_opt_promote_leaf") == 0) {
            config.enable_promote_leaf = false;
        } else if (strcmp(argv[i], "-fno_opt_promote_loops") == 0) {
            config.enable_promote_loops = false;
        } else if (strncmp(argv[i], "-finline-max=", 13) == 0) {
            g_inline_call_limit = atoi(argv[i] + 13);
        } else if (strncmp(argv[i], "-finline-exclude=", 17) == 0) {
            safe_str_copy(g_inline_exclude_name, argv[i] + 17,
                          sizeof(g_inline_exclude_name));
        } else if (strncmp(argv[i], "-fmax_passes=", 13) == 0) {
            max_passes = atoi(argv[i] + 13);
        } else if (out_idx == 0 && argv[i][0] != '-') {
            safe_str_copy(outFile, argv[i], sizeof(outFile));
            out_idx = i;
        } else {
            fprintf(stderr, "ERROR: '%s' unrecognized option\n", argv[i]);
        }
    }

    // -------------------------------------------------------------------
    // Tally up Optimizations and Display Configuration
    // -------------------------------------------------------------------

    if (config.verbose)
	{
		fprintf(stdout, "--- Configuration: Enabled Optimizations ---\n");
	}

    int opt_count = 0;
    if (config.enable_peephole_pairs)
	{
        if (config.verbose)
            fprintf(stdout, "  - peephole_pairs\n");
		opt_count++;
	}
    if (config.enable_peephole_algebra)
	{
        if (config.verbose)
            fprintf(stdout, "  - peephole_algebra\n");
		opt_count++;
	}
    if (config.enable_peephole_forwarding)
	{
        if (config.verbose)
            fprintf(stdout, "  - peephole_forwarding\n");
		opt_count++;
	}
    if (config.enable_peephole_jumps)
	{
        if (config.verbose)
            fprintf(stdout, "  - peephole_jumps\n");
		opt_count++;
	}
    if (config.enable_peephole_movs)
	{
        if (config.verbose)
            fprintf(stdout, "  - peephole_movs\n");
		opt_count++;
	}
    if (config.enable_peephole_immediates)
	{
        if (config.verbose)
            fprintf(stdout, "  - peephole_immediates\n");
		opt_count++;
	}
    if (config.enable_peephole_reduce)
	{
        if (config.verbose)
            fprintf(stdout, "  - peephole_reduce\n");
		opt_count++;
	}
    if (config.enable_peephole_shifts)
	{
        if (config.verbose)
            fprintf(stdout, "  - peephole_shifts\n");
		opt_count++;
	}
    if (config.enable_peephole_dead_stores)
	{
        if (config.verbose)
            fprintf(stdout, "  - peephole_dead_stores\n");
		opt_count++;
	}
    if (config.enable_peephole_loads)
	{
        if (config.verbose)
            fprintf(stdout, "  - peephole_loads\n");
		opt_count++;
	}
    if (config.enable_peephole_immediate_prop)
	{
        if (config.verbose)
            fprintf(stdout, "  - peephole_immediate_prop\n");
		opt_count++;
	}
    if (config.enable_peephole_jmp_chain)
	{
        if (config.verbose)
            fprintf(stdout, "  - peephole_jmp_chain\n");
		opt_count++;
	}
    if (config.enable_dce)
	{
        if (config.verbose)
            fprintf(stdout, "  - dce\n");
		opt_count++;
	}
    if (config.enable_constant_folding)
	{
        if (config.verbose)
            fprintf(stdout, "  - constant_folding\n");
		opt_count++;
	}
    if (config.enable_omit_frame_pointers)
	{
        if (config.verbose)
            fprintf(stdout, "  - omit_frame_pointers\n");
		opt_count++;
	}
    if (config.enable_inline)
	{
        if (config.verbose)
            fprintf(stdout, "  - inline\n");
		opt_count++;
	}
    if (config.enable_promote_regs)
	{
        if (config.verbose)
            fprintf(stdout, "  - promote_regs\n");
		opt_count++;
	}
    if (config.enable_promote_leaf)
	{
        if (config.verbose)
            fprintf(stdout, "  - promote_leaf\n");
		opt_count++;
	}
    if (config.enable_promote_loops)
	{
        if (config.verbose)
            fprintf(stdout, "  - promote_loops\n");
		opt_count++;
	}

    if (config.verbose)
	{
		fprintf (stdout, "    (%d total enabled)\n", opt_count);
	}

    // -------------------------------------------------------------------
    // Set Output File
    // -------------------------------------------------------------------
    if (strlen(outFile) == 0) {
        safe_str_copy(outFile, inFile, sizeof(outFile));
        char *ext = strrchr(outFile, '.');
        if (ext && strcmp(ext, ".asm") == 0) {
            *ext = '\0';
        }
        snprintf(outFile + strlen(outFile),
                sizeof(outFile) - strlen(outFile), "Opt.asm");
    }

    // -------------------------------------------------------------------
    // Parse Input Assembly File
    // -------------------------------------------------------------------
    AsmNode *program_ast = parse_vircon32_asm(inFile);

    int passes = 0;
    int total_opts = 0;
    int opts_in_pass = 0;

    if (config.verbose)
        printf("--- Starting Optimization Phase 1: Local Passes ---\n");

    // -------------------------------------------------------------------
    // PHASE 1: Iterative Local & AST Optimizations
    // -------------------------------------------------------------------
    // This phase applies local optimizations iteratively until no more
    // optimizations can be applied or the maximum pass count is reached.
    // The order of passes is designed to expose new optimization
    // opportunities with each iteration.
    // -------------------------------------------------------------------
    int pass_count = 0;
    do {
        opts_in_pass = 0;
        int p_opts = 0, a_opts = 0, f_opts = 0, i_opts = 0, d_opts = 0;
        int j_opts = 0, m_opts = 0, c_opts = 0, s_opts = 0, r_opts = 0;
        int pl_opts = 0, lp_opts = 0;
        int ds_opts = 0, ld_opts = 0, ip_opts = 0, jc_opts = 0, sh_opts = 0;
		int ofp_opts = 0;
        pass_count++;

        // ----------------------------------------------------------------
        // 1. Interprocedural & Structural (exposes new code to the iteration)
        // ----------------------------------------------------------------
        // Inlining functions can expose new optimization opportunities
        // by replacing function calls with their bodies
        if (config.enable_inline) {
            i_opts = inline_trivial_functions(program_ast);
        }

        // ----------------------------------------------------------------
        // 2. Memory-to-Register Promotion
        // ----------------------------------------------------------------
        // These passes convert stack/memory accesses to register operations
        // for better performance and to enable further optimizations.
        if (config.enable_promote_regs) {
            r_opts = pass_promote_stack_slots(program_ast);
        }
        if (config.enable_promote_leaf) {
            pl_opts = pass_promote_stack_slots(program_ast);
        }
        if (config.enable_promote_loops) {
            lp_opts = pass_promote_loop_registers(program_ast);
        }

        // ----------------------------------------------------------------
        // 3. Data-Flow Forwarding & Immediate Folding
        // ----------------------------------------------------------------
        // Store-to-load forwarding eliminates redundant memory loads
        // by forwarding values directly from stores to loads
        if (config.enable_peephole_forwarding) {
            f_opts = peephole_forwarding(program_ast);
        }
        // Combine consecutive arithmetic operations with immediate operands
        if (config.enable_peephole_immediates) {
            c_opts = peephole_immediates(program_ast);
        }
        // Replace expensive operations with cheaper equivalents
        if (config.enable_peephole_reduce) {
            s_opts = peephole_reduce(program_ast);
        }

        // ----------------------------------------------------------------
        // 4. Algebraic & Local Instruction Cleanup
        // ----------------------------------------------------------------
        // Algebraic simplifications remove or replace instructions that
        // are algebraically redundant (e.g., MOV r, r)
        if (config.enable_peephole_algebra) {
            a_opts = peephole_algebra(program_ast);
        }
        // Peephole optimizations on 2-instruction windows
        if (config.enable_peephole_pairs) {
            p_opts = peephole_pairs(program_ast);
        }
        // Remove redundant MOV instructions
        if (config.enable_peephole_movs) {
            m_opts = peephole_movs(program_ast);
        }

        // ----------------------------------------------------------------
        // 5. Additional Peephole Optimizations
        // ----------------------------------------------------------------
        // Dead store elimination: remove stores that are immediately overwritten
        if (config.enable_peephole_dead_stores) {
            ds_opts = peephole_dead_stores(program_ast);
        }
        // Redundant load elimination: replace loads with register values
        if (config.enable_peephole_loads) {
            ld_opts = peephole_loads(program_ast);
        }
        // Immediate propagation: propagate immediate values through MOVs
        if (config.enable_peephole_immediate_prop) {
            ip_opts = peephole_immediate_prop(program_ast);
        }
        // Jump chain elimination: chain consecutive jumps
        if (config.enable_peephole_jmp_chain) {
            jc_opts = peephole_jmp_chain(program_ast);
        }
        // Shift optimizations: optimize shift operations
        if (config.enable_peephole_shifts) {
            sh_opts = peephole_shifts(program_ast);
        }

        // ----------------------------------------------------------------
        // 6. Control Flow & Dead Code Cleanup
        // ----------------------------------------------------------------
        // Remove redundant jumps and dead code to clean up control flow
        if (config.enable_peephole_jumps) {
            j_opts = peephole_jumps(program_ast);
        }
        if (config.enable_dce) {
            d_opts = pass_dead_function_elimination(program_ast);
        }

        // omit frame pointers
        if (config.enable_omit_frame_pointers) {
            ofp_opts = omit_frame_pointers(program_ast);
        }

        // Calculate total optimizations for this pass
        opts_in_pass = p_opts + a_opts + f_opts + j_opts + m_opts + c_opts +
                       s_opts + r_opts + pl_opts + lp_opts + i_opts + d_opts +
                       ds_opts + ld_opts + ip_opts + jc_opts + sh_opts + ofp_opts;

        total_opts += opts_in_pass;
        passes++;

        // Verbose output for this pass
        if (config.verbose && opts_in_pass > 0) {
            printf("Pass %d applied %d optimizations:\n", passes, opts_in_pass);
            if (p_opts  > 0)
                printf("  - peephole_pairs:          %d\n", p_opts);
            if (a_opts  > 0)
                printf("  - peephole_algebra:        %d\n", a_opts);
            if (f_opts  > 0)
                printf("  - peephole_forwarding:     %d\n", f_opts);
            if (j_opts  > 0)
                printf("  - peephole_jumps:          %d\n", j_opts);
            if (m_opts  > 0)
                printf("  - peephole_movs:           %d\n", m_opts);
            if (c_opts  > 0)
                printf("  - peephole_immediates:     %d\n", c_opts);
            if (s_opts  > 0)
                printf("  - peephole_reduce:         %d\n", s_opts);
            if (ds_opts > 0)
                printf("  - peephole_dead_stores:    %d\n", ds_opts);
            if (ld_opts > 0)
                printf("  - peephole_loads loads:    %d\n", ld_opts);
            if (ip_opts > 0)
                printf("  - peephole_immediate_prop: %d\n", ip_opts);
            if (jc_opts > 0)
                printf("  - peephole_jump_chain:     %d\n", jc_opts);
            if (sh_opts > 0)
                printf("  - peephole_shifts:         %d\n", sh_opts);
            if (d_opts  > 0)
                printf("  - dce:                     %d\n", d_opts);
            if (i_opts  > 0)
                printf("  - inline:                  %d\n", i_opts);
            if (pl_opts > 0)
                printf("  - promote_leaf:            %d\n", pl_opts);
            if (lp_opts > 0)
                printf("  - promote_loops:           %d\n", lp_opts);
            if (r_opts  > 0)
                printf("  - promote_regs:            %d\n", r_opts);
            if (ofp_opts  > 0)
                printf("  - omit_frame_pointers:     %d\n", ofp_opts);
        }

    } while (opts_in_pass > 0 && pass_count < max_passes);

    // -------------------------------------------------------------------
    // PHASE 2: Global Data-Flow & CFG Optimizations
    // -------------------------------------------------------------------
    // This phase builds a Control Flow Graph and performs global
    // data-flow analysis for constant propagation and folding.
    // -------------------------------------------------------------------
    int global_folds = 0;
    ControlFlowGraph *cfg = NULL;

    if (config.enable_constant_folding) {
        if (config.verbose)
            printf("--- Starting Optimization Phase 2: Global Data-Flow ---\n");
        cfg = build_cfg(program_ast);
        propagate_constants_cfg(cfg);

        global_folds = fold_constants_cfg(cfg);
        total_opts += global_folds;

        if (config.verbose && global_folds > 0) {
            printf("  - Global constant folds: %d\n", global_folds);
        }
    }

    // -------------------------------------------------------------------
    // PHASE 3: Lightweight Post-CFG Cleanup
    // -------------------------------------------------------------------
    // After global optimizations, perform a final cleanup pass to remove
    // any newly exposed redundant instructions or control flow.
    // -------------------------------------------------------------------
    int cleanup_opts;
    do {
        cleanup_opts = 0;
        int c_opts = 0, s_opts = 0, a_opts = 0, p_opts = 0, m_opts = 0, j_opts = 0;
        int ds_opts = 0, ld_opts = 0, ip_opts = 0, jc_opts = 0, sh_opts = 0, ofp_opts = 0;

        if (config.enable_peephole_immediates)
            c_opts = peephole_immediates(program_ast);
        if (config.enable_peephole_reduce)
            s_opts = peephole_reduce(program_ast);
        if (config.enable_peephole_algebra)
            a_opts = peephole_algebra(program_ast);
        if (config.enable_peephole_pairs)
            p_opts = peephole_pairs(program_ast);
        if (config.enable_peephole_movs)
            m_opts = peephole_movs(program_ast);
        if (config.enable_peephole_jumps)
            j_opts = peephole_jumps(program_ast);
        if (config.enable_peephole_dead_stores)
            ds_opts = peephole_dead_stores(program_ast);
        if (config.enable_peephole_loads)
            ld_opts = peephole_loads(program_ast);
        if (config.enable_peephole_immediate_prop)
            ip_opts = peephole_immediate_prop(program_ast);
        if (config.enable_peephole_jmp_chain)
            jc_opts = peephole_jmp_chain(program_ast);
        if (config.enable_peephole_shifts)
            sh_opts = peephole_shifts(program_ast);
        if (config.enable_omit_frame_pointers)
            sh_opts = omit_frame_pointers(program_ast);

        cleanup_opts = c_opts + s_opts + a_opts + p_opts + m_opts + j_opts +
                       ds_opts + ld_opts + ip_opts + jc_opts + sh_opts + ofp_opts;
        total_opts += cleanup_opts;

        if (config.verbose && cleanup_opts > 0) {
            printf("Cleanup Pass applied %d follow-up optimizations:\n",
                   cleanup_opts);
            if (p_opts  > 0)
                printf("  - peephole_pairs:          %d\n", p_opts);
            if (a_opts  > 0)
                printf("  - peephole_algebra:        %d\n", a_opts);
            if (j_opts  > 0)
                printf("  - peephole_jumps:          %d\n", j_opts);
            if (m_opts  > 0)
                printf("  - peephole_movs:           %d\n", m_opts);
            if (c_opts  > 0)
                printf("  - peephole_immediates:     %d\n", c_opts);
            if (s_opts  > 0)
                printf("  - peephole_reduce:         %d\n", s_opts);
            if (ds_opts > 0)
                printf("  - peephole_dead_stores:    %d\n", ds_opts);
            if (ld_opts > 0)
                printf("  - peephole_loads:          %d\n", ld_opts);
            if (ip_opts > 0)
                printf("  - peephole_immediate_prop: %d\n", ip_opts);
            if (jc_opts > 0)
                printf("  - peephole_jmp_chain:      %d\n", jc_opts);
            if (sh_opts > 0)
                printf("  - peephole_shifts:         %d\n", sh_opts);
            if (ofp_opts > 0)
                printf("  - omit_frame_pointers:     %d\n", ofp_opts);
        }
    } while (cleanup_opts > 0);

    // -------------------------------------------------------------------
    // Export CFG if requested
    // -------------------------------------------------------------------
    if (strlen(dotFile) > 0) {
        if (!cfg) cfg = build_cfg(program_ast);
        export_cfg_to_dot(dotFile, cfg);
        if (config.verbose)
            printf("CFG exported to '%s'.\n", dotFile);
    }

    // -------------------------------------------------------------------
    // Final Statistics
    // -------------------------------------------------------------------
    if (config.verbose) {
        printf("\nOptimization complete: %d total optimizations applied.\n",
               total_opts);
    }

    // -------------------------------------------------------------------
    // Write Output File
    // -------------------------------------------------------------------
    write_vircon32_asm(outFile, program_ast);

    // -------------------------------------------------------------------
    // Cleanup
    // -------------------------------------------------------------------
    if (cfg) free_cfg(cfg);
    free(program_ast);
    return 0;
}
