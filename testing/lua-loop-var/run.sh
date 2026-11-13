#!/usr/bin/env bash
##
## run.sh - regression test: lua-loop-var (-L lua, -O2 and up; src/peephole/lua_checks.c, commit
## 2fe50eb, "L3" of doc/lua-loops.md). With "var=v" on the ';@loop kind=for' line v32lua promises
## that nothing but the per-iteration copy writes the visible loop variable [BP+v]; the pass
## then reads the counter slot instead and deletes the copy. Programs (Lua source and v32lua's
## output next to it, so no compiler is needed; the headers of the .asm files say how they were made):
##   directed.lua/.asm   16 functions p01..p12: 14 numeric for loops carry var= and must be rewritten
##                       (plain, nested with the same name, nested with another name, break, goto
##                       continue, return from inside, a call in the body, slot reuse after the loop,
##                       inner limit the outer variable); p08 (captured by a closure), p09 (assigned
##                       in the body) and p10 (variadic) have no var= and must not be touched
##   fill.lua/.asm       one loop, the base of the hand-edited negatives
##   fill_old.asm        the same loop from a v32lua without the hint: no var=, untouched
## Checked: the count (-t prints "lua-loop-var:14"), the loops named by V32OPT_LOOPVAR_DEBUG=1
## (stderr: "lua-loop-var: LABEL rewritten" / the reason), the result globals r_* of the run at -O2,
## -O3 and -O2 -fno-lua-loop-var agree with the unoptimized program, --trigger-max=0 leaves it
## alone, no ";@loop ... var=" is left on a rewritten loop, and --check-src is clean (and --check-vars on fill.asm).
## Five hand-made negatives are made from fill.asm by inserting one line (the reason is what
## V32OPT_LOOPVAR_DEBUG names; none of them changes the output at all):
##   n1  a second store to the variable in the body        (use)
##   n2  LEA of the variable's slot (address taken)       (facts(frame))
##   n3  a read of the variable after the counter step    (step)
##   n4  a read of the variable after the loop, before any write  (exit)
##   n5  a jump into the loop body from outside           (facts(loop))
## TEETH: the pass is new in 2fe50eb: with the original optimizer the count is 0 on every input,
## so the rewrite checks fail (the negatives pass there, trivially).
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

# count FILE FLAGS... : the number lua-loop-var reports (-t), and its stderr reasons in $TMP/why.txt
count() {
    local f="$1"; shift
    V32OPT_LOOPVAR_DEBUG=1 "${OPT}" "${f}" -L lua "$@" -t -o "${TMP}/t.asm" > "${TMP}/t.txt" 2> "${TMP}/why.txt"
    sed -n 's/^lua-loop-var:\([0-9]*\)$/\1/p' "${TMP}/t.txt" | head -1
}
# agree FILE FLAGS... : the results of the optimized run are those of the unoptimized one
agree() {
    local f="$1" want got; shift
    ncases=$((ncases + 1))
    want="$(python3 "${RUN}" "${f}")"
    got="$(python3 "${RUN}" --opt "${OPT}" "${f}" -- "$@")"
    [ -n "${want}" ] && [ "${want}" = "${got}" ] || fail "$(basename "${f}") $*: unoptimized '${want}', optimized '${got}'"
}
# insert_after FILE REGEX TEXT OUT : TEXT goes after the first line matching REGEX
insert_after() {
    awk -v re="$2" -v ins="$3" '{print} !done && $0 ~ re {print ins; done=1}' "$1" > "$4"
}

# ---- directed.asm: fourteen rewrites
D="${HERE}/directed.asm"
ncases=$((ncases + 1))
n="$(count "${D}" -O2)"
[ "${n}" = 14 ] || fail "directed.asm -O2: expected lua-loop-var:14, got '${n}'"
for l in p01_fill_for_start_0 p01_for_start_0 p02a_for_start_1 p02b_for_start_1 p03_for_start_0 p03_for_start_1 \
         p04_for_start_0 p05_for_start_0 p06_for_start_0 p07_for_start_0 p11a_for_start_0 p11b_for_start_0 \
         p12_for_start_0 p12_for_start_1; do
    ncases=$((ncases + 1))
    grep -q "^lua-loop-var: __${l} rewritten$" "${TMP}/why.txt" || fail "loop __${l} was not rewritten"
done
# the three loops without var= are not mentioned at all
for l in p08 p09 p10; do
    ncases=$((ncases + 1))
    ! grep -q "__${l}_for" "${TMP}/why.txt" || fail "${l}: the pass looked at a loop without var=: $(grep "__${l}_for" "${TMP}/why.txt" | head -1)"
