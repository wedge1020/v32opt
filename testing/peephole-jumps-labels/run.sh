#!/usr/bin/env bash
##
## run.sh - regression test: peephole-jumps deletes "JMP L" when only labels, comments and blank
## lines lie between it and L (src/peephole/jumps.c, commit 6e5873c; "L2(i)" of doc/lua-loops.md).
## jumps_labels.asm has one function per case (run with -fpeephole-jumps only, every function
## compared as text):
##   fire_one, fire_two, fire_three  the JMP over one, two and three labels is deleted
##   fire_comments                   comments and blank lines in the run do not stop it
##   keep_instr                      an instruction between the jump and its target: kept
##   keep_directive                  a directive (%define) between: it ends the run, kept
##   keep_jt, keep_jf                JT and JF over two labels are untouched (one-label form only)
## --trigger-max=0 leaves the whole program as it is; the optimized program also runs in v32sim
## against its unoptimized self (RAM image, STRICT=1) at -fpeephole-jumps, -O1 and -O2, which
## needs v32as and v32sim built next to this project.
## scope_records.asm (commit 2f6a632): a ';@scope }' record in the unreachable range after a JMP stays
## (--check-vars exits 0; the build before the fix reports an unclosed ';@scope {').
## TEETH: with the original optimizer fire_two, fire_three and fire_comments keep their JMP.
## Usage: ./run.sh [path/to/v32opt]     (or V32OPT=path, default ../../bin/v32opt)
##
#########################################################################
HERE="$(cd "$(dirname "$0")" && pwd)"
OPT="${1:-${V32OPT:-${HERE}/../../bin/v32opt}}"
case "${OPT}" in /*) ;; *) OPT="$(pwd)/${OPT}" ;; esac
DIFF="${HERE}/../differential"
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT
status=0
ncases=0
fail() { echo "FAIL: $1"; status=1; }

[ -x "${OPT}" ] || { echo "FAIL: ${OPT} is not an executable"; exit 1; }

fn() {
    awk -v f="__function_$2:" '$0 == f {on=1; next} on && /^__function_/ && $0 !~ /_return:$/ {exit} on && !/^[ \t]*;/ && !/^[ \t]*$/ {print}' "$1" 2>/dev/null |
        tr 'A-Z' 'a-z' | sed -e 's/^[ \t]*//' -e 's/[ \t]*$//' -e 's/[ \t][ \t]*/ /g'
}
A="${HERE}/jumps_labels.asm"
"${OPT}" "${A}" -fpeephole-jumps -o "${TMP}/a.asm" >/dev/null 2>&1 || fail "jumps_labels.asm: v32opt failed"

expect() {
    ncases=$((ncases + 1))
    local got
    got="$(fn "${TMP}/a.asm" "$1" | tr '\n' '|')"
    [ "${got}" = "$2|" ] || fail "$1: expected '$2', got '${got%|}'"
}
expect fire_one       'mov r0, 1|__fo_a:|iadd r0, 10|ret'
expect fire_two       'mov r0, 2|jt r7, __ft_a|mov r1, 0|__ft_a:|__ft_b:|iadd r0, 10|ret'
expect fire_three     'mov r0, 3|__fh_a:|__fh_b:|__fh_c:|iadd r0, 10|ret'
expect fire_comments  'mov r0, 4|__fc_a:|__fc_b:|iadd r0, 10|ret'
for f in keep_instr keep_directive keep_jt keep_jf; do
    ncases=$((ncases + 1))
    [ -n "$(fn "${A}" "${f}")" ] || { fail "${f}: not in jumps_labels.asm"; continue; }
    [ "$(fn "${A}" "${f}")" = "$(fn "${TMP}/a.asm" "${f}")" ] || fail "${f} was changed"
done

# ---- the count: four JMPs deleted (-t prints "peephole-jumps:N")
"${OPT}" "${A}" -fpeephole-jumps -t -o "${TMP}/at.asm" > "${TMP}/at.txt" 2>/dev/null
ncases=$((ncases + 1))
[ "$(sed -n 's/^peephole-jumps:\([0-9]*\)$/\1/p' "${TMP}/at.txt")" = 4 ] || fail "-t: expected 4 deletions: $(tr '\n' ' ' < "${TMP}/at.txt")"

# ---- --trigger-max=0
"${OPT}" "${A}" -fpeephole-jumps --trigger-max=0 -o "${TMP}/c0.asm" >/dev/null 2>&1
ncases=$((ncases + 1))
cmp -s <(grep -v '^[ \t]*;' "${A}") <(grep -v '^[ \t]*;' "${TMP}/c0.asm") || fail "--trigger-max=0 still deleted a jump"
# one at a time: --trigger-max=1 deletes exactly one
"${OPT}" "${A}" -fpeephole-jumps --trigger-max=1 -t -o "${TMP}/c1.asm" > "${TMP}/c1.txt" 2>/dev/null
ncases=$((ncases + 1))
[ "$(sed -n 's/^peephole-jumps:\([0-9]*\)$/\1/p' "${TMP}/c1.txt")" = 1 ] || fail "--trigger-max=1: expected 1 deletion: $(tr '\n' ' ' < "${TMP}/c1.txt")"

# ---- scope_records.asm (commit 2f6a632): the ';@scope }' record in unreachable code survives
S="${HERE}/scope_records.asm"
"${OPT}" "${S}" -L lua -fpeephole-jumps --check-vars -o "${TMP}/s.asm" > "${TMP}/s.txt" 2>&1
rc=$?
ncases=$((ncases + 1))
[ "${rc}" = 0 ] || fail "scope_records.asm -fpeephole-jumps --check-vars: exit ${rc}: $(head -2 "${TMP}/s.txt" | tr '\n' ' ')"
ncases=$((ncases + 1))
grep -q ';@scope }' "${TMP}/s.asm" || fail "scope_records.asm: the ';@scope }' line is gone"
ncases=$((ncases + 1))
[ "$(grep -ci '^[[:space:]]*jmp ' "${TMP}/s.asm")" = 0 ] || fail "scope_records.asm: a JMP is left"
"${OPT}" "${S}" -O0 -L lua -fpeephole-jumps --check-vars -o /dev/null >/dev/null 2>&1
ncases=$((ncases + 1))
[ $? = 0 ] || fail "scope_records.asm -O0 -fpeephole-jumps --check-vars failed"

# ---- the simulator
cd "${DIFF}" || exit 1
V32OPT="${OPT}" WORK="${TMP}/work" STRICT=1 python3 run.py \
    --flags "-fpeephole-jumps" --flags "-O1" --flags "-O2" "${A}" "${S}" > "${TMP}/diff.txt" 2>&1
rc=$?
ncases=$((ncases + 6))
while IFS= read -r line; do
    echo "${line}"; status=1
done < <(grep -E '^(FAIL|ERROR)' "${TMP}/diff.txt" | sed -E 's/  +/ /g' | cut -c1-160)
if [ "${rc}" != 0 ] && [ "${status}" = 0 ]; then
    fail "harness exited ${rc}: $(head -3 "${TMP}/diff.txt" | tr '\n' ' ')"
fi

if [ "${status}" = 0 ]; then
    echo "PASS: peephole-jumps-labels (${ncases} checks)"
else
    echo "FAIL: peephole-jumps-labels (see above)"
fi
exit "${status}"
