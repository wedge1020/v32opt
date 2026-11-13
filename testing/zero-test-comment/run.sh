#!/usr/bin/env bash
##
## run.sh - regression test: peephole-zero-test keeps the trailing comment of the jump it rewrites
## (zt.asm: "JT R1, __f_end ; SRC-NOTE keep me" becomes "JF R2, __f_end ; SRC-NOTE keep me").
## Usage: ./run.sh [path/to/v32opt]     (or V32OPT=path, default ../../bin/v32opt)
##
HERE="$(cd "$(dirname "$0")" && pwd)"
OPT="${1:-${V32OPT:-${HERE}/../../bin/v32opt}}"
case "${OPT}" in /*) ;; *) OPT="$(pwd)/${OPT}" ;; esac
TMP="$(mktemp -d)"; trap 'rm -rf "${TMP}"' EXIT
status=0; n=0
"${OPT}" "${HERE}/zt.asm" -fpeephole-zero-test -o "${TMP}/o.asm" >/dev/null 2>&1 || { echo "FAIL: v32opt failed"; exit 1; }
n=$((n + 1))
grep -qE '^[[:space:]]+J[TF][[:space:]]+R2, __f_end ; SRC-NOTE keep me$' "${TMP}/o.asm" || { echo "FAIL: the rewritten jump lost its comment: $(grep -E 'J[TF]' "${TMP}/o.asm" | grep -v '^;' | tr -s ' ')"; status=1; }
n=$((n + 1))
grep -qE '^[[:space:]]+IEQ' "${TMP}/o.asm" && { echo "FAIL: the test was not rewritten (IEQ still there)"; status=1; }
[ "${status}" = 0 ] && echo "PASS: zero-test-comment (${n} checks)" || echo "FAIL: zero-test-comment"
exit "${status}"
