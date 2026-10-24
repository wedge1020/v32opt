#!/usr/bin/env bash
##
## run.sh - regression test: loop-unroll on loop-unroll.asm.
##
## The loops of the unr_* functions must be unrolled (their start labels are
## gone and the trip count's worth of body copies is there); those of the
## keep_* functions must be untouched. Needs no Vircon32 toolchain.
## Usage: ./run.sh [path/to/v32opt]
##
#########################################################################
OPT="${1:-../../bin/v32opt}"
HERE="$(cd "$(dirname "$0")" && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT
status=0
fail() { echo "FAIL: $1"; status=1; }

"${OPT}" "${HERE}/loop-unroll.asm" -floop-unroll -o "${TMP}/o.asm" >/dev/null || fail "v32opt failed"
fn() { awk -v f="__function_$2:" '$0 == f {on=1; next} on && /^__function_/ {exit} on {print}' "$1"; }

for l in u1_start u2_start u3_start u4_outer u4_inner u5_start; do
    grep -q "^__${l}:" "${TMP}/o.asm" && fail "${l}: the loop was not unrolled"
done
for l in k1_start k2_start k3_start k4_start k5_start k6_start k7_start k8_start; do
    grep -q "^__${l}:" "${TMP}/o.asm" || fail "${l}: the loop was unrolled but must not be"
done

# unr_up: 4 copies of the store, with the counter folded into the address
n="$(fn "${TMP}/o.asm" unr_up | grep -c 'MOV \[R1\], R2')"
[ "${n}" = 4 ] || fail "unr_up: expected 4 body copies, found ${n}"
fn "${TMP}/o.asm" unr_up | grep -q 'JMP\|JF' && fail "unr_up: test or back edge left behind"
# unr_down: 10, 7, 4, 1 -> 4 copies
n="$(fn "${TMP}/o.asm" unr_down | grep -c 'IADD R5, ')"
[ "${n}" = 4 ] || fail "unr_down: expected 4 body copies, found ${n}"
fn "${TMP}/o.asm" unr_down | grep -q 'IADD R5, 10' || fail "unr_down: the counter was not replaced by its value"
# unr_continue_break: R0 is read after the loop, so it is set on every path
fn "${TMP}/o.asm" unr_continue_break | grep -q 'MOV R0, 1' || fail "unr_continue_break: R0 (live after the loop) is not set to the test result"
fn "${TMP}/o.asm" unr_continue_break | grep -q 'MOV R0, 0' || fail "unr_continue_break: R0 is not 0 after the normal exit"
# nested: 3 x 3 inner bodies
n="$(fn "${TMP}/o.asm" unr_nested | grep -c 'IADD R6, ')"
[ "${n}" = 9 ] || fail "unr_nested: expected 9 inner bodies, found ${n}"
for f in keep_counter_written keep_call keep_many_trips keep_register_bound keep_other_ref_to_start keep_jump_into_body keep_init_not_constant keep_init_across_branch; do
    a="$(fn "${HERE}/loop-unroll.asm" "${f}" | grep -v '^;')"
    b="$(fn "${TMP}/o.asm" "${f}" | grep -v '^;')"
    [ "${a}" = "${b}" ] || fail "${f} was changed"
done
[ "${status}" = 0 ] && echo "PASS: loop-unroll"
exit "${status}"
