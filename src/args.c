#include "v32opt.h"
#include <getopt.h>

// --- Optimization Level Presets ------------------------------------------
static void set_opt_level(OptConfig *cfg, int level) {
    // Disable all
    cfg->opt_peephole_compiler_myopia =
    cfg->opt_peephole_pairs = cfg->opt_peephole_algebra = cfg->opt_peephole_forwarding =
    cfg->opt_peephole_jumps = cfg->opt_peephole_movs = cfg->opt_peephole_immediates =
    cfg->opt_peephole_reduce = cfg->opt_peephole_shifts = cfg->opt_peephole_dead_stores =
    cfg->opt_peephole_loads = cfg->opt_peephole_immediate_prop = cfg->opt_peephole_jmp_chain =
    cfg->opt_cse = cfg->opt_dce = cfg->opt_constant_folding = cfg->opt_inline =
    cfg->opt_promote_regs = cfg->opt_promote_leaf = cfg->opt_promote_loops =
    cfg->opt_omit_frame_pointers = false;

    if (level == 0) return; // -O0: all off

    // -O1
    cfg->opt_peephole_compiler_myopia =
    cfg->opt_peephole_pairs = cfg->opt_peephole_algebra = cfg->opt_peephole_jumps =
    cfg->opt_peephole_jmp_chain = cfg->opt_peephole_immediates = cfg->opt_peephole_reduce =
    cfg->opt_peephole_shifts = cfg->opt_peephole_immediate_prop = true;

    if (level == 1) return;

    // -O2: add CSE, DCE, constant folding, omit-frame-pointers
    cfg->opt_cse = cfg->opt_dce = cfg->opt_constant_folding = cfg->opt_omit_frame_pointers = true;

    if (level == 2) return;

    // -O3: add inlining
    cfg->opt_inline = true;

    if (level == 3) return;
    cfg->opt_inline = false;
    cfg->opt_peephole_immediate_prop = false;
}

// --- Handle -f<name> / -fno-<name> ----------------------------------------
static bool handle_f_arg(const char *arg, OptConfig *cfg, int *max_passes) {
    if (strncmp(arg, "no-", 3) == 0) {
        // Disable
        const char *name = arg + 3;
        if (strcmp(name, "peephole-pairs") == 0) cfg->opt_peephole_pairs = false;
        else if (strcmp(name, "peephole-algebra") == 0) cfg->opt_peephole_algebra = false;
        else if (strcmp(name, "peephole-compiler-myopia") == 0) cfg->opt_peephole_compiler_myopia = false;
        else if (strcmp(name, "peephole-forwarding") == 0) cfg->opt_peephole_forwarding = false;
        else if (strcmp(name, "peephole-jumps") == 0) cfg->opt_peephole_jumps = false;
        else if (strcmp(name, "peephole-movs") == 0) cfg->opt_peephole_movs = false;
        else if (strcmp(name, "peephole-immediates") == 0) cfg->opt_peephole_immediates = false;
        else if (strcmp(name, "peephole-reduce") == 0) cfg->opt_peephole_reduce = false;
        else if (strcmp(name, "peephole-shifts") == 0) cfg->opt_peephole_shifts = false;
        else if (strcmp(name, "peephole-dead-stores") == 0) cfg->opt_peephole_dead_stores = false;
        else if (strcmp(name, "peephole-loads") == 0) cfg->opt_peephole_loads = false;
        else if (strcmp(name, "peephole-immediate-prop") == 0) cfg->opt_peephole_immediate_prop = false;
        else if (strcmp(name, "peephole-jmp-chain") == 0) cfg->opt_peephole_jmp_chain = false;
        else if (strcmp(name, "cse") == 0) cfg->opt_cse = false;
        else if (strcmp(name, "dce") == 0) cfg->opt_dce = false;
        else if (strcmp(name, "constant-folding") == 0) cfg->opt_constant_folding = false;
        else if (strcmp(name, "omit-frame-pointers") == 0) cfg->opt_omit_frame_pointers = false;
        else if (strcmp(name, "inline") == 0) cfg->opt_inline = false;
        else if (strcmp(name, "promote-regs") == 0) cfg->opt_promote_regs = false;
        else if (strcmp(name, "promote-leaf") == 0) cfg->opt_promote_leaf = false;
        else if (strcmp(name, "promote-loops") == 0) cfg->opt_promote_loops = false;
        else return false;
        return true;
    }

    // Parameterized options
    if (strncmp(arg, "inline-max=", 11) == 0) { cfg->opt_inline_max_body_ins = atoi(arg + 11); return true; }
    if (strncmp(arg, "inline-call-limit=", 17) == 0) { cfg->opt_inline_call_limit = atoi(arg + 17); g_inline_call_limit = cfg->opt_inline_call_limit; return true; }
    if (strncmp(arg, "inline-exclude=", 15) == 0) { safe_str_copy(g_inline_exclude_name, arg + 15, sizeof(g_inline_exclude_name)); return true; }
    if (strncmp(arg, "max-passes=", 11) == 0) { *max_passes = atoi(arg + 11); return true; }

    // Enable
    if (strcmp(arg, "peephole-pairs") == 0) cfg->opt_peephole_pairs = true;
    else if (strcmp(arg, "peephole-algebra") == 0) cfg->opt_peephole_algebra = true;
    else if (strcmp(arg, "peephole-compiler-myopia") == 0) cfg->opt_peephole_compiler_myopia = true;
    else if (strcmp(arg, "peephole-forwarding") == 0) cfg->opt_peephole_forwarding = true;
    else if (strcmp(arg, "peephole-jumps") == 0) cfg->opt_peephole_jumps = true;
    else if (strcmp(arg, "peephole-movs") == 0) cfg->opt_peephole_movs = true;
    else if (strcmp(arg, "peephole-immediates") == 0) cfg->opt_peephole_immediates = true;
    else if (strcmp(arg, "peephole-reduce") == 0) cfg->opt_peephole_reduce = true;
    else if (strcmp(arg, "peephole-shifts") == 0) cfg->opt_peephole_shifts = true;
    else if (strcmp(arg, "peephole-dead-stores") == 0) cfg->opt_peephole_dead_stores = true;
    else if (strcmp(arg, "peephole-loads") == 0) cfg->opt_peephole_loads = true;
    else if (strcmp(arg, "peephole-immediate-prop") == 0) cfg->opt_peephole_immediate_prop = true;
    else if (strcmp(arg, "peephole-jmp-chain") == 0) cfg->opt_peephole_jmp_chain = true;
    else if (strcmp(arg, "cse") == 0) cfg->opt_cse = true;
    else if (strcmp(arg, "dce") == 0) cfg->opt_dce = true;
    else if (strcmp(arg, "constant-folding") == 0) cfg->opt_constant_folding = true;
    else if (strcmp(arg, "omit-frame-pointers") == 0) cfg->opt_omit_frame_pointers = true;
    else if (strcmp(arg, "inline") == 0) cfg->opt_inline = true;
    else if (strcmp(arg, "promote-regs") == 0) cfg->opt_promote_regs = true;
    else if (strcmp(arg, "promote-leaf") == 0) cfg->opt_promote_leaf = true;
    else if (strcmp(arg, "promote-loops") == 0) cfg->opt_promote_loops = true;
    else return false;

    return true;
}

