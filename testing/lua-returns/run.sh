#!/usr/bin/env bash
##
## run.sh - regression test: ';@fn F returns=N' (written by v32lua) in the liveness analysis
## (g_ret1 in src/liveness.c).
##
## v32lua returns a second and third value in R2/R3 and stores how many in [RET_COUNT], which callers
## look at before they read them. When every function a RET can belong to has "returns=N" with N <= 1,
## R2 and R3 are not live at that RET (-L lua) and a write to them before it is dead.
##   ret1.asm      hand-written: __function_f, reached only through "CALL R1", writes R2 and R3 before its
##                 RET. With returns=1 or returns=0 both writes go; with returns=2, or without a hint, both
##                 stay. (TEETH: the other two cases.)
##   returns.lua   v32lua output (returns.asm, compiled with v32lua -w -g --no-build-scripts): functions
##                 that return one, two and three values, forward a call (`return two(n)`), return `...`
##                 or return nothing, called directly, through a table and in multiple assignments.
##                 The hints are in the file for the first, second, third and last kind and not for the
##                 forwarding ones; the results r_* agree with the unoptimized run at -O1 -O2 -O3.
##   header        the ';@hints 1 abi=caller-save lang=lua' line v32lua writes selects -L lua when -L is not
##                 given (an explicit -L c wins).
## Needs v32as and v32sim built next to this project for the result checks.
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
fail() { echo "FAIL: $1"; status=1; }

[ -x "${OPT}" ] || { echo "FAIL: ${OPT} is not an executable"; exit 1; }

# ---- unit: which hint frees R2/R3
count() { grep -cE "^[[:space:]]+MOV +$2, $3\$" "$1"; }   # count FILE REG VALUE
for h in returns=1 returns=0 returns=2 none; do
    { [ "${h}" != none ] && echo ";@fn __function_f ${h}"; cat "${HERE}/ret1.asm"; } > "${TMP}/u.asm"
    "${OPT}" "${TMP}/u.asm" -L lua -O2 -o "${TMP}/u.o.asm" >/dev/null 2>&1 || { fail "ret1.asm ${h}: v32opt failed"; continue; }
    ncases=$((ncases + 1))
    r2="$(count "${TMP}/u.o.asm" R2 4)"; r3="$(count "${TMP}/u.o.asm" R3 5)"
    case "${h}" in
        returns=1|returns=0) want=0 ;;
        *)                   want=1 ;;
    esac
    [ "${r2}" = "${want}" ] && [ "${r3}" = "${want}" ] || fail "ret1.asm ${h}: MOV R2,4 x${r2} and MOV R3,5 x${r3}, expected ${want} of each"
done

# ---- the compiler writes what the test relies on
A="${HERE}/returns.asm"
for f in one two three none; do
    ncases=$((ncases + 1))
    grep -qE "^[[:space:]]*;@fn __function_${f} returns=[0-9]+\$" "${A}" || fail "returns.asm: no returns= line for ${f}"
done
for f in fwd va; do
    ncases=$((ncases + 1))
    grep -qE ";@fn __function_${f} returns=" "${A}" && fail "returns.asm: ${f} forwards values and must have no returns= line"
done

# ---- the header's "lang=lua" (v32lua output) is -L lua unless -L is given
"${OPT}" "${A}" -O2 -o "${TMP}/h0.asm" >/dev/null 2>&1
"${OPT}" "${A}" -O2 -L lua -o "${TMP}/h1.asm" >/dev/null 2>&1
"${OPT}" "${A}" -O2 -L c -o "${TMP}/h2.asm" >/dev/null 2>&1
ncases=$((ncases + 1))
cmp -s "${TMP}/h0.asm" "${TMP}/h1.asm" || fail "returns.asm -O2 without -L differs from -L lua (the header's lang=lua was not used)"
ncases=$((ncases + 1))
cmp -s "${TMP}/h0.asm" "${TMP}/h2.asm" && fail "returns.asm -O2 -L c gives the same as without -L (an explicit -L must win)"

# ---- results agree
want="$(python3 "${RUN}" "${A}")"
for lv in -O1 -O2 -O3; do
    ncases=$((ncases + 1))
    got="$(python3 "${RUN}" --opt "${OPT}" "${A}" -- "${lv}")"
    [ -n "${want}" ] && [ "${want}" = "${got}" ] || fail "returns.asm ${lv}: unoptimized '${want}', optimized '${got}'"
done

if [ "${status}" = 0 ]; then
    echo "PASS: lua-returns (${ncases} checks)"
else
    echo "FAIL: lua-returns (see above)"
fi
exit "${status}"
