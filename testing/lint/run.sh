#!/usr/bin/env bash
##
## run.sh - regression test: no pass may write a register the input did not
##          (callee_save_lint.py).  Every fixture in testing/*/ is optimized
##          at -O1, -O2 and -O3 and compared with its input.  Needs python3
##          and no Vircon32 toolchain.   Usage: ./run.sh [path/to/v32opt]
##
#########################################################################
OPT="${1:-../../bin/v32opt}"
HERE="$(cd "$(dirname "$0")" && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT
command -v python3 >/dev/null || { echo "SKIP: python3 not found"; exit 0; }
status=0
n=0
ROOT="$(dirname "${HERE}")"
for f in "${ROOT}"/*/*.asm; do
    case "${f}" in *Opt.asm|*/differential/*|*/lint/*) continue;; esac
    for lv in -O1 -O2 -O3; do
        "${OPT}" "${f}" "${lv}" -o "${TMP}/o.asm" >/dev/null 2>&1 || { echo "FAIL: v32opt ${lv} ${f}"; status=1; continue; }
        out="$(python3 "${HERE}/callee_save_lint.py" "${f}" "${TMP}/o.asm")" || { echo "FAIL: ${lv} ${f}"; echo "${out}"; status=1; }
        n=$((n + 1))
    done
done
[ "${status}" = 0 ] && echo "PASS: callee-save lint (${n} runs)"
exit "${status}"
