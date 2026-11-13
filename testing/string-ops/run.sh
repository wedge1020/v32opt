#!/usr/bin/env bash
##
## run.sh - regression test: MOVS, SETS and CMPS read and write CR, SR and DR (= R11, R12, R13),
## which they do not name, and CR/SR/DR are those registers (modifies_register() and
## is_register_read() in src/helpers.c, forget_string_regs() in src/cfg.c; commit 1aacb50).
## Each program changes its RAM result if the pass named in its row forgets that; each is run
## in v32sim against its unoptimized self (RAM image, STRICT=1; needs v32as and v32sim built
## next to this project) with that single pass enabled and at -O2:
##   known_peephole_movs.asm      peephole-movs          the second set of R11-R13 after MOVS stays
##   alias.asm                    peephole-movs, peephole-dead-stores, constant-folding
##                                                       "mov CR, 5" is a write of R11; MOV R12 before SETS is read
##   ops_dead_stores.asm          peephole-dead-stores   SETS reads R11-R13 (also the alias spellings)
##   ops_constant_folding.asm     constant-folding       R11-R13 are not constants after MOVS
##   ops_constant_folding_alias.asm  constant-folding    "mov SR, 41" replaces the known R12
##   ops_forwarding.asm           peephole-forwarding    a copy of R12 from before MOVS is not R12 after
##   ops_loads.asm                peephole-loads         "mov SR, 41" between two loads of one cell
##   ops_immediate_prop.asm       peephole-immediate-prop  MOV R12, 300 ... IADD R12, 5 around MOVS
##   ops_myopia.asm               peephole-compiler-myopia "mov SR, 41" between a store and its reload
##   ops_cse.asm                  cse                    the same expression of R12 around MOVS
##   ops_cmps.asm                 constant-folding, peephole-movs, peephole-forwarding, peephole-dead-stores
##                                                       the same with CMPS
## TEETH: a build of 1aacb50's parent (the optimizer before the change) fails every row with its
## single pass(es) and, except known_peephole_movs, ops_loads and ops_myopia, at -O2 as well; the
## original optimizer fails 26 of the checks.
## Text checks at the end: the string instructions and the register writes are all still in the
## -O2 output of ops_constant_folding.asm.
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

# FILE:pass,pass,...   (each pass alone, then -O2)
ROWS="known_peephole_movs:peephole-movs
alias:peephole-movs,peephole-dead-stores,constant-folding
ops_dead_stores:peephole-dead-stores
ops_constant_folding:constant-folding
ops_constant_folding_alias:constant-folding
ops_forwarding:peephole-forwarding
ops_loads:peephole-loads
ops_immediate_prop:peephole-immediate-prop
ops_myopia:peephole-compiler-myopia
ops_cse:cse
ops_cmps:constant-folding,peephole-movs,peephole-forwarding,peephole-dead-stores"

cd "${DIFF}" || exit 1
n=0
while IFS=: read -r f passes; do
    flags=(--flags "-O2")
    for p in ${passes//,/ }; do flags+=(--flags "-f${p}"); done
    n=$((n + 1))
    V32OPT="${OPT}" WORK="${TMP}/w${n}" STRICT=1 python3 run.py "${flags[@]}" "${HERE}/${f}.asm" > "${TMP}/d${n}.txt" 2>&1
    rc=$?
    ncases=$((ncases + 1 + ${#flags[@]} / 2))
    while IFS= read -r line; do
        echo "${line}"; status=1
    done < <(grep -E '^(FAIL|ERROR)' "${TMP}/d${n}.txt" | sed -E 's/  +/ /g' | cut -c1-160)
    if [ "${rc}" != 0 ] && [ "${status}" = 0 ]; then
        fail "${f}: harness exited ${rc}: $(head -3 "${TMP}/d${n}.txt" | tr '\n' ' ')"
    fi
done <<< "${ROWS}"

# ---- the string instructions and their register setup survive -O2 (nothing folded into the reads)
"${OPT}" "${HERE}/ops_constant_folding.asm" -O2 -o "${TMP}/cf.asm" >/dev/null 2>&1 || fail "ops_constant_folding.asm -O2: v32opt failed"
for re in '^[[:space:]]*movs$' '^[[:space:]]*mov R0, R12$' '^[[:space:]]*mov R1, R13$' '^[[:space:]]*mov R2, R11$'; do
    ncases=$((ncases + 1))
    grep -qiE "${re}" "${TMP}/cf.asm" || fail "ops_constant_folding.asm -O2: no line matches '${re}'"
done

if [ "${status}" = 0 ]; then
    echo "PASS: string-ops (${ncases} checks)"
else
    echo "FAIL: string-ops (see above)"
fi
exit "${status}"
