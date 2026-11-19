#!/usr/bin/env bash
##
## run.sh - regression test: peephole-copies must not paste a LABEL into an integer ALU operand
## (propagate_imm in src/peephole/dead_regs.c).
##
##     MOV R2, __tic80_palette / IADD R1, R2   must NOT become   IADD R1, __tic80_palette
##
## v32as takes a label only in MOV and in a memory address; IADD ISUB IMUL AND OR XOR SHL IMIN IMAX
## and the integer comparisons want a register or a literal integer ("IADD second operand must be a
## register or an integer"). label.asm has one function per case:
##   fire_*  rewritten: a label into MOV, a label as "[label]", a %define with an integer value
##           into an ALU operand (the preprocessor makes it a literal)
##   keep_*  unchanged: a label feeding IADD ISUB AND IMIN ILT
## With "assemble" (stock DevTools or v32as) in PATH, the optimized file must also assemble.
## TEETH: before the fix keep_* are all rewritten ("iadd r0, __lbl_arr") and assemble fails.
## Usage: ./run.sh [path/to/v32opt]     (or V32OPT=path, default ../../bin/v32opt)
##
#########################################################################
HERE="$(cd "$(dirname "$0")" && pwd)"
OPT="${1:-${V32OPT:-${HERE}/../../bin/v32opt}}"
case "${OPT}" in /*) ;; *) OPT="$(pwd)/${OPT}" ;; esac
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT
status=0
fail() { echo "FAIL: $1"; status=1; }
[ -x "${OPT}" ] || { echo "FAIL: ${OPT} is not an executable"; exit 1; }

fn() {
    awk -v f="__function_$2:" '$0 == f {on=1; next} on && /^__function_/ {exit} on && !/^[ \t]*;/ {print}' "$1" 2>/dev/null |
        tr 'A-Z' 'a-z' | sed -e 's/^[ \t]*//' -e 's/[ \t]*$//' -e 's/[ \t][ \t]*/ /g' -e '/^$/d'
}
A="${HERE}/label.asm"
"${OPT}" "${A}" -fpeephole-copies -o "${TMP}/a.asm" >/dev/null 2>&1 || fail "label.asm: v32opt failed"

expect() {
    local got
    got="$(fn "${TMP}/a.asm" "$1" | tr '\n' '|')"
    [ "${got}" = "$2|" ] || fail "$1: expected '$2', got '${got%|}'"
}
expect fire_mov           'mov r0, __lbl_arr|ret'
expect fire_load          'mov r0, [__lbl_arr]|ret'
expect fire_define_alu    'mov r0, [200]|iadd r0, global_x|ret'
for f in keep_iadd keep_isub keep_and keep_imin keep_ilt; do
    [ -n "$(fn "${A}" "${f}")" ] || { fail "${f}: not in label.asm"; continue; }
    [ "$(fn "${A}" "${f}")" = "$(fn "${TMP}/a.asm" "${f}")" ] || fail "${f} was changed: $(fn "${TMP}/a.asm" "${f}" | tr '\n' '|')"
done

# the whole pipeline's output must not hold a label as an integer-ALU operand either
for fl in "-O1" "-O2" "-O3"; do
    "${OPT}" "${A}" ${fl} -o "${TMP}/o.asm" >/dev/null 2>&1 || { fail "${fl}: v32opt failed"; continue; }
    if grep -qiE '^[ \t]*(iadd|isub|imul|and|or|xor|shl|imin|imax|ilt|ile|igt|ige|ieq|ine)[ \t]+[a-z0-9]+,[ \t]*__lbl_arr\b' "${TMP}/o.asm"; then
        fail "${fl}: a label is an integer-ALU operand"
    fi
    if command -v assemble >/dev/null 2>&1; then
        assemble "${TMP}/o.asm" -o "${TMP}/o.vbin" >"${TMP}/as.txt" 2>&1 || fail "${fl}: output does not assemble: $(head -1 "${TMP}/as.txt")"
    fi
done
[ "${status}" = 0 ] && echo "peephole-copies-label: PASS"
exit "${status}"
