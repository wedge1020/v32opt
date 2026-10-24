#include "v32opt.h"

// ===================================================================
// EXECUTION PROFILE  (--profile FILE)
//
// FILE is a line profile of the program being optimized, as written by
// scripts/v32prof.py from a v32sim --profile-out run:
//
//     # v32opt line profile v1
//     # total <instructions executed>
//     <line> <times that line ran>          one per executed instruction line
//
// Every node of the input remembers its line (src_line) and, from the
// profile, how many times it ran (exec). The loop passes ask loop_heat()
// about a loop: the share of the whole run spent between its start label and
// its back edge decides whether it is hot (worth growing the code for), cold
// (not worth even a copy of its test) or neither.
//
// Nodes made by a pass have no line (src_line 0). A loop that contains one
// -- or whose lines the profile does not cover at all -- is reported as
// warm, i.e. treated as if there were no profile, never as cold.
// ===================================================================

bool        g_profile_on       = false;
long long   g_profile_total    = 0;
double      g_profile_hot_pct  = 2.0;
double      g_profile_cold_pct = 0.1;
const char *g_profile_file     = NULL;

void assign_source_lines(AsmNode *head)
{
    int line = 0;
    for (AsmNode *n = head; n; n = n->next) n->src_line = line++;     // the dummy head is line 0
}

bool profile_arg(const char *arg)
{
    if (strncmp(arg, "profile-hot=", 12) == 0)  { g_profile_hot_pct  = atof(arg + 12); return true; }
    if (strncmp(arg, "profile-cold=", 13) == 0) { g_profile_cold_pct = atof(arg + 13); return true; }
    return false;
}

int profile_load(AsmNode *head, const char *file)
{
    FILE *fp = fopen(file, "r");
    if (!fp) {
        fprintf(stderr, "v32opt: cannot open profile '%s': %s\n", file, strerror(errno));
        return 1;
    }
    // index the nodes by line
    long nlines = 0;
    for (AsmNode *n = head; n; n = n->next) nlines++;
    AsmNode **by_line = calloc((size_t)nlines + 1, sizeof(AsmNode *));
    for (AsmNode *n = head; n; n = n->next) if (n->src_line >= 0 && n->src_line <= nlines) by_line[n->src_line] = n;

    char buf[256];
    long long total = 0, covered = 0;
    long bad = 0, entries = 0;
    while (fgets(buf, sizeof(buf), fp)) {
        if (buf[0] == '#') {
            long long t;
            if (sscanf(buf, "# total %lld", &t) == 1) total = t;
            continue;
        }
        long line;
        long long count;
        if (sscanf(buf, "%ld %lld", &line, &count) != 2) continue;
        entries++;
        AsmNode *n = (line >= 1 && line <= nlines) ? by_line[line] : NULL;
        if (!n || n->type == OP_OTHER || n->type == OP_LABEL) { bad++; continue; }
        n->exec = count;
        covered += count;
    }
    fclose(fp);
    free(by_line);
    if (entries == 0 || bad * 10 > entries) {
        fprintf(stderr, "v32opt: the profile '%s' does not match the input (%ld of %ld lines are not instructions); ignoring it\n",
                file, bad, entries);
        for (AsmNode *n = head; n; n = n->next) n->exec = 0;
        return 1;
    }
    g_profile_total = total ? total : covered;
    g_profile_on = g_profile_total > 0;
    return 0;
}

int loop_heat(AsmNode *from, AsmNode *to)
{
    if (!g_profile_on) return 0;
    long long sum = 0;
    for (AsmNode *n = from; n; n = n->next) {
        if (n->type != OP_LABEL && n->type != OP_OTHER) {
            if (n->src_line == 0) return 0;                 // made by a pass: unknown
            sum += n->exec;
        }
        if (n == to) break;
    }
    if (sum == 0) return 0;                                 // not covered by the profile: unknown
    double share = 100.0 * (double)sum / (double)g_profile_total;
    if (share >= g_profile_hot_pct) return 1;
    if (share < g_profile_cold_pct) return -1;
    return 0;
}
