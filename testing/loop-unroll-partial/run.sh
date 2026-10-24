#!/usr/bin/env bash
##
## run.sh - regression test: loop-unroll-partial on loop-unroll-partial.asm.
##
## part_* loops get a guarded main loop (K copies of the body, the original
## loop kept as the remainder) with the guard computed from the bound's kind;
## keep_* loops must be untouched. Needs no Vircon32 toolchain.
## Usage: ./run.sh [path/to/v32opt]
##
#########################################################################
OPT="${1:-../../bin/v32opt}"
HERE="$(cd "$(dirname "$0")" && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT
status=0
fail() { echo "FAIL: $1"; status=1; }

"${OPT}" "${HERE}/loop-unroll-partial.asm" -floop-unroll-partial -o "${TMP}/o.asm" >/dev/null || fail "v32opt failed"
fn() { awk -v f="__function_$2:" '$0 == f {on=1; next} on && /^__function_/ {exit} on {print}' "$1"; }

# guard text per kind of bound
guard() { fn "${TMP}/o.asm" "$1" | grep -A3 -E '^__v32opt_unroll_[0-9]+:' | tr -s ' ' | tr '\n' '|'; }
g="$(guard part_imm)";        case "${g}" in *"MOV R0, R4|"*"ILT R0, 97|"*"JF R0, __part_imm_start"*) ;; *) fail "part_imm guard: ${g}";; esac
g="$(guard part_reg)";        case "${g}" in *"MOV R0, R6|"*"ISUB R0, 3|"*"IGT R0, R4|"*) ;; *) fail "part_reg guard: ${g}";; esac
g="$(guard part_slot)";       case "${g}" in *"MOV R0, [BP+2]|"*"ISUB R0, 3|"*"IGT R0, R4|"*) ;; *) fail "part_slot guard: ${g}";; esac
g="$(guard part_down)";       case "${g}" in *"MOV R0, [BP+2]|"*"IADD R0, 3|"*"ILT R0, R4|"*) ;; *) fail "part_down guard: ${g}";; esac
g="$(guard part_step3_le)";   case "${g}" in *"MOV R0, [BP+2]|"*"ISUB R0, 9|"*"IGE R0, R4|"*) ;; *) fail "part_step3_le guard: ${g}";; esac

# four copies of the body (and of the step) in the main loop, one in the remainder
n="$(fn "${TMP}/o.asm" part_imm | grep -c 'IADD R5, R4')"
[ "${n}" = 5 ] || fail "part_imm: expected 5 copies of the body (4 + remainder), found ${n}"
# the original loop is still there as the remainder
fn "${TMP}/o.asm" part_imm | grep -q '^__part_imm_start:' || fail "part_imm: the remainder loop is gone"

# the factor is a tunable
"${OPT}" "${HERE}/loop-unroll-partial.asm" -floop-unroll-partial -floop-unroll-partial-factor=2 -o "${TMP}/o2.asm" >/dev/null || fail "v32opt failed (factor=2)"
n="$(fn "${TMP}/o2.asm" part_imm | grep -c 'IADD R5, R4')"
[ "${n}" = 3 ] || fail "part_imm with factor 2: expected 3 copies of the body (2 + remainder), found ${n}"
# and so is the size limit
"${OPT}" "${HERE}/loop-unroll-partial.asm" -floop-unroll-partial -floop-unroll-partial-max-body=1 -o "${TMP}/o3.asm" >/dev/null || fail "v32opt failed (max-body=1)"
fn "${TMP}/o3.asm" part_imm | grep -q 'v32opt_unroll_' && fail "max-body=1 should have left part_imm alone"

for f in keep_bound_register_written keep_slot_written keep_slot_escapes keep_call keep_scratch_live keep_ine keep_wrong_direction keep_big_body keep_counter_written; do
    a="$(fn "${HERE}/loop-unroll-partial.asm" "${f}" | grep -v '^;')"
    b="$(fn "${TMP}/o.asm" "${f}" | grep -v '^;')"
    [ "${a}" = "${b}" ] || fail "${f} was changed"
done
[ "${status}" = 0 ] && echo "PASS: loop-unroll-partial"
exit "${status}"