// --- Main Argument Processor ----------------------------------------------
void process_args(int argc, char **argv, OptConfig *cfg,
                 char *in_file, size_t in_size,
                 char *out_file, size_t out_size,
                 char *dot_file, size_t dot_size,
                 int *max_passes) {
    optind = 1; // Reset getopt state
    opterr = 0; // Suppress default errors

    // Long options
    static struct option long_opts[] = {
        {"dot", required_argument, NULL, 'D'},
        {NULL, 0, NULL, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "vdto:O:f:", long_opts, NULL)) != -1) {
        switch (opt) {
            case 'v': cfg->verbose = true; break;
            case 'd': cfg->debug = true; break;
            case 't': cfg->testing = true; break;
            case 'o': safe_str_copy(out_file, optarg, out_size); break;
            case 'O':
                switch (optarg[0])
                {
                    case '0':
                        set_opt_level (cfg, 0);
                        break;

                    case '1':
                        set_opt_level (cfg, 1);
                        break;

                    case '2':
                        set_opt_level (cfg, 2);
                        break;

                    case '3':
                        set_opt_level (cfg, 3);
                        break;

                    case 's':
                        set_opt_level (cfg, 4);
                        break;

                    default:
                        fprintf (stderr, "ERROR: unrecognized optimization level '%s'\n", optarg);
                        exit (1);
                        break;
                }
                break;

            case 'D': safe_str_copy(dot_file, optarg, dot_size); break;
            case 'f':
                if (!handle_f_arg(optarg, cfg, max_passes)) {
                    fprintf(stderr, "ERROR: unrecognized optimization '%s'\n", optarg);
                    exit(1);
                }
                break;
            case '?': exit(1); // getopt already printed error
            default: fprintf(stderr, "ERROR: unknown option '-%c'\n", opt); exit(1);
        }
    }

    // ADD THIS NEW LOGIC:
    // Positional arg is our input file
    if (optind < argc) {
        safe_str_copy(in_file, argv[optind], in_size);
    } else {
        fprintf(stderr, "ERROR: No input assembly file specified.\n");
        exit(1);
    }
}
