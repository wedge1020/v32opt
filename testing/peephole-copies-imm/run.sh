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
##   keep_*  unchanged: a third use, a float immediate, an integer literal feeding a float
##           operation (FADD), a use across a label or a jump, Rt live afterwards, a CALL between, an
##           [Rt+k] use, Rt as the stored VALUE, Rt as a destination, a volatile ';@v' MOV or use
## (an integer literal feeding a float operation, FADD, is refused too: see copies_imm_float.asm)
## copies_imm_float.asm: the float operations (commit 6879a93). The twelve float mnemonics FADD FSUB
##   FMUL FDIV FMIN FMAX FEQ FNE FGT FGE FLT FLE take the literal in two ways, by the word the
##   register held and not by the spelling: a decimal float literal ("MOV R1, 1.5") is copied as
##   written; a hex word is respelled as the decimal float that reads back to the same bits
##   (0x40200000 -> 2.5, 0xC0200000 -> -2.5, 0x3DCCCCCD -> 0.1; v32as reads an integer literal in
##   a float operation as a number to convert, so FADD R0, 0x40200000 would add 1075838976.0).
##   Refused, and unchanged: an integer literal (MOV R1, 5 / FADD R0, R1: the word 5 is a denormal),
##   a float literal feeding an integer operation, a comparison or an address, a NaN, an infinity, a
##   denormal, a symbol, a third use, Rt live afterwards. The twelve mnemonics and the output text
##   are checked, and the program is run in v32sim against its unoptimized self.
## Every program is also run in v32sim against its unoptimized self (RAM image, STRICT=1),
## which needs v32as and v32sim built next to this project.
## TEETH: the optimizer before 6879a93 leaves all 29 fire_* functions of copies_imm_float.asm as they
## are (29 FAILs; the 12 keep_* rows and the simulator runs pass, so the refusals are guards); the
## original optimizer has no immediate propagation at all.
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

# ---- copies_imm_float.asm: the float operations
F="${HERE}/copies_imm_float.asm"
"${OPT}" "${F}" -fpeephole-copies -o "${TMP}/f.asm" >/dev/null 2>&1 || fail "copies_imm_float.asm: v32opt failed"
expectf() {
    ncases=$((ncases + 1))
    local got
    got="$(fn "${TMP}/f.asm" "$1" | tr '\n' '|')"
    [ "${got}" = "$2|" ] || fail "$1: expected '$2', got '${got%|}'"
}
for m in fadd fsub fmul fdiv fmin fmax feq fne fgt fge flt fle; do
    expectf "fire_dec_${m}" "mov r0, [200]|${m} r0, 1.5|ret"
    expectf "fire_hex_${m}" "mov r0, [200]|${m} r0, 2.5|ret"
done
expectf fire_hexneg   'mov r0, [200]|fadd r0, -2.5|ret'
expectf fire_hexfrac  'mov r0, [200]|fmul r0, 0.1|ret'
expectf fire_decneg   'mov r0, [200]|fsub r0, -0.5|ret'
expectf fire_dec_two  'mov r0, [200]|mov r2, [200]|fadd r0, 1.5|fmul r2, 1.5|fadd r0, r2|ret'
expectf fire_mov_float 'mov r0, 2.5|ret'
for f in keep_int_into_float keep_int_into_fmul keep_float_into_int keep_float_into_cmp keep_float_address \
         keep_nan keep_nan_neg keep_inf keep_denormal keep_symbol keep_third_use keep_live_after; do
    ncases=$((ncases + 1))
    [ -n "$(fn "${F}" "${f}")" ] || { fail "${f}: not in copies_imm_float.asm"; continue; }
    [ "$(fn "${F}" "${f}")" = "$(fn "${TMP}/f.asm" "${f}")" ] || fail "${f} was changed"
done
"${OPT}" "${F}" -fpeephole-copies --trigger-max=0 -o "${TMP}/f0.asm" >/dev/null 2>&1
ncases=$((ncases + 1))
cmp -s <(grep -v '^[ \t]*;' "${F}") <(grep -v '^[ \t]*;' "${TMP}/f0.asm") || fail "copies_imm_float.asm: --trigger-max=0 still rewrote something"

# ---- --trigger-max: one slot per rewrite
"${OPT}" "${A}" -fpeephole-copies --trigger-max=0 -o "${TMP}/c0.asm" >/dev/null 2>&1
ncases=$((ncases + 1))
cmp -s <(grep -v '^[ \t]*;' "${A}") <(grep -v '^[ \t]*;' "${TMP}/c0.asm") || fail "--trigger-max=0 still rewrote something"

# ---- the optimized programs agree with the unoptimized one in the simulator
cd "${DIFF}" || exit 1
V32OPT="${OPT}" WORK="${TMP}/work" STRICT=1 python3 run.py \
    --flags "-fpeephole-copies" --flags "-O2" --flags "-O3" "${A}" "${F}" > "${TMP}/diff.txt" 2>&1
rc=$?
ncases=$((ncases + 3 + 4))
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
