#!/usr/bin/env bash
##
## run.sh - regression test: constant-folding entry states (pin_blocks in src/cfg.c).
##
## The CFG has only jump and fall-through edges, so a block control also enters by a CALL, by
## a computed jump through a taken address, or from the boot, starts with every register
## unknown. Each program has a "mov rX, rY" that must NOT become "mov rX, imm":
##   fold_across_call_target_label.asm   f0: mov r6,5 / f1: mov r2,r6, with "call f1" elsewhere
##   fold_fallthrough_entry.asm          f5 falls into f6, which is also called (these two are
##                                       testing/differential/asm/fold_*.asm)
##   entries_data.asm    a label mentioned only in "pointer _t", reached by a computed call
##   entries_first.asm   the first block of the program, also the target of a back edge
##   entries_call.asm    the block after a CALL (the callee changed R5-R7)
## entries_fold.asm is the control: a label that is only a plain JT target still folds.
## Every program is then run in v32sim against its unoptimized self at -fconstant-folding, -O2
## and -O3 (RAM image, STRICT=1), which needs v32as and v32sim built next to this project.
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

# check NAME FILE present|absent REGEX... : on the folded output, case-insensitive, whole line
check() {
    local name="$1" in="$2" want="$3" re; shift 3
    "${OPT}" "${in}" -fconstant-folding -o "${TMP}/${name}.asm" >/dev/null 2>&1 || { fail "${name}: v32opt failed"; return; }
    for re in "$@"; do
        ncases=$((ncases + 1))
        if grep -qiE "^[[:space:]]*${re}[[:space:]]*$" "${TMP}/${name}.asm"; then
            [ "${want}" = present ] || fail "${name}: '${re}' was produced"
        else
            [ "${want}" = absent ] || fail "${name}: '${re}' is gone"
        fi
    done
}

A="${DIFF}/asm"
check fold_call_target "${A}/fold_across_call_target_label.asm" present 'mov r2, r6'
check fold_call_target "${A}/fold_across_call_target_label.asm" absent  'mov r2, 5'
check fold_fallthrough "${A}/fold_fallthrough_entry.asm" present 'mov r7, r5'
check fold_fallthrough "${A}/fold_fallthrough_entry.asm" absent  'mov r7, 7'
check data  "${HERE}/entries_data.asm"  present 'mov r2, r6'
check data  "${HERE}/entries_data.asm"  absent  'mov r2, 5' 'mov r2, 13'
check first "${HERE}/entries_first.asm" present 'mov r2, r1'
check first "${HERE}/entries_first.asm" absent  'mov r2, 5'
check call  "${HERE}/entries_call.asm"  present 'mov r2, r6' 'mov r3, r7' 'mov r4, r5'
check call  "${HERE}/entries_call.asm"  absent  'mov r2, 5' 'mov r3, 6' 'mov r4, 7'
# the control
check fold  "${HERE}/entries_fold.asm"  present 'mov r2, 5' 'mov r3, 5'
check fold  "${HERE}/entries_fold.asm"  absent  'mov r2, r6' 'mov r3, r6'

# ---- the optimized programs agree with the unoptimized ones in the simulator
files=("${A}/fold_across_call_target_label.asm" "${A}/fold_fallthrough_entry.asm"
       "${HERE}/entries_data.asm" "${HERE}/entries_first.asm" "${HERE}/entries_call.asm" "${HERE}/entries_fold.asm")
cd "${DIFF}" || exit 1
V32OPT="${OPT}" WORK="${TMP}/work" STRICT=1 python3 run.py \
    --flags "-fconstant-folding" --flags "-O2" --flags "-O3" "${files[@]}" > "${TMP}/diff.txt" 2>&1
rc=$?
ncases=$((ncases + ${#files[@]} * 3))
while IFS= read -r line; do
    echo "${line}"; status=1
done < <(grep -E '^(FAIL|ERROR)' "${TMP}/diff.txt" | sed -E 's/  +/ /g' | cut -c1-160)
if [ "${rc}" != 0 ] && [ "${status}" = 0 ]; then
    fail "harness exited ${rc}: $(head -3 "${TMP}/diff.txt" | tr '\n' ' ')"
fi

if [ "${status}" = 0 ]; then
    echo "PASS: constant-folding-entries (${ncases} checks)"
else
    echo "FAIL: constant-folding-entries (see above)"
fi
exit "${status}"
