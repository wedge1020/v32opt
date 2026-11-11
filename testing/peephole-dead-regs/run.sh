#!/usr/bin/env bash
##
## run.sh - regression test: register-liveness soundness (doc/liveness-audit.md).
##
## Runs the differential harness (testing/differential/run.py) over the
## hand-written probes testing/differential/asm/live_*.asm: each is optimized
## with -fpeephole-dead-regs, -O2 and -O3, assembled, run in v32sim, and its
## whole RAM image (STRICT=1) must equal the unoptimized program's.
## Needs v32as and v32sim built next to this project (see
## testing/differential/README.md), unlike the pure-text tests.
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
fail() { echo "FAIL: $1"; status=1; }

[ -x "${OPT}" ] || { echo "FAIL: ${OPT} is not an executable"; exit 1; }

cd "${DIFF}" || exit 1
files=(asm/live_*.asm)
[ -e "${files[0]}" ] || { echo "FAIL: no asm/live_*.asm"; exit 1; }

V32OPT="${OPT}" WORK="${TMP}/work" STRICT=1 python3 run.py \
    --flags "-fpeephole-dead-regs" --flags "-O2" --flags "-O3" \
    "${files[@]}" > "${TMP}/out.txt" 2>&1
rc=$?

# one line per failing (program, configuration)
while IFS= read -r line; do
    echo "${line}"; status=1
done < <(grep -E '^(FAIL|ERROR)' "${TMP}/out.txt" | sed -E 's/  +/ /g' | cut -c1-160)
# a harness that stopped early (missing tools, crash) is a failure too
if [ "${rc}" != 0 ] && [ "${status}" = 0 ]; then
    fail "harness exited ${rc}: $(head -3 "${TMP}/out.txt" | tr '\n' ' ')"
fi

n="${#files[@]}"
if [ "${status}" = 0 ]; then
    echo "PASS: peephole-dead-regs liveness (${n} programs x 3 configurations)"
else
    echo "FAIL: peephole-dead-regs liveness (${n} programs, see above)"
fi
exit "${status}"
