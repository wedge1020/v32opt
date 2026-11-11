#!/usr/bin/env bash
##
## run.sh - regression test: the promote passes do not add a register save that an [SP+k]
## operand of the function can reach (frame_info() in src/promote.c).
##
## Under the callee-save ABI (the stock compiler, and v32cc output without a ';@hints'
## header) promote-regs/-leaf/-loops save a register they start to use with a fresh PUSH after
## the prologue. In a function whose single "ISUB SP, N" reserves both the locals and the
## outgoing arguments (v32cc), that PUSH sits at the top of the stack, where "MOV [SP+k], x"
## then stores the arguments: the epilogue POP gave an argument back to the caller as the saved
## register. The stock compiler's separate "ISUB SP, argwords" before the call is not reached.
## Programs (testing/differential/asm):
##   save_args_v32cc.asm   h: one ISUB, arguments staged at [SP], [SP+1]: no new PUSH
##   save_args_stock.asm   leaf0/leaf1/mid0: stock layout: they do get new PUSHes
## Each is run in v32sim against its unoptimized self at -O2 and -O3 (RAM image, STRICT=1),
## which needs v32as and v32sim built next to this project.
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

V="${DIFF}/asm/save_args_v32cc.asm"
S="${DIFF}/asm/save_args_stock.asm"

# number of "PUSH Rn" (not BP) in function $2 of file $1
pushes() {
    awk -v f="__function_$2:" '$0 == f {on=1; next} on && /^__function_/ && $0 !~ /_return:$/ {exit}
        on && tolower($1) == "push" && tolower($2) != "bp" {n++} END {print n + 0}' "$1"
}

for L in -O2 -O3; do
    "${OPT}" "${V}" ${L} -o "${TMP}/v.${L}.asm" >/dev/null 2>&1 || fail "save_args_v32cc ${L}: v32opt failed"
    "${OPT}" "${S}" ${L} -o "${TMP}/s.${L}.asm" >/dev/null 2>&1 || fail "save_args_stock ${L}: v32opt failed"
    # v32cc layout: h has no PUSH of a register, and its ISUB and arguments are as they were
    ncases=$((ncases + 3))
    n="$(pushes "${TMP}/v.${L}.asm" h)"
    [ "${n}" = 0 ] || fail "save_args_v32cc ${L}: h got ${n} register save(s), expected none"
    grep -qiE '^[[:space:]]*mov \[SP\+1\], R1$' "${TMP}/v.${L}.asm" || fail "save_args_v32cc ${L}: the argument store [SP+1] is gone"
    grep -qiE '^[[:space:]]*isub SP, 5$' "${TMP}/v.${L}.asm" || fail "save_args_v32cc ${L}: h lost its ISUB SP, 5"
    # stock layout: more saves than the compiler wrote, in each function
    for f in leaf0 leaf1 mid0; do
        ncases=$((ncases + 1))
        before="$(pushes "${S}" "${f}")"
        after="$(pushes "${TMP}/s.${L}.asm" "${f}")"
        [ "${after}" -gt "${before}" ] || fail "save_args_stock ${L}: ${f} has ${after} register saves, the input ${before}: none was added"
    done
done

# ---- the optimized programs agree with the unoptimized ones in the simulator
cd "${DIFF}" || exit 1
V32OPT="${OPT}" WORK="${TMP}/work" STRICT=1 python3 run.py --flags "-O2" --flags "-O3" "${V}" "${S}" > "${TMP}/diff.txt" 2>&1
rc=$?
ncases=$((ncases + 4))
while IFS= read -r line; do
    echo "${line}"; status=1
done < <(grep -E '^(FAIL|ERROR)' "${TMP}/diff.txt" | sed -E 's/  +/ /g' | cut -c1-160)
if [ "${rc}" != 0 ] && [ "${status}" = 0 ]; then
    fail "harness exited ${rc}: $(head -3 "${TMP}/diff.txt" | tr '\n' ' ')"
fi

if [ "${status}" = 0 ]; then
    echo "PASS: promote-save (${ncases} checks)"
else
    echo "FAIL: promote-save (see above)"
fi
exit "${status}"
