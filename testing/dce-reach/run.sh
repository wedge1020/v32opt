#!/usr/bin/env bash
##
## run.sh - regression test: dead-code elimination keeps a function that is reached without a
## CALL to its entry label, and still removes one that nothing reaches (src/dce.c, step 3B;
## commit 7dcd2e8). A function is live when any operand names any label of it (a jump into an
## inner label, a shared tail) and when a live function falls off its end into it.
##   fall.asm                         __function_b only fallen into from __function_a
##   ../frame-cleanup/shared_tail.asm function a jumps to "__b_tail" inside function b, which
##                                    nothing calls (the same program the frame-cleanup suite runs)
##   dce_data.asm                     a function named only by "pointer" in a data table (computed
##                                    CALL); the control: __function_dead and __function_dead2,
##                                    which only calls the first, are removed; called functions stay
## Each is run with -fdce (and -O2, -O3 for the two that survive inlining) and must assemble and
## agree with the unoptimized run in v32sim (RAM image, STRICT=1; needs v32as and v32sim).
## TEETH: with the original optimizer fall.asm and shared_tail.asm fail to assemble (undefined
## label: the function was removed); dce_data.asm passes there too (the data mention was already
## honoured), which is the point of the guard.
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

run() { "${OPT}" "$1" "${@:3}" -o "$2" >/dev/null 2>&1 || fail "$(basename "$1") ${*:3}: v32opt failed"; }
has()  { ncases=$((ncases + 1)); grep -qE "$2" "$1" || fail "$3"; }
hasnt() { ncases=$((ncases + 1)); ! grep -qE "$2" "$1" || fail "$3"; }

T="${HERE}/../frame-cleanup/shared_tail.asm"
for f in fall.asm dce_data.asm "${T}"; do
    case "${f}" in /*) p="${f}" ;; *) p="${HERE}/${f}" ;; esac
    b="$(basename "${f}" .asm)"
    run "${p}" "${TMP}/${b}.asm" -fdce
done
# fall.asm: both functions stay and the code between is unchanged
has  "${TMP}/fall.asm" '^__function_a:' "fall: __function_a was removed"
has  "${TMP}/fall.asm" '^__function_b:' "fall: __function_b (only fallen into) was removed"
# shared_tail.asm: function b (nobody calls it) and its inner label stay
has  "${TMP}/shared_tail.asm" '^__function_b:' "shared_tail: __function_b was removed"
has  "${TMP}/shared_tail.asm" '^__b_tail:' "shared_tail: __b_tail was removed"
has  "${TMP}/shared_tail.asm" 'jt R0, __b_tail' "shared_tail: the jump to __b_tail is gone"
# dce_data.asm
has   "${TMP}/dce_data.asm" '^__function_cb:'    "dce_data: __function_cb (pointer in data) was removed"
has   "${TMP}/dce_data.asm" '^__function_calls:' "dce_data: __function_calls was removed"
has   "${TMP}/dce_data.asm" '^__function_leaf:'  "dce_data: __function_leaf was removed"
hasnt "${TMP}/dce_data.asm" '^__function_dead:'  "dce_data: __function_dead is still there"
hasnt "${TMP}/dce_data.asm" '^__function_dead2:' "dce_data: __function_dead2 is still there"
# the dead functions go at -O2 too, and the data-reached one stays
run "${HERE}/dce_data.asm" "${TMP}/d2.asm" -O2
has   "${TMP}/d2.asm" '^__function_cb:'   "dce_data -O2: __function_cb was removed"
hasnt "${TMP}/d2.asm" '^__function_dead' "dce_data -O2: a dead function is still there"
# --trigger-max=0: nothing is removed
run "${HERE}/dce_data.asm" "${TMP}/d0.asm" -fdce --trigger-max=0
has   "${TMP}/d0.asm" '^__function_dead:' "--trigger-max=0 still removed __function_dead"

# ---- against the unoptimized run in the simulator
cd "${DIFF}" || exit 1
V32OPT="${OPT}" WORK="${TMP}/work" STRICT=1 python3 run.py \
    --flags "-fdce" --flags "-O2" --flags "-O3" "${HERE}/fall.asm" "${HERE}/dce_data.asm" > "${TMP}/diff.txt" 2>&1
rc=$?
V32OPT="${OPT}" WORK="${TMP}/work2" STRICT=1 python3 run.py \
    --flags "-fdce" --flags "-O2" --flags "-O3" "${T}" >> "${TMP}/diff.txt" 2>&1
[ "${rc}" = 0 ] && rc=$?
ncases=$((ncases + 3 * 3))
while IFS= read -r line; do
    echo "${line}"; status=1
done < <(grep -E '^(FAIL|ERROR)' "${TMP}/diff.txt" | sed -E 's/  +/ /g' | cut -c1-160)
if [ "${rc}" != 0 ] && [ "${status}" = 0 ]; then
    fail "harness exited ${rc}: $(head -3 "${TMP}/diff.txt" | tr '\n' ' ')"
fi

if [ "${status}" = 0 ]; then
    echo "PASS: dce-reach (${ncases} checks)"
else
    echo "FAIL: dce-reach (see above)"
fi
exit "${status}"
