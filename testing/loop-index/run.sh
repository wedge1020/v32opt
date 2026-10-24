#!/usr/bin/env bash
##
## run.sh - regression test: loop-index on loop-index.asm.
##
## fold_* functions get "IADD R4, global_a" before the loop, their bound moved
## by the base (64 -> 164) and no "IADD Rt, R4" pair left; keep_* functions
## must be untouched. Needs no Vircon32 toolchain.
## Usage: ./run.sh [path/to/v32opt]
##
#########################################################################
OPT="${1:-../../bin/v32opt}"
HERE="$(cd "$(dirname "$0")" && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT
status=0
fail() { echo "FAIL: $1"; status=1; }

"${OPT}" "${HERE}/loop-index.asm" -floop-index -o "${TMP}/o.asm" >/dev/null || fail "v32opt failed"
fn() { awk -v f="__function_$2:" '$0 == f {on=1; next} on && /^__function_/ {exit} on {print}' "$1"; }

for f in fold_one fold_two_uses fold_early_exit_dead; do
    T="$(fn "${TMP}/o.asm" "${f}")"
    printf '%s\n' "${T}" | grep -q 'IADD R4, global_a' || fail "${f}: no IADD R4, global_a before the loop"
    printf '%s\n' "${T}" | grep -q 'ILT R0, 164' || fail "${f}: the bound was not moved by the base"
    printf '%s\n' "${T}" | grep -q 'IADD R1, R4' && fail "${f}: an address pair is left"
    printf '%s\n' "${T}" | grep -q 'MOV R1, R4' || fail "${f}: the pair was not turned into MOV R1, R4"
done
n="$(fn "${TMP}/o.asm" fold_two_uses | grep -c 'MOV R1, R4')"
[ "${n}" = 2 ] || fail "fold_two_uses: expected 2 folded uses, found ${n}"
for f in keep_value_use keep_two_bases keep_live_after keep_call keep_register_bound keep_exit_target_uses_counter keep_no_pair; do
    a="$(fn "${HERE}/loop-index.asm" "${f}" | grep -v '^;')"
    b="$(fn "${TMP}/o.asm" "${f}" | grep -v '^;')"
    [ "${a}" = "${b}" ] || fail "${f} was changed"
done
[ "${status}" = 0 ] && echo "PASS: loop-index"
exit "${status}"
