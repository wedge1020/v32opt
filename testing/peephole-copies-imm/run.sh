#!/usr/bin/env bash
##
## run.sh - regression test: immediate propagation in peephole-copies (propagate_imm in
## src/peephole/dead_regs.c).
##
##     MOV Rt, imm / OP Rd, Rt      ->  OP Rd, imm
##     MOV Rt, imm / MOV Rd, [Rt]   ->  MOV Rd, [imm]
##     MOV Rt, imm / MOV [Rt], Rs   ->  MOV [imm], Rs
##
## when Rt is read by at most two instructions, all of those forms, and dead after the last;
## the MOV then goes. copies_imm.asm has one function per case:
##   fire_*  the exact rewrite below
##   keep_*  unchanged: a third use, a float immediate, an operation with no immediate form
##           (FADD), a use across a label or a jump, Rt live afterwards, a CALL between, an
##           [Rt+k] use, Rt as the stored VALUE, Rt as a destination, a volatile ';@v' MOV or use
## Every program is also run in v32sim against its unoptimized self (RAM image, STRICT=1),
## which needs v32as and v32sim built next to this project.
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

# body of function $2 of file $1 without comments, one line per instruction, lower case, blanks squeezed
fn() {
    awk -v f="__function_$2:" '$0 == f {on=1; next} on && /^__function_/ && $0 !~ /_return:$/ {exit} on && !/^[ \t]*;/ {print}' "$1" 2>/dev/null |
        tr 'A-Z' 'a-z' | sed -e 's/^[ \t]*//' -e 's/[ \t]*$//' -e 's/[ \t][ \t]*/ /g' -e '/^$/d'
}

A="${HERE}/copies_imm.asm"
"${OPT}" "${A}" -fpeephole-copies -o "${TMP}/a.asm" >/dev/null 2>&1 || fail "copies_imm.asm: v32opt failed"

# expect FUNCTION 'line1|line2|...' : the whole function, as ' | '-separated instructions
expect() {
    ncases=$((ncases + 1))
    local got
    got="$(fn "${TMP}/a.asm" "$1" | tr '\n' '|')"
    [ "${got}" = "$2|" ] || fail "$1: expected '$2', got '${got%|}'"
}
expect fire_src          'mov r0, [200]|iadd r0, 5|ret'
expect fire_src_two      'mov r0, [200]|mov r2, [201]|iadd r0, 5|imul r2, 5|iadd r0, r2|ret'
expect fire_cmp          'mov r0, [200]|ilt r0, 5|ret'
expect fire_load         'mov r0, [global_x]|ret'
expect fire_load_num     'mov r0, [301]|ret'
expect fire_store        'mov r0, [200]|mov [302], r0|ret'
expect fire_store_load   'mov r0, [200]|mov [303], r0|mov r0, [303]|iadd r0, 1|ret'

for f in keep_third_use keep_float keep_fadd keep_label keep_jump keep_live_after keep_call_between \
         keep_offset keep_store_value keep_dst_use keep_volatile_use keep_volatile_mov; do
    ncases=$((ncases + 1))
    [ -n "$(fn "${A}" "${f}")" ] || { fail "${f}: not in copies_imm.asm"; continue; }
    [ "$(fn "${A}" "${f}")" = "$(fn "${TMP}/a.asm" "${f}")" ] || fail "${f} was changed"
done

# ---- --trigger-max: one slot per rewrite
"${OPT}" "${A}" -fpeephole-copies --trigger-max=0 -o "${TMP}/c0.asm" >/dev/null 2>&1
ncases=$((ncases + 1))
cmp -s <(grep -v '^[ \t]*;' "${A}") <(grep -v '^[ \t]*;' "${TMP}/c0.asm") || fail "--trigger-max=0 still rewrote something"

# ---- the optimized programs agree with the unoptimized one in the simulator
cd "${DIFF}" || exit 1
V32OPT="${OPT}" WORK="${TMP}/work" STRICT=1 python3 run.py \
    --flags "-fpeephole-copies" --flags "-O2" --flags "-O3" "${A}" > "${TMP}/diff.txt" 2>&1
rc=$?
ncases=$((ncases + 3))
while IFS= read -r line; do
    echo "${line}"; status=1
done < <(grep -E '^(FAIL|ERROR)' "${TMP}/diff.txt" | sed -E 's/  +/ /g' | cut -c1-160)
if [ "${rc}" != 0 ] && [ "${status}" = 0 ]; then
    fail "harness exited ${rc}: $(head -3 "${TMP}/diff.txt" | tr '\n' ' ')"
fi

if [ "${status}" = 0 ]; then
    echo "PASS: peephole-copies-imm (${ncases} checks)"
else
    echo "FAIL: peephole-copies-imm (see above)"
fi
exit "${status}"
