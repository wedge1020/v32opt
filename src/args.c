#include "v32opt.h"

// --- Optimization Level Presets ------------------------------------------
static void set_opt_level(OptConfig *cfg, int level) {

    // Disable all by default
    cfg -> opt_peephole_algebra          = false;
    cfg -> opt_peephole_compiler_myopia  = false;
    cfg -> opt_peephole_dead_stores      = false;
    cfg -> opt_peephole_forwarding       = false;
    cfg -> opt_peephole_immediates       = false;
    cfg -> opt_peephole_immediate_prop   = false;
    cfg -> opt_peephole_jmp_chain        = false;
    cfg -> opt_peephole_jumps            = false;
    cfg -> opt_peephole_loads            = false;
    cfg -> opt_peephole_movs             = false;
    cfg -> opt_peephole_pairs            = false;
    cfg -> opt_peephole_reduce           = false;
    cfg -> opt_peephole_shifts           = false;
    cfg -> opt_cse                       = false;
    cfg -> opt_constant_folding          = false;
    cfg -> opt_dce                       = false;
    cfg -> opt_omit_frame_pointers       = false;
    cfg -> opt_inline                    = false;

    // still disabled by default: needs further testing/completion
    cfg -> opt_promote_regs              = false;
    cfg -> opt_promote_leaf              = false;
    cfg -> opt_promote_loops             = false;

    if (level                           == 0) return; // -O0: all off

    // -O1: peephole optimizations
    cfg -> opt_peephole_algebra          = true;
    cfg -> opt_peephole_compiler_myopia  = true;
    cfg -> opt_peephole_dead_stores      = true;
    cfg -> opt_peephole_forwarding       = true;
    cfg -> opt_peephole_immediates       = true;
    cfg -> opt_peephole_immediate_prop   = true;
    cfg -> opt_peephole_jmp_chain        = true;
    cfg -> opt_peephole_jumps            = true;
    cfg -> opt_peephole_loads            = true;
    cfg -> opt_peephole_movs             = true;
    cfg -> opt_peephole_pairs            = true;
    cfg -> opt_peephole_reduce           = true;
    cfg -> opt_peephole_shifts           = true;

    if (level                           == 1) return;

    // -O2: add CSE, DCE, constant folding, omit-frame-pointers
    cfg -> opt_cse                       = true;
    cfg -> opt_constant_folding          = true;
    cfg -> opt_dce                       = true;
    cfg -> opt_omit_frame_pointers       = true;

    if (level                           == 2) return;

    // -O3: add inlining
    cfg -> opt_inline                    = true;

    if (level                           == 3) return;
    //cfg->opt_inline = false;
    //cfg->opt_peephole_immediate_prop = false;
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

static void print_version(const char *prog_name)
{
    fprintf(stdout, "%s %s\n", prog_name, VERSION);
    fprintf(stdout, "Assembly Optimizer targeting Vircon32 (v32opt) by %s\n", AUTHOR);
    fprintf(stdout, "  github: %s\n", URL);
}

void print_usage(const char *prog_name)
{
    fprintf(stdout, "Assembly Optimizer for Vircon32 (%s)\n", prog_name);
    fprintf(stdout, "Usage: %s <input.asm> [-o output.asm] [options]\n", prog_name);
    fprintf(stdout, "Options:\n");
    fprintf(stdout, "  --help, -h       Displays this information\n");
    fprintf(stdout, "  --version, -V    Displays optimizer version\n");
    fprintf(stdout, "  -v               Verbose output (show pass statistics)\n");
    fprintf(stdout, "  -d               In-code debugging (mark the hits)\n");
    fprintf(stdout, "  -t               Testing mode: just display opt:hits\n");
    fprintf(stdout, "  -L <mode>        Language mode: 'C' (default) or 'lua'\n");
    fprintf(stdout, "                   Lua mode enables boxed-type awareness\n\n");
    fprintf(stdout, "  --dot <cfg.dot>  Export Control Flow Graph to DOT format\n");
    fprintf(stdout, "  -o <file>        Specify output assembly file name\n");
    fprintf(stdout, "  -O0              Disable all optimizations [default]\n");
    fprintf(stdout, "  -Os              Enables space-saving optimizations\n");
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
    fprintf(stdout, "  promote-loops, omit-frame-pointers, peephole-compiler-myopia\n\n");
    fprintf(stdout, "Diagnostic Flags:\n");
    fprintf(stdout, "  -finline-max=N   Cap the number of inlined CALL sites to N\n");
    fprintf(stdout, "  -fmax-passes=N   Cap the maximum iterative optimization passes to N\n");
    fprintf(stdout, "  --trigger-max=N  Global cap: allow at most N total transformations\n");
    fprintf(stdout, "                   to commit across EVERY enabled pass and EVERY\n");
    fprintf(stdout, "                   fixed-point iteration, combined. Once N is reached,\n");
    fprintf(stdout, "                   every later candidate is left untouched, as if it had\n");
    fprintf(stdout, "                   never matched. Omit (or pass a negative N) for the\n");
    fprintf(stdout, "                   default, unlimited behavior.\n");
    fprintf(stdout, "                   Meant for bisecting a miscompile: re-run with\n");
    fprintf(stdout, "                   increasing N until the output breaks -- the Nth\n");
    fprintf(stdout, "                   transform applied (in program order, across all\n");
    fprintf(stdout, "                   passes) is the one to inspect. Combine with -d to see\n");
    fprintf(stdout, "                   exactly which transform that was in the output.\n\n");
    fprintf(stdout, "NOTE: peephole-dead-stores,\n");
    fprintf(stdout, "promote-regs, promote-leaf, and promote-loops not yet\n");
    fprintf(stdout, "connected to any optimization category. Test and bugfix first\n\n");
}

// --- Main Argument Processor ----------------------------------------------
void process_args(int argc, char **argv, OptConfig *cfg,
                 char *in_file, size_t in_size,
                 char *out_file, size_t out_size,
                 char *dot_file, size_t dot_size,
                 int *max_passes)
{
    optind          = 1; // Reset getopt state
    opterr          = 0; // Suppress default errors

    cfg->lang_mode  = LANG_C;  // Default to C mode

    // Long options
    static struct option long_opts[] = {
        {"dot",          required_argument, NULL, 'D'},
        {"langmode",     required_argument, NULL, 'L'},
        {"version",      no_argument,       NULL, 'V'},
        {"help",         no_argument,       NULL, 'h'},
        {"trigger-max",  required_argument, NULL, 'T'},
        {NULL, 0, NULL, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "hVvdto:O:f:L:", long_opts, NULL)) != -1) {
        switch (opt) {
            case 'h': print_usage(argv[0]); exit(0);
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

            case 'T': {
                char *endptr = NULL;
                long val = strtol(optarg, &endptr, 10);
                if (endptr == optarg || *endptr != '\0') {
                    fprintf(stderr, "ERROR: --trigger-max requires an integer, got '%s'\n", optarg);
                    exit(1);
                }
                // Negative means "unlimited" (same as omitting the flag,
                // since g_trigger_max already defaults to -1) -- accepted
                // rather than rejected so scripts that compute N don't need
                // a special case for "no cap this run".
                g_trigger_max = val;
                break;
            }
            case 'V': print_version(argv[0]); exit(0);
            case 'f':
                if (!handle_f_arg(optarg, cfg, max_passes)) {
                    fprintf(stderr, "ERROR: unrecognized optimization '%s'\n", optarg);
                    exit(1);
                }
                break;

            case 'L':
                if (strcmp(optarg, "c") == 0 || strcmp(optarg, "C") == 0) {
                    cfg->lang_mode = LANG_C;
                } else if (strcmp(optarg, "lua") == 0 || strcmp(optarg, "LUA") == 0) {
                    cfg->lang_mode = LANG_LUA;
                } else {
                    fprintf(stderr, "ERROR: Unknown language mode '%s'. Use 'c' or 'lua'.\n", optarg);
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
