#!/usr/bin/env bash
##
## run.sh - regression test: promote-loops across hot CALLs (L4 of doc/lua-loops.md; src/promote.c,
## commit f22c44d; -fpromote-loops-calls, on wherever promote-loops is, -fno-promote-loops-calls
## turns it off). A direct CALL to a routine with a ';@fn F clobbers=' list no longer ends the
## loop: the loop's slot goes in a register outside the lists. cases.lua / cases.asm (v32lua
## output, checked in) has one numeric for loop per function:
##   q08  a table store in the body                       promoted across CALL __builtin_table_set
##   q07c a goto-continue loop with a table store         promoted
##   q04  the body calls a user function (__builtin_exec) refused: "exec"
##   q05  string concatenation (__builtin_strcat)         refused: callee without a list
## and one made by hand from cases.asm: q08 with its CALL replaced by "MOV R9, f / CALL R9"
## (a CALL through a register) -> refused "call-through-register", q07c still promoted.
## Asserted from V32OPT_PROMOTE_CALLS_DEBUG=1 (stderr: "promote-loops-calls: LABEL promoted|refused why"):
## the two promotions are there, the two refusals are, and q04/q05 are never promoted. With
## -fno-promote-loops-calls there is no promotion at all (-t reports no promote-loops line, the
## default reports 10, the other loops of the program are not affected) and every loop is
## refused as unhinted/exec. The results r_* of the run at -O2, -O3 and -O2 -fno-promote-loops-calls
## agree with the unoptimized program; V32OPT_PROMOTE_CALLS_CHECK=1 (walks each callee for a write of
## the chosen register; it prints "promote-loops-calls-check: LABEL CALLEE Rn ok" per call) says ok for
## every call and does not change the output.
## TEETH: the optimizer before f22c44d has no such switch and reports no "promoted" line.
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
C="${HERE}/cases.asm"

# why FILE FLAGS... : stderr of the debug run in $TMP/why.txt, the promote-loops count in $TMP/n.txt
why() {
    local f="$1"; shift
    V32OPT_PROMOTE_CALLS_DEBUG=1 "${OPT}" "${f}" -L lua "$@" -t -o "${TMP}/t.asm" > "${TMP}/t.txt" 2> "${TMP}/why.txt"
    sed -n 's/^promote-loops:\([0-9]*\)$/\1/p' "${TMP}/t.txt" | head -1 > "${TMP}/n.txt"
}
promoted() { grep -q "^promote-loops-calls: __$2_for_start_0 promoted" "$1"; }
refused()  { grep -q "^promote-loops-calls: __$2_for_start_0 refused $3" "$1"; }
agree() {
    local f="$1" want got; shift
    ncases=$((ncases + 1))
    want="$(python3 "${RUN}" "${f}")"
    got="$(python3 "${RUN}" --opt "${OPT}" "${f}" -- "$@")"
    [ -n "${want}" ] && [ "${want}" = "${got}" ] || fail "$(basename "${f}") $*: unoptimized '${want}', optimized '${got}'"
}

why "${C}" -O2
n="$(cat "${TMP}/n.txt")"
ncases=$((ncases + 1)); promoted "${TMP}/why.txt" q08  || fail "q08 was not promoted across its CALL"
ncases=$((ncases + 1)); promoted "${TMP}/why.txt" q07c || fail "q07c (goto continue) was not promoted across its CALL"
ncases=$((ncases + 1)); refused  "${TMP}/why.txt" q04 exec || fail "q04 (user function) was not refused as exec"
ncases=$((ncases + 1)); refused  "${TMP}/why.txt" q05 unhinted || fail "q05 (strcat) was not refused as unhinted"
ncases=$((ncases + 1)); ! promoted "${TMP}/why.txt" q04 || fail "q04 was promoted"
ncases=$((ncases + 1)); ! promoted "${TMP}/why.txt" q05 || fail "q05 was promoted"
ncases=$((ncases + 1)); [ -n "${n}" ] && [ "${n}" -ge 2 ] || fail "-O2: promote-loops reported '${n}'"

# ---- the switch
why "${C}" -O2 -fno-promote-loops-calls
ncases=$((ncases + 1)); [ -z "$(cat "${TMP}/n.txt")" ] || fail "-fno-promote-loops-calls: promote-loops still reports $(cat "${TMP}/n.txt") promotions"
for l in q08 q07c; do
    ncases=$((ncases + 1)); ! promoted "${TMP}/why.txt" "${l}" || fail "-fno-promote-loops-calls: ${l} promoted"
    ncases=$((ncases + 1)); refused "${TMP}/why.txt" "${l}" unhinted || fail "-fno-promote-loops-calls: ${l} not refused as unhinted"
done
why "${C}" -O2 -fpromote-loops-calls
ncases=$((ncases + 1)); promoted "${TMP}/why.txt" q08 || fail "-fpromote-loops-calls: q08 not promoted"

# ---- a CALL through a register is refused
awk '/^__function_q08:/{on=1} on&&/CALL  __builtin_table_set/&&!d{print "    MOV   R9, __builtin_table_set"; print "    CALL  R9"; d=1; next} {print}' "${C}" > "${TMP}/m2.asm"
why "${TMP}/m2.asm" -O2
ncases=$((ncases + 1)); refused  "${TMP}/why.txt" q08 call-through-register || fail "m2: CALL R9 not refused"
ncases=$((ncases + 1)); ! promoted "${TMP}/why.txt" q08 || fail "m2: q08 promoted across CALL R9"
ncases=$((ncases + 1)); promoted "${TMP}/why.txt" q07c || fail "m2: q07c no longer promoted"
agree "${TMP}/m2.asm" -O2

# ---- the results
agree "${C}" -O2
agree "${C}" -O3
agree "${C}" -O2 -fno-promote-loops-calls

# ---- the check aid: nothing found, same output
ncases=$((ncases + 1))
"${OPT}" "${C}" -L lua -O2 -o "${TMP}/o1.asm" >/dev/null 2>&1
V32OPT_PROMOTE_CALLS_CHECK=1 "${OPT}" "${C}" -L lua -O2 -o "${TMP}/o2.asm" > "${TMP}/chk.txt" 2>&1
rc=$?
{ [ "${rc}" = 0 ] && cmp -s "${TMP}/o1.asm" "${TMP}/o2.asm" && grep -q "^promote-loops-calls-check: .* ok$" "${TMP}/chk.txt" && ! grep "^promote-loops-calls-check:" "${TMP}/chk.txt" | grep -qv " ok$"; } ||
    fail "V32OPT_PROMOTE_CALLS_CHECK=1: rc=${rc} or output differs: $(head -2 "${TMP}/chk.txt" | tr '\n' ' ')"

if [ "${status}" = 0 ]; then
    echo "PASS: promote-loops-calls (${ncases} checks)"
else
    echo "FAIL: promote-loops-calls (see above)"
fi
exit "${status}"
