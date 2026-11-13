#!/usr/bin/env bash
##
## run.sh - regression test: function boundaries on v32lua output (function_last() and
## function_complete() in src/promote.c) and the fixed point of promote-* (commit c2abc22).
##
## 80784a8: v32lua puts the top-level code in __global_scope_initialization and the program's
## own functions after it; the pass that finds the function a frame offset belongs to ran
## main() and the top-level code together, so two numeric for loops that happen to use the same
## [BP-n] slots were taken for one function and peephole-lua-checks deleted the top-level
## loop's step and counter stores: the top-level loop never ended. The programs (Lua source
## and the v32lua output next to it; the .asm files are checked in, no compiler is needed):
##   bug1_globalloop  a top-level for loop and a for loop in main()      (the original reproducer)
##   bug1a            a top-level loop only, main() has none
##   bug1b            a top-level loop and a loop in f() that calls g()
##   bug1c            a top-level loop, a loop in main() and a loop in f()
## For each, the result globals (r_*) of the run at -O1, -O2 and -O3 (-L lua) must be those of
## the unoptimized program. TEETH: with the original optimizer (/home/claude/base/v32opt)
## bug1_globalloop does not finish in the simulator at -O1/-O2/-O3, nor bug1c at -O1 (at -O2/-O3
## the original optimizer does not finish on bug1c at all: it also trips c2abc22).
##
## c2abc22: a cold-CALL save/reload of a slot was counted as a use of the slot, so promote-*
## found something new to do after every pass and never reached a fixed point.
##   bug2a  f sums t[i] over a for loop       (original optimizer: never terminates at -O2 and -O3)
##   bug2b  f keeps the last t[i] in a local  (original: never terminates at -O2 -fno-peephole-lua-checks)
## v32opt must finish within a time limit at -O2, -O2 -fno-peephole-lua-checks and -O3, the
## number of fixed-point passes it reports under -v stays at or under MAXPASSES (4 observed,
## the bound has margin), and the results agree with the unoptimized program.
## The Lua sources were compiled with v32lua -w -g --no-build-scripts (the patched compiler that
## writes var= on the ';@loop' line); a comment line at the top of each .asm says so.
## Needs v32as and v32sim built next to this project (the simulator runs every variant).
## Usage: ./run.sh [path/to/v32opt]     (or V32OPT=path, default ../../bin/v32opt)
##
#########################################################################
HERE="$(cd "$(dirname "$0")" && pwd)"
OPT="${1:-${V32OPT:-${HERE}/../../bin/v32opt}}"
case "${OPT}" in /*) ;; *) OPT="$(pwd)/${OPT}" ;; esac
RUN="${HERE}/../lua-asm-run/asmrun.py"
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT
status=0
ncases=0
MAXPASSES=12
LIMIT=30        # seconds for one v32opt run
fail() { echo "FAIL: $1"; status=1; }

[ -x "${OPT}" ] || { echo "FAIL: ${OPT} is not an executable"; exit 1; }

# agree FILE FLAGS... : the results of the optimized run are those of the unoptimized one
agree() {
    local f="$1" want got; shift
    ncases=$((ncases + 1))
    want="$(python3 "${RUN}" "${HERE}/${f}")"
    got="$(python3 "${RUN}" --opt "${OPT}" --timeout "${LIMIT}" "${HERE}/${f}" -- "$@")"
    [ -n "${want}" ] && [ "${want}" = "${got}" ] || fail "${f} $*: unoptimized '${want}', optimized '${got}'"
}

for f in bug1_globalloop bug1a bug1b bug1c bug2a bug2b; do
    for L in -O1 -O2 -O3; do agree "${f}.asm" "${L}"; done
done

# ---- non-convergence: terminates, and in how many passes
for f in bug2a bug2b; do
    for fl in "-O2" "-O2 -fno-peephole-lua-checks" "-O3"; do
        ncases=$((ncases + 1))
        # shellcheck disable=SC2086
        timeout "${LIMIT}" "${OPT}" "${HERE}/${f}.asm" -L lua ${fl} -v -o "${TMP}/o.asm" > "${TMP}/v.txt" 2>&1
        rc=$?
        if [ "${rc}" != 0 ]; then
            fail "${f} ${fl}: v32opt exited ${rc} (124 = did not finish in ${LIMIT} s)"
            continue
        fi
        n="$(grep -c '^Pass [0-9]* applied' "${TMP}/v.txt")"
        [ "${n}" -le "${MAXPASSES}" ] || fail "${f} ${fl}: ${n} passes (more than ${MAXPASSES})"
    done
done
# the unoptimized program and the -O0 run are the same program (the harness itself works)
ncases=$((ncases + 1))
[ "$(python3 "${RUN}" "${HERE}/bug1a.asm")" = "n=1077936128 sum=1086324736" ] || fail "bug1a baseline results changed (t has 3 items summing 6.0)"

if [ "${status}" = 0 ]; then
    echo "PASS: lua-function-bounds (${ncases} checks)"
else
    echo "FAIL: lua-function-bounds (see above)"
fi
exit "${status}"
