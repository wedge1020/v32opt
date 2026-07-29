#include "v32opt.h"

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
	.opt_peephole_pairs           = false,
	.opt_peephole_algebra         = false,
	.opt_peephole_forwarding      = false,
	.opt_peephole_jumps           = false,
	.opt_peephole_movs            = false,
	.opt_peephole_immediates      = false,
	.opt_peephole_reduce          = false,
	.opt_peephole_shifts          = false,
	.opt_peephole_dead_stores     = false,
	.opt_peephole_loads           = false,
	.opt_peephole_immediate_prop  = false,
	.opt_peephole_jmp_chain       = false,
	.opt_cse                      = false,
	.opt_dce                      = false,
	.opt_constant_folding         = false,
	.opt_inline                   = false,
	.opt_inline_call_limit        = -1,
	.opt_inline_max_body_ins      = 8,
	.opt_promote_regs             = false,
	.opt_promote_leaf             = false,
	.opt_promote_loops            = false,
	.opt_omit_frame_pointers      = false
};

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
        fprintf(stdout, "  -d               In-code debugging (mark the hits)\n");
        fprintf(stdout, "  -t               Testing mode: just display opt:hits\n");
        fprintf(stdout, "  --dot <cfg.dot>  Export Control Flow Graph to DOT format\n");
        fprintf(stdout, "  -o <file>        Specify output assembly file name\n");
        fprintf(stdout, "  -O0              Disable all optimizations [default]\n");
        fprintf(stdout, "  -O1              Enables first level optimizations\n");
        fprintf(stdout, "  -O2              Enables second level optimizations\n");
        fprintf(stdout, "  -O3              Enables aggressive optimizations (could break):\n");
        fprintf(stdout, "Individual Optimization Toggles:\n");
        fprintf(stdout, "  -f<name>     Enable specific pass (e.g., -fpeephole-algebra)\n");
        fprintf(stdout, "  -fno-<name>  Disable specific pass (e.g., -fno-inline)\n\n");
        fprintf(stdout, "Available optimization names:\n");
        fprintf(stdout, "  peephole-pairs, peephole-algebra, peephole-forwarding,\n");
        fprintf(stdout, "  peephole-jumps, peephole-movs, peephole-immediates,\n");
        fprintf(stdout, "  peephole-reduce, peephole-shifts, peephole-dead-stores,\n");
        fprintf(stdout, "  peephole-loads, peephole-immediate-prop, peephole-jmp-chain,\n");
        fprintf(stdout, "  inline, cse, dce, constant-folding, promote-regs, promote-leaf,\n");
        fprintf(stdout, "  promote-loops, omit-frame-pointers\n\n");
        fprintf(stdout, "Diagnostic Flags:\n");
        fprintf(stdout, "  -finline-max=N   Cap the number of inlined CALL sites to N\n");
        fprintf(stdout, "  -fmax-passes=N   Cap the maximum iterative optimization passes to N\n\n");
        fprintf(stdout, "NOTE: peephole-dead-stores,\n");
        fprintf(stdout, "promote-regs, promote-leaf, and promote-loops not yet\n");
        fprintf(stdout, "connected to any optimization category. Test and bugfix first\n\n");
        return (1);
    }

    char *inFile = argv[1];
    char outFile[256] = {0};
    char dotFile[256] = {0};
    int  max_passes   = 1000;
    OptType tally[MAX_OPTIMIZATION_ALGORITHMS]  = { 0 };

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
        } else if (strcmp(argv[i], "-t") == 0) {
            config.testing = true;
        } else if (strcmp(argv[i], "-d") == 0) {
            config.debug   = true;
        } else if (strcmp(argv[i], "-O0") == 0) {
            config.opt_peephole_pairs           = false;
            config.opt_peephole_algebra         = false;
            config.opt_peephole_forwarding      = false;
            config.opt_peephole_jumps           = false;
            config.opt_peephole_movs            = false;
            config.opt_peephole_immediates      = false;
            config.opt_peephole_reduce          = false;
            config.opt_peephole_shifts          = false;
            config.opt_peephole_dead_stores     = false;
            config.opt_peephole_loads           = false;
            config.opt_peephole_immediate_prop  = false;
            config.opt_peephole_jmp_chain       = false;
            config.opt_cse                      = false;
            config.opt_dce                      = false;
            config.opt_constant_folding         = false;
            config.opt_inline                   = false;
            config.opt_promote_regs             = false;
            config.opt_promote_leaf             = false;
            config.opt_promote_loops            = false;
            config.opt_omit_frame_pointers      = false;
        } else if (strcmp(argv[i], "-O1") == 0) {
            config.opt_peephole_pairs           = true;
            config.opt_peephole_algebra         = true;
            config.opt_peephole_jumps           = true;
            config.opt_peephole_jmp_chain       = true;
            config.opt_peephole_forwarding      = false;
            config.opt_peephole_loads           = false;
            config.opt_peephole_movs            = false;
            config.opt_peephole_immediates      = true;
            config.opt_peephole_reduce          = true;
            config.opt_peephole_shifts          = true;
            config.opt_peephole_dead_stores     = false;
            config.opt_peephole_immediate_prop  = true;
            config.opt_cse                      = false;
            config.opt_dce                      = false;
            config.opt_constant_folding         = false;
            config.opt_inline                   = false;
            config.opt_promote_regs             = false;
            config.opt_promote_leaf             = false;
            config.opt_promote_loops            = false;
            config.opt_omit_frame_pointers      = false;
        } else if (strcmp(argv[i], "-O2") == 0) {
            config.opt_peephole_pairs           = true;
            config.opt_peephole_algebra         = true;
            config.opt_peephole_jumps           = true;
            config.opt_peephole_immediates      = true;
            config.opt_peephole_reduce          = true;
            config.opt_peephole_shifts          = true;
            config.opt_peephole_dead_stores     = false;
            config.opt_peephole_forwarding      = false;
            config.opt_peephole_loads           = false;
            config.opt_peephole_movs            = false;
            config.opt_peephole_immediate_prop  = true;
            config.opt_peephole_jmp_chain       = true;
            config.opt_cse                      = true;
            config.opt_dce                      = true;
            config.opt_constant_folding         = true;
            config.opt_inline                   = false;
            config.opt_promote_regs             = false;
            config.opt_promote_leaf             = false;
            config.opt_promote_loops            = false;
            config.opt_omit_frame_pointers      = true;
        } else if (strcmp(argv[i], "-O3") == 0) {
            config.opt_peephole_pairs           = true;
            config.opt_peephole_algebra         = true;
            config.opt_peephole_forwarding      = false;
            config.opt_peephole_jumps           = true;
            config.opt_peephole_movs            = false;
            config.opt_peephole_immediates      = true;
            config.opt_peephole_reduce          = true;
            config.opt_peephole_shifts          = true;
            config.opt_peephole_dead_stores     = false;
            config.opt_peephole_loads           = false;
            config.opt_peephole_immediate_prop  = true;
            config.opt_peephole_jmp_chain       = true;
            config.opt_cse                      = true;
            config.opt_dce                      = true;
            config.opt_constant_folding         = true;
            config.opt_inline                   = true;
            config.opt_promote_regs             = false;
            config.opt_promote_leaf             = false;
            config.opt_promote_loops            = false;
            config.opt_omit_frame_pointers      = true;
        } else if (strcmp(argv[i], "-fpeephole-pairs")      == 0) {
            config.opt_peephole_pairs           = true;
        } else if (strcmp(argv[i], "-fpeephole-algebra")    == 0) {
            config.opt_peephole_algebra         = true;
        } else if (strcmp(argv[i], "-fpeephole-forwarding") == 0) {
            config.opt_peephole_forwarding      = true;
        } else if (strcmp(argv[i], "-fpeephole-jumps") == 0) {
            config.opt_peephole_jumps           = true;
        } else if (strcmp(argv[i], "-fpeephole-movs") == 0) {
            config.opt_peephole_movs            = true;
        } else if (strcmp(argv[i], "-fpeephole-immediates") == 0) {
            config.opt_peephole_immediates      = true;
        } else if (strcmp(argv[i], "-fpeephole-reduce") == 0) {
            config.opt_peephole_reduce          = true;
        } else if (strcmp(argv[i], "-fpeephole-shifts") == 0) {
            config.opt_peephole_shifts          = true;
        } else if (strcmp(argv[i], "-fpeephole-dead-stores") == 0) {
            config.opt_peephole_dead_stores     = true;
        } else if (strcmp(argv[i], "-fpeephole-loads") == 0) {
            config.opt_peephole_loads           = true;
        } else if (strcmp(argv[i], "-fpeephole-immediate-prop") == 0) {
            config.opt_peephole_immediate_prop  = true;
        } else if (strcmp(argv[i], "-fpeephole-jmp-chain") == 0) {
            config.opt_peephole_jmp_chain      = true;
        } else if (strcmp(argv[i], "-fcse") == 0) {
            config.opt_cse                      = true;
        } else if (strcmp(argv[i], "-fdce") == 0) {
            config.opt_dce = true;
        } else if (strcmp(argv[i], "-fconstant-folding") == 0) {
            config.opt_constant_folding = true;
        } else if (strcmp(argv[i], "-fomit-frame-pointers") == 0) {
            config.opt_omit_frame_pointers      = true;
        } else if (strcmp(argv[i], "-finline") == 0) {
            config.opt_inline = true;
        } else if (strcmp(argv[i], "-fpromote-regs") == 0) {
            config.opt_promote_regs             = true;
        } else if (strcmp(argv[i], "-fpromote-leaf") == 0) {
            config.opt_promote_leaf             = true;
        } else if (strcmp(argv[i], "-fpromote-loops") == 0) {
            config.opt_promote_loops            = true;
        } else if (strcmp(argv[i], "-fno-peephole-jumps") == 0) {
            config.opt_peephole_jumps = false;
        } else if (strcmp(argv[i], "-fno-peephole-movs") == 0) {
            config.opt_peephole_movs = false;
        } else if (strcmp(argv[i], "-fno-peephole-immediates") == 0) {
            config.opt_peephole_immediates = false;
        } else if (strcmp(argv[i], "-fno-peephole-reduce") == 0) {
            config.opt_peephole_reduce = false;
        } else if (strcmp(argv[i], "-fno-peephole-pairs") == 0) {
            config.opt_peephole_pairs = false;
        } else if (strcmp(argv[i], "-fno-peephole-algebra") == 0) {
            config.opt_peephole_algebra = false;
        } else if (strcmp(argv[i], "-fno-peephole-forwarding") == 0) {
            config.opt_peephole_forwarding = false;
        } else if (strcmp(argv[i], "-fno-peephole-loads") == 0) {
            config.opt_peephole_loads = false;
        } else if (strcmp(argv[i], "-fno-peephole-immediate-prop") == 0) {
            config.opt_peephole_immediate_prop  = false;
        } else if (strcmp(argv[i], "-fno-peephole-jumps") == 0) {
            config.opt_peephole_jumps = false;
        } else if (strcmp(argv[i], "-fno-peephole-jmp-chain") == 0) {
            config.opt_peephole_jmp_chain      = false;
        } else if (strcmp(argv[i], "-fno-peephole-movs") == 0) {
            config.opt_peephole_movs = false;
        } else if (strcmp(argv[i], "-fno-cse") == 0) {
            config.opt_cse                      = false;
        } else if (strcmp(argv[i], "-fno-dce") == 0) {
            config.opt_dce = false;
        } else if (strcmp(argv[i], "-fno-constant-folding") == 0) {
            config.opt_constant_folding = false;
        } else if (strcmp(argv[i], "-fno-omit-frame-pointers") == 0) {
            config.opt_omit_frame_pointers      = false;
        } else if (strcmp(argv[i], "-fno-inline") == 0) {
            config.opt_inline = false;
        } else if (strcmp(argv[i], "-fno-promote-regs") == 0) {
            config.opt_promote_regs = false;
        } else if (strcmp(argv[i], "-fno-promote-leaf") == 0) {
            config.opt_promote_leaf = false;
        } else if (strcmp(argv[i], "-fno-promote-loops") == 0) {
            config.opt_promote_loops = false;
        } else if (strncmp(argv[i], "-finline-max=", 13) == 0) {
            config.opt_inline_max_body_ins = atoi(argv[i] + 13);
        } else if (strncmp(argv[i], "-finline-call-limit=", 13) == 0) {
            config.opt_inline_call_limit = atoi(argv[i] + 13);
        } else if (strncmp(argv[i], "-finline-exclude=", 17) == 0) {
            safe_str_copy(g_inline_exclude_name, argv[i] + 17,
                          sizeof(g_inline_exclude_name));
        } else if (strncmp(argv[i], "-fmax-passes=", 13) == 0) {
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
    if (config.opt_peephole_pairs)
    {
        if (config.verbose)
            fprintf(stdout, "  - peephole_pairs\n");
        opt_count++;
    }
    if (config.opt_peephole_algebra)
    {
        if (config.verbose)
            fprintf(stdout, "  - peephole_algebra\n");
        opt_count++;
    }
    if (config.opt_peephole_forwarding)
    {
        if (config.verbose)
            fprintf(stdout, "  - peephole_forwarding\n");
        opt_count++;
    }
    if (config.opt_peephole_jumps)
    {
        if (config.verbose)
            fprintf(stdout, "  - peephole_jumps\n");
        opt_count++;
    }
    if (config.opt_peephole_movs)
    {
        if (config.verbose)
            fprintf(stdout, "  - peephole_movs\n");
        opt_count++;
    }
    if (config.opt_peephole_immediates)
    {
        if (config.verbose)
            fprintf(stdout, "  - peephole_immediates\n");
        opt_count++;
    }
    if (config.opt_peephole_reduce)
    {
        if (config.verbose)
            fprintf(stdout, "  - peephole_reduce\n");
        opt_count++;
    }
    if (config.opt_peephole_shifts)
    {
        if (config.verbose)
            fprintf(stdout, "  - peephole_shifts\n");
        opt_count++;
    }
    if (config.opt_peephole_dead_stores)
    {
        if (config.verbose)
            fprintf(stdout, "  - peephole_dead_stores\n");
        opt_count++;
    }
    if (config.opt_peephole_loads)
    {
        if (config.verbose)
            fprintf(stdout, "  - peephole_loads\n");
        opt_count++;
    }
    if (config.opt_peephole_immediate_prop)
    {
        if (config.verbose)
            fprintf(stdout, "  - peephole_immediate_prop\n");
        opt_count++;
    }
    if (config.opt_peephole_jmp_chain)
    {
        if (config.verbose)
            fprintf(stdout, "  - peephole_jmp_chain\n");
        opt_count++;
    }
    if (config.opt_cse)
    {
        if (config.verbose)
            fprintf(stdout, "  - cse\n");
        opt_count++;
    }
    if (config.opt_dce)
    {
        if (config.verbose)
            fprintf(stdout, "  - dce\n");
        opt_count++;
    }
    if (config.opt_constant_folding)
    {
        if (config.verbose)
            fprintf(stdout, "  - constant_folding\n");
        opt_count++;
    }
    if (config.opt_omit_frame_pointers)
    {
        if (config.verbose)
            fprintf(stdout, "  - omit_frame_pointers\n");
        opt_count++;
    }
    if (config.opt_inline)
    {
        if (config.verbose)
            fprintf(stdout, "  - inline\n");
        opt_count++;
    }
    if (config.opt_promote_regs)
    {
        if (config.verbose)
            fprintf(stdout, "  - promote_regs\n");
        opt_count++;
    }
    if (config.opt_promote_leaf)
    {
        if (config.verbose)
            fprintf(stdout, "  - promote_leaf\n");
        opt_count++;
    }
    if (config.opt_promote_loops)
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
        OptType opts[MAX_OPTIMIZATION_ALGORITHMS] = { 0 };
        pass_count++;

        // ----------------------------------------------------------------
        // 1. Interprocedural & Structural (exposes new code to the iteration)
        // ----------------------------------------------------------------
        // Inlining functions can expose new optimization opportunities
        // by replacing function calls with their bodies
        if (config.opt_inline)
        {
            opts[OPT_INLINE]         = inline_trivial_functions (program_ast);
        }

        // ----------------------------------------------------------------
        // 2. Memory-to-Register Promotion
        // ----------------------------------------------------------------
        // These passes convert stack/memory accesses to register operations
        // for better performance and to enable further optimizations.
        if (config.opt_promote_regs)
        {
            opts[OPT_PROMOTE_REGS]   = pass_promote_stack_slots (program_ast);
        }

        if (config.opt_promote_leaf)
        {
            opts[OPT_PROMOTE_LEAF]   = pass_promote_stack_slots (program_ast);
        }

        if (config.opt_promote_loops)
        {
            opts[OPT_PROMOTE_LOOPS]  = pass_promote_loop_registers (program_ast);
        }

        // ----------------------------------------------------------------
        // 3. Data-Flow Forwarding & Immediate Folding
        // ----------------------------------------------------------------
        // Store-to-load forwarding eliminates redundant memory loads
        // by forwarding values directly from stores to loads
        if (config.opt_peephole_forwarding)
        {
            opts[OPT_PEEPHOLE_FORWARDING]  = peephole_forwarding (program_ast);
        }
        // Combine consecutive arithmetic operations with immediate operands
        if (config.opt_peephole_immediates)
        {
            opts[OPT_PEEPHOLE_IMMEDIATES]  = peephole_immediates(program_ast);
        }
        // Replace expensive operations with cheaper equivalents
        if (config.opt_peephole_reduce)
        {
            opts[OPT_PEEPHOLE_REDUCE]      = peephole_reduce (program_ast);
        }

        // ----------------------------------------------------------------
        // 4. Algebraic & Local Instruction Cleanup
        // ----------------------------------------------------------------
        // Algebraic simplifications remove or replace instructions that
        // are algebraically redundant (e.g., MOV r, r)
        if (config.opt_peephole_algebra)
        {
            opts[OPT_PEEPHOLE_ALGEBRA]     = peephole_algebra (program_ast);
        }
        // Peephole optimizations on 2-instruction windows
        if (config.opt_peephole_pairs)
        {
            opts[OPT_PEEPHOLE_PAIRS]       = peephole_pairs (program_ast);
        }
        // Remove redundant MOV instructions
        if (config.opt_peephole_movs)
        {
            opts[OPT_PEEPHOLE_MOVS]        = peephole_movs (program_ast);
        }

        // ----------------------------------------------------------------
        // 5. Additional Peephole Optimizations
        // ----------------------------------------------------------------
        // Dead store elimination: remove stores that are immediately overwritten
        if (config.opt_peephole_dead_stores)
        {
            opts[OPT_PEEPHOLE_DEAD_STORES]  = peephole_dead_stores (program_ast);
        }
        // Redundant load elimination: replace loads with register values
        if (config.opt_peephole_loads)
        {
            opts[OPT_PEEPHOLE_LOADS]        = peephole_loads (program_ast);
        }
        // Immediate propagation: propagate immediate values through MOVs
        if (config.opt_peephole_immediate_prop)
        {
            opts[OPT_PEEPHOLE_IMMEDIATE_PROP]  = peephole_immediate_prop (program_ast);
        }
        // Jump chain elimination: chain consecutive jumps
        if (config.opt_peephole_jmp_chain)
        {
            opts[OPT_PEEPHOLE_JMP_CHAIN]       = peephole_jmp_chain (program_ast);
        }
        // Shift optimizations: optimize shift operations
        if (config.opt_peephole_shifts)
        {
            opts[OPT_PEEPHOLE_SHIFTS]              = peephole_shifts (program_ast);
        }

        // ----------------------------------------------------------------
        // 6. Control Flow & Dead Code Cleanup
        // ----------------------------------------------------------------
        // Remove redundant jumps and dead code to clean up control flow
        if (config.opt_peephole_jumps)
        {
            opts[OPT_PEEPHOLE_JUMPS]               = peephole_jumps (program_ast);
        }

        if (config.opt_cse)
        {
            opts[OPT_CSE]  = opt_cse (program_ast);
        }

        if (config.opt_dce)
        {
            opts[OPT_DCE]  = opt_dce (program_ast);
        }

        // omit frame pointers
        if (config.opt_omit_frame_pointers)
        {
            opts[OPT_OMIT_FRAME_POINTERS]  = omit_frame_pointers (program_ast);
        }

        // Calculate total optimizations for this pass
        for (int index = 0; index < MAX_OPTIMIZATION_ALGORITHMS; index++)
        {
            opts_in_pass  = opts_in_pass + opts[index]; // totals for the round
            tally[index]  = tally[index] + opts[index]; // tally each opt
        }

        total_opts        = total_opts   + opts_in_pass;
        passes            = passes       + 1;

        // Verbose output for this pass
        if (config.verbose && opts_in_pass > 0) {
            printf("Pass %d applied %d optimizations:\n", passes, opts_in_pass);
            for (int index = 0; index < MAX_OPTIMIZATION_ALGORITHMS; index++)
            {
                if (opts[index] >  0)
                {
                    switch (index)
                    {
                        case OPT_PEEPHOLE_PAIRS:
                            fprintf (stdout, "  - peephole-pairs:          ");
                            break;

                        case OPT_PEEPHOLE_ALGEBRA:
                            fprintf (stdout, "  - peephole-algebra:        ");
                            break;

                        case OPT_PEEPHOLE_FORWARDING:
                            fprintf (stdout, "  - peephole-forwarding:     ");
                            break;

                        case OPT_PEEPHOLE_JUMPS:
                            fprintf (stdout, "  - peephole-jumps:          ");
                            break;

                        case OPT_PEEPHOLE_MOVS:
                            fprintf (stdout, "  - peephole-movs:           ");
                            break;

                        case OPT_PEEPHOLE_IMMEDIATES:
                            fprintf (stdout, "  - peephole-immediates:     ");
                            break;

                        case OPT_PEEPHOLE_REDUCE:
                            fprintf (stdout, "  - peephole-reduce:         ");
                            break;

                        case OPT_PEEPHOLE_DEAD_STORES:
                            fprintf (stdout, "  - peephole-dead-stores:    ");
                            break;

                        case OPT_PEEPHOLE_LOADS:
                            fprintf (stdout, "  - peephole-loads:          ");
                            break;

                        case OPT_PEEPHOLE_IMMEDIATE_PROP:
                            fprintf (stdout, "  - peephole-immediate-prop: ");
                            break;

                        case OPT_PEEPHOLE_JMP_CHAIN:
                            fprintf (stdout, "  - peephole-jmp-chain:      ");
                            break;

                        case OPT_PEEPHOLE_SHIFTS:
                            fprintf (stdout, "  - peephole-shifts:         ");
                            break;

                        case OPT_CSE:
                            fprintf (stdout, "  - cse:                     ");
                            break;

                        case OPT_DCE:
                            fprintf (stdout, "  - dce:                     ");
                            break;

                        case OPT_CONSTANT_FOLDING:
                            fprintf (stdout, "  - constant-folding:        ");
                            break;

                        case OPT_INLINE:
                            fprintf (stdout, "  - inline:                  ");
                            break;

                        case OPT_PROMOTE_REGS:
                            fprintf (stdout, "  - promote-regs:            ");
                            break;

                        case OPT_PROMOTE_LEAF:
                            fprintf (stdout, "  - promote-leaf:            ");
                            break;

                        case OPT_PROMOTE_LOOPS:
                            fprintf (stdout, "  - promote-loops:           ");
                            break;

                        case OPT_OMIT_FRAME_POINTERS:
                            fprintf (stdout, "  - omit-frame-pointers:     ");
                            break;
                    }

                    fprintf (stdout, "%d\n", opts[index]);
                }
            }
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

    if (config.opt_constant_folding) {
        if (config.verbose)
            printf("--- Starting Optimization Phase 2: Global Data-Flow ---\n");
        cfg = build_cfg(program_ast);
        propagate_constants_cfg(cfg);

        global_folds = fold_constants_cfg(cfg);
        total_opts += global_folds;
        tally[OPT_CONSTANT_FOLDING]  = tally[OPT_CONSTANT_FOLDING] + global_folds;

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
        OptType opts[MAX_OPTIMIZATION_ALGORITHMS] = { 0 };

        if (config.opt_peephole_immediates)
        {
            opts[OPT_PEEPHOLE_IMMEDIATES]  = peephole_immediates(program_ast);
        }
        if (config.opt_peephole_reduce)
        {
            opts[OPT_PEEPHOLE_REDUCE]      = peephole_reduce (program_ast);
        }
        if (config.opt_peephole_algebra)
        {
            opts[OPT_PEEPHOLE_ALGEBRA]     = peephole_algebra (program_ast);
        }
        if (config.opt_peephole_pairs)
        {
            opts[OPT_PEEPHOLE_PAIRS]       = peephole_pairs (program_ast);
        }
        if (config.opt_peephole_movs)
        {
            opts[OPT_PEEPHOLE_MOVS]        = peephole_movs (program_ast);
        }
        if (config.opt_peephole_jumps)
        {
            opts[OPT_PEEPHOLE_JUMPS]               = peephole_jumps (program_ast);
        }
        if (config.opt_peephole_dead_stores)
        {
            opts[OPT_PEEPHOLE_DEAD_STORES]  = peephole_dead_stores (program_ast);
        }
        if (config.opt_peephole_loads)
        {
            opts[OPT_PEEPHOLE_LOADS]        = peephole_loads (program_ast);
        }
        if (config.opt_peephole_immediate_prop)
        {
            opts[OPT_PEEPHOLE_IMMEDIATE_PROP]  = peephole_immediate_prop (program_ast);
        }
        if (config.opt_peephole_jmp_chain)
        {
            opts[OPT_PEEPHOLE_JMP_CHAIN]       = peephole_jmp_chain (program_ast);
        }
        if (config.opt_peephole_shifts)
        {
            opts[OPT_PEEPHOLE_SHIFTS]              = peephole_shifts (program_ast);
        }
        if (config.opt_omit_frame_pointers)
        {
            opts[OPT_OMIT_FRAME_POINTERS]  = omit_frame_pointers (program_ast);
        }

        for (int index = 0; index < MAX_OPTIMIZATION_ALGORITHMS; index++)
        {
            cleanup_opts  = cleanup_opts + opts[index]; // totals for the round
            tally[index]  = tally[index] + opts[index]; // tally each opt
        }
        total_opts += cleanup_opts;

        if (config.verbose && cleanup_opts > 0) {
            printf("Cleanup Pass applied %d follow-up optimizations:\n",
                   cleanup_opts);
            for (int index = 0; index < MAX_OPTIMIZATION_ALGORITHMS; index++)
            {
                if (opts[index] >  0)
                {
                    switch (index)
                    {
                        case OPT_PEEPHOLE_PAIRS:
                            fprintf (stdout, "  - peephole-pairs:          ");
                            break;

                        case OPT_PEEPHOLE_ALGEBRA:
                            fprintf (stdout, "  - peephole-algebra:        ");
                            break;

                        case OPT_PEEPHOLE_FORWARDING:
                            fprintf (stdout, "  - peephole-forwarding:     ");
                            break;

                        case OPT_PEEPHOLE_JUMPS:
                            fprintf (stdout, "  - peephole-jumps:          ");
                            break;

                        case OPT_PEEPHOLE_MOVS:
                            fprintf (stdout, "  - peephole-movs:           ");
                            break;

                        case OPT_PEEPHOLE_IMMEDIATES:
                            fprintf (stdout, "  - peephole-immediates:     ");
                            break;

                        case OPT_PEEPHOLE_REDUCE:
                            fprintf (stdout, "  - peephole-reduce:         ");
                            break;

                        case OPT_PEEPHOLE_DEAD_STORES:
                            fprintf (stdout, "  - peephole-dead-stores:    ");
                            break;

                        case OPT_PEEPHOLE_LOADS:
                            fprintf (stdout, "  - peephole-loads:          ");
                            break;

                        case OPT_PEEPHOLE_IMMEDIATE_PROP:
                            fprintf (stdout, "  - peephole-immediate-prop: ");
                            break;

                        case OPT_PEEPHOLE_JMP_CHAIN:
                            fprintf (stdout, "  - peephole-jmp-chain:      ");
                            break;

                        case OPT_PEEPHOLE_SHIFTS:
                            fprintf (stdout, "  - peephole-shifts:         ");
                            break;

                        case OPT_CSE:
                            fprintf (stdout, "  - cse:                     ");
                            break;

                        case OPT_DCE:
                            fprintf (stdout, "  - dce:                     ");
                            break;

                        case OPT_CONSTANT_FOLDING:
                            fprintf (stdout, "  - constant-folding:        ");
                            break;

                        case OPT_INLINE:
                            fprintf (stdout, "  - inline:                  ");
                            break;

                        case OPT_PROMOTE_REGS:
                            fprintf (stdout, "  - promote-regs:            ");
                            break;

                        case OPT_PROMOTE_LEAF:
                            fprintf (stdout, "  - promote-leaf:            ");
                            break;

                        case OPT_PROMOTE_LOOPS:
                            fprintf (stdout, "  - promote-loops:           ");
                            break;

                        case OPT_OMIT_FRAME_POINTERS:
                            fprintf (stdout, "  - omit-frame-pointers:     ");
                            break;
                    }

                    fprintf (stdout, "%d\n", opts[index]);
                }
            }
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
    // Testing Results
    // -------------------------------------------------------------------
    if (config.testing)
    {
        for (int index  = 0; index <  MAX_OPTIMIZATION_ALGORITHMS; index++)
        {
            if (tally[index] >  0)
            {
                switch (index)
                {
                    case OPT_PEEPHOLE_PAIRS:
                        fprintf (stdout, "peephole-pairs:");
                        break;

                    case OPT_PEEPHOLE_ALGEBRA:
                        fprintf (stdout, "peephole-algebra:");
                        break;

                    case OPT_PEEPHOLE_FORWARDING:
                        fprintf (stdout, "peephole-forwarding:");
                        break;

                    case OPT_PEEPHOLE_JUMPS:
                        fprintf (stdout, "peephole-jumps:");
                        break;

                    case OPT_PEEPHOLE_MOVS:
                        fprintf (stdout, "peephole-movs:");
                        break;

                    case OPT_PEEPHOLE_IMMEDIATES:
                        fprintf (stdout, "peephole-immediates:");
                        break;

                    case OPT_PEEPHOLE_REDUCE:
                        fprintf (stdout, "peephole-reduce:");
                        break;

                    case OPT_PEEPHOLE_DEAD_STORES:
                        fprintf (stdout, "peephole-dead-stores:");
                        break;

                    case OPT_PEEPHOLE_LOADS:
                        fprintf (stdout, "peephole-loads:");
                        break;

                    case OPT_PEEPHOLE_IMMEDIATE_PROP:
                        fprintf (stdout, "peephole-immediate-prop: ");
                        break;

                    case OPT_PEEPHOLE_JMP_CHAIN:
                        fprintf (stdout, "peephole-jmp-chain:");
                        break;

                    case OPT_PEEPHOLE_SHIFTS:
                        fprintf (stdout, "peephole-shifts:");
                        break;

                    case OPT_CSE:
                        fprintf (stdout, "cse:");
                        break;

                    case OPT_DCE:
                        fprintf (stdout, "dce:");
                        break;

                    case OPT_CONSTANT_FOLDING:
                        fprintf (stdout, "constant-folding:");
                        break;

                    case OPT_INLINE:
                        fprintf (stdout, "inline:");
                        break;

                    case OPT_PROMOTE_REGS:
                        fprintf (stdout, "promote-regs:");
                        break;

                    case OPT_PROMOTE_LEAF:
                        fprintf (stdout, "promote-leaf:");
                        break;

                    case OPT_PROMOTE_LOOPS:
                        fprintf (stdout, "promote-loops:");
                        break;

                    case OPT_OMIT_FRAME_POINTERS:
                        fprintf (stdout, "omit-frame-pointers:");
                        break;
                }

                    fprintf (stdout, "%d\n", tally[index]);
                }
            }

        fprintf (stdout, "total:%d\n", total_opts);
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