done
ncases=$((ncases + 1))
[ "$(count "${D}" -O3)" = 14 ] || fail "directed.asm -O3: expected lua-loop-var:14"
ncases=$((ncases + 1))
[ "$(count "${D}" -O1)" = "" ] || fail "directed.asm -O1: the pass ran at -O1"
ncases=$((ncases + 1))
[ "$(count "${D}" -O2 -fno-lua-loop-var)" = "" ] || fail "directed.asm -O2 -fno-lua-loop-var: the pass ran"
# no rewritten loop keeps its var= (the fact no longer holds)
"${OPT}" "${D}" -L lua -O2 -o "${TMP}/d2.asm" >/dev/null 2>&1 || fail "directed.asm -O2: v32opt failed"
ncases=$((ncases + 1))
[ "$(grep -c ';@loop.* var=' "${TMP}/d2.asm")" = 0 ] || fail "a rewritten loop still has var= on its ';@loop' line"
# --trigger-max=0
ncases=$((ncases + 1))
[ "$(count "${D}" -O2 --trigger-max=0)" = "" ] || fail "--trigger-max=0 still rewrote a loop"
# the unoptimized program and the optimized ones agree
agree "${D}" -O2
agree "${D}" -O3
agree "${D}" -O2 -fno-lua-loop-var

# ---- fill.asm: one loop; --check-src --check-vars clean on the rewritten program
F="${HERE}/fill.asm"
ncases=$((ncases + 1))
[ "$(count "${F}" -O2)" = 1 ] || fail "fill.asm -O2: expected lua-loop-var:1"
ncases=$((ncases + 1))
"${OPT}" "${F}" -L lua -O2 --check-src --check-vars -o "${TMP}/f2.asm" > "${TMP}/chk.txt" 2>&1
rc=$?
[ "${rc}" = 0 ] || fail "fill.asm -O2 --check-src --check-vars: exit ${rc}: $(head -3 "${TMP}/chk.txt" | tr '\n' ' ')"
# (directed.asm is NOT checked with --check-vars: p08's closure makes the checker complain about the
# unoptimized input itself, at -O0; see ISSUES)
ncases=$((ncases + 1))
"${OPT}" "${D}" -L lua -O2 --check-src -o "${TMP}/d3.asm" > "${TMP}/chk.txt" 2>&1
rc=$?
[ "${rc}" = 0 ] || fail "directed.asm -O2 --check-src: exit ${rc}: $(head -3 "${TMP}/chk.txt" | tr '\n' ' ')"
agree "${F}" -O2

# ---- an input from a compiler without the hint is untouched by the pass
O="${HERE}/fill_old.asm"
ncases=$((ncases + 1))
[ "$(count "${O}" -O2)" = "" ] || fail "fill_old.asm: the pass rewrote a loop without var="
ncases=$((ncases + 1))
"${OPT}" "${O}" -L lua -O2 -o "${TMP}/o1.asm" >/dev/null 2>&1
"${OPT}" "${O}" -L lua -O2 -fno-lua-loop-var -o "${TMP}/o2.asm" >/dev/null 2>&1
cmp -s "${TMP}/o1.asm" "${TMP}/o2.asm" || fail "fill_old.asm: -O2 differs with and without the pass"
agree "${O}" -O2

# ---- hand-edited negatives: each refused for its reason, the output unchanged by the pass
neg() {     # neg NAME REGEX TEXT REASON [REGEX2 TEXT2]
    local n="$1" f="${TMP}/$1.asm"
    insert_after "${F}" "$2" "$3" "${f}"
    if [ -n "$5" ]; then insert_after "${f}" "$5" "$6" "${f}.2"; mv "${f}.2" "${f}"; fi
    ncases=$((ncases + 1))
    [ "$(count "${f}" -O2)" = "" ] || fail "${n}: the loop was rewritten"
    ncases=$((ncases + 1))
    grep -qxF "lua-loop-var: __fill_for_start_0 $4" "${TMP}/why.txt" || fail "${n}: expected the reason '$4', got '$(sort -u "${TMP}/why.txt" | grep lua-loop-var | tr '\n' ' ')'"
    ncases=$((ncases + 1))
    "${OPT}" "${f}" -L lua -O2 -o "${TMP}/${n}.o1.asm" >/dev/null 2>&1
    "${OPT}" "${f}" -L lua -O2 -fno-lua-loop-var -o "${TMP}/${n}.o2.asm" >/dev/null 2>&1
    cmp -s "${TMP}/${n}.o1.asm" "${TMP}/${n}.o2.asm" || fail "${n}: output differs with and without the pass"
}
neg n1 "MOV   \\[BP - 4\\], R3 ; init local 'i'" "    MOV   [BP - 4], R2" "use"
neg n2 "MOV   \\[BP - 4\\], R3 ; init local 'i'" "    LEA   R6, [BP - 4]" "facts(frame)"
neg n3 "MOV   \\[BP - 3\\], R1 ; Save incremented control counter" "    MOV   R6, [BP - 4]" "step"
neg n4 "^__fill_for_end_0:" "    MOV   R6, [BP - 4]\n    MOV   [BP - 6], R6" "exit"
neg n5 "MOV   \\[BP - 4\\], R1 ; init local 'i'" "    JF    R0, __fill_mid" "facts(loop)" "MOV   R2, \\[BP - 4\\] ; load 'i'" "__fill_mid:"

if [ "${status}" = 0 ]; then
    echo "PASS: lua-loop-var (${ncases} checks)"
else
    echo "FAIL: lua-loop-var (see above)"
fi
exit "${status}"
