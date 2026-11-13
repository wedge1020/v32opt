#!/usr/bin/env bash
##
## run.sh - regression test: the compiler-ABI rule for open classes of the liveness analysis
## (class_trusts_abi() in src/liveness.c, OPEN CLASSES paragraph of the header comment; commit
## 2f9327c), and the reuse of liveness sessions (commits 47034c9, 2f9327c).
##
## A function that can be entered from an unknown place (reached through "CALL Rn" / "JMP Rn", or
## from the program start) has callers nobody knows, so at its RET every register is live, unless
## a compiler vouches for the convention: the input is v32lua output (-L lua) or has a ';@hints'
## header, and every unknown way in is a "__function_" entry. Then only the ABI registers are live
## there. Without it no register is ever free for the promote passes in v32lua output.
##   second_r1.asm   hand-written, no header: __function_f returns a second result in R1 and is
##                   reached only through "CALL R2". The strict rule applies and R1 must survive:
##                   the results agree with the unoptimized run at -fpeephole-dead-regs, -O2, -O3,
##                   and "mov R1, 9" is still in __function_f. (The same program WITH a header,
##                   second_r1_hdr.asm = this plus the line ";@hints 1 abi=caller-save", is
##                   miscompiled by design: the header promises a convention that f breaks, and
##                   the write to R1 is deleted. That is the documented consequence of a false
##                   header, so it is not a test.)
##   trust_lua.lua   v32lua output (trust_lua.asm, compiled with v32lua -w -g --no-build-scripts):
##                   two functions with loops, reached only through the runtime's exec. With the
##                   rule, promote-loops reports at least one promotion (4 observed) and the result
##                   globals r_* agree with the unoptimized run at -O2 and -O3. TEETH: a build with
##                   LV_TRUST_COMPILER_ABI set to 0 reports none (and no promote-regs either); so
##                   does the original optimizer's promote-loops on this input.
##
## Reuse: with V32OPT_LV_NOREUSE=1 liveness sessions are rebuilt for every pass; the output must be
## byte-identical to the normal one. Three inputs of different kinds, each at -O2 and -O3:
## second_r1.asm (hand-written), trust_lua.asm (v32lua, -L lua), ../frame-cleanup/frame_remove.asm
## (compiler-shaped C-mode code).
## Needs v32as and v32sim built next to this project for the result checks.
## Usage: ./run.sh [path/to/v32opt]     (or V32OPT=path, default ../../bin/v32opt)
##
#########################################################################
HERE="$(cd "$(dirname "$0")" && pwd)"
OPT="${1:-${V32OPT:-${HERE}/../../bin/v32opt}}"
case "${OPT}" in /*) ;; *) OPT="$(pwd)/${OPT}" ;; esac
DIFF="${HERE}/../differential"
RUN="${HERE}/../lua-asm-run/asmrun.py"
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT
status=0
ncases=0
fail() { echo "FAIL: $1"; status=1; }

[ -x "${OPT}" ] || { echo "FAIL: ${OPT} is not an executable"; exit 1; }

# ---- hand-written, no header: R1 survives
S="${HERE}/second_r1.asm"
cd "${DIFF}" || exit 1
V32OPT="${OPT}" WORK="${TMP}/work" STRICT=1 python3 run.py \
    --flags "-fpeephole-dead-regs" --flags "-O2" --flags "-O3" "${S}" > "${TMP}/diff.txt" 2>&1
rc=$?
ncases=$((ncases + 3))
while IFS= read -r line; do
    echo "${line}"; status=1
done < <(grep -E '^(FAIL|ERROR)' "${TMP}/diff.txt" | sed -E 's/  +/ /g' | cut -c1-160)
if [ "${rc}" != 0 ] && [ "${status}" = 0 ]; then
    fail "harness exited ${rc}: $(head -3 "${TMP}/diff.txt" | tr '\n' ' ')"
fi
"${OPT}" "${S}" -O2 -o "${TMP}/s.asm" >/dev/null 2>&1 || fail "second_r1.asm -O2: v32opt failed"
ncases=$((ncases + 1))
awk '$0 == "__function_f:" {on=1; next} on && /^__function_/ {exit} on' "${TMP}/s.asm" | grep -qiE '^[[:space:]]*mov R1, 9$' ||
    fail "second_r1.asm -O2: 'mov R1, 9' is gone from __function_f"

# ---- Lua shape: promote-loops promotes, results agree
L="${HERE}/trust_lua.asm"
for lv in -O2 -O3; do
    ncases=$((ncases + 1))
    "${OPT}" "${L}" -L lua "${lv}" -t -o "${TMP}/l.asm" > "${TMP}/l.txt" 2>/dev/null
    n="$(sed -n 's/^promote-loops:\([0-9]*\)$/\1/p' "${TMP}/l.txt" | head -1)"
    [ -n "${n}" ] && [ "${n}" -ge 1 ] || fail "trust_lua.asm ${lv}: promote-loops reported '${n}' promotions (expected at least 1)"
    ncases=$((ncases + 1))
    want="$(python3 "${RUN}" "${L}")"
    got="$(python3 "${RUN}" --opt "${OPT}" "${L}" -- "${lv}")"
    [ -n "${want}" ] && [ "${want}" = "${got}" ] || fail "trust_lua.asm ${lv}: unoptimized '${want}', optimized '${got}'"
done

# ---- reuse: V32OPT_LV_NOREUSE=1 gives the same bytes
reuse() {   # reuse FILE FLAGS...
    local f="$1"; shift
    ncases=$((ncases + 1))
    "${OPT}" "${f}" "$@" -o "${TMP}/r1.asm" >/dev/null 2>&1 || { fail "$(basename "${f}") $*: v32opt failed"; return; }
    V32OPT_LV_NOREUSE=1 "${OPT}" "${f}" "$@" -o "${TMP}/r2.asm" >/dev/null 2>&1 || { fail "$(basename "${f}") $* NOREUSE: v32opt failed"; return; }
    cmp -s "${TMP}/r1.asm" "${TMP}/r2.asm" || fail "$(basename "${f}") $*: V32OPT_LV_NOREUSE=1 changes the output"
}
for lv in -O2 -O3; do
    reuse "${S}" "${lv}"
    reuse "${L}" -L lua "${lv}"
    reuse "${HERE}/../frame-cleanup/frame_remove.asm" "${lv}"
done

if [ "${status}" = 0 ]; then
    echo "PASS: liveness-trust (${ncases} checks)"
else
    echo "FAIL: liveness-trust (see above)"
fi
exit "${status}"
