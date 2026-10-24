#!/usr/bin/env bash
##
## run.sh - regression test: loop-invert on loop-invert.asm.
##
## Functions named inv_* must have every loop inverted (one new "JT Rc,body"
## per loop and no "JMP <loop start>" left), keep_* functions must be
## untouched. Needs no Vircon32 toolchain. Usage: ./run.sh [path/to/v32opt]
##
#########################################################################
OPT="${1:-../../bin/v32opt}"
HERE="$(cd "$(dirname "$0")" && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT
status=0
fail() { echo "FAIL: $1"; status=1; }

"${OPT}" "${HERE}/loop-invert.asm" -floop-invert -o "${TMP}/o.asm" >/dev/null || fail "v32opt failed"
fn() { awk -v f="__function_$2:" '$0 == f {on=1; next} on && /^__function_/ {exit} on {print}' "$1"; }

# expected number of inversions per function
expect() {
    local f="$1" n="$2" got
    got="$(fn "${TMP}/o.asm" "$f" | grep -c -E '^ *J[TF] R[0-9]+, [._A-Za-z0-9]*(v32opt_loop|body)')"
    [ "${got}" = "${n}" ] || fail "${f}: expected ${n} new back-edge JT, found ${got}"
}
expect inv_for 1
expect inv_while_continue_break 1
expect inv_nested 2
expect inv_first_condition_only 1
expect inv_existing_label 1
expect inv_exit_on_true 1
fn "${TMP}/o.asm" inv_exit_on_true | grep -q -E '^ *JF R0, [._A-Za-z0-9]*v32opt_loop' || fail "a test that exits on JT needs a JF back edge"
for f in keep_call_in_test keep_exit_mismatch keep_forward keep_long_test keep_dup_a keep_dup_b; do
    a="$(fn "${HERE}/loop-invert.asm" "${f}" | grep -v '^;')"
    b="$(fn "${TMP}/o.asm" "${f}" | grep -v '^;')"
    [ "${a}" = "${b}" ] || fail "${f} was changed"
done
# the continue in the while loop still jumps to the original top test
fn "${TMP}/o.asm" inv_while_continue_break | grep -q 'JT R0, __while_2_start' || fail "continue no longer reaches the loop's top test"
[ "${status}" = 0 ] && echo "PASS: loop-invert"
exit "${status}"
