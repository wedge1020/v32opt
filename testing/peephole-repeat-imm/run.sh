#!/usr/bin/env bash
##
## run.sh - regression test: peephole-repeat-imm (-O3, src/peephole/repeat_imm.c).
##
## "MOV Rd, imm" is deleted when Rd provably holds imm already, across labels that nothing
## references (the leftovers of full unrolling). repeat_imm.asm has one function per case:
##   fire_*  the MOV goes (the '-d' comment "[DEBUG peephole-repeat-imm]" is in the function,
##           as many times as the table below says)
##   keep_*  it stays and the function is unchanged: a write of the register (ALU, POP, load,
##           IN, another immediate), a CALL, a label that is a jump target / a CALL target /
##           mentioned in "mov Rx, label" / mentioned only in a data directive, a directive
##           in between, a volatile (';@v') instruction as the MOV or in between
## repeat_imm_blk.asm: MOVS, SETS and CMPS write R11-R13 without naming them.
## The pass is part of -O3 only. Every program is run in v32sim against its unoptimized
## self (RAM image, STRICT=1), which needs v32as and v32sim built next to this project.
## known_peephole_movs.asm (fixed in 1aacb50) is run by testing/string-ops/run.sh.
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

# body of function $2 of file $1 without comments or '-d' marks
fn() {
    awk -v f="__function_$2:" '$0 == f {on=1; next} on && /^__function_/ && $0 !~ /_return:$/ {exit} on && !/^[ \t]*;/ {print}' "$1" 2>/dev/null |
        sed -e 's/^[ \t]*//' -e 's/[ \t]*$//'
}
# number of deletions marked in function $2 of file $1
hits() {
    awk -v f="__function_$2:" '$0 == f {on=1; next} on && /^__function_/ && $0 !~ /_return:$/ {exit}
        on && /DEBUG peephole-repeat-imm/ {n++} END {print n + 0}' "$1" 2>/dev/null
}
tally() { sed -n 's/^peephole-repeat-imm:\([0-9]*\)$/\1/p' "$1"; }

A="${HERE}/repeat_imm.asm"
"${OPT}" "${A}" -fpeephole-repeat-imm -d -o "${TMP}/a.asm" >/dev/null 2>&1 || fail "repeat_imm.asm: v32opt failed"
"${OPT}" "${A}" -fpeephole-repeat-imm -t -o "${TMP}/at.asm" > "${TMP}/a.t" 2>/dev/null

# ---- fires: function and number of deleted MOVs
total=0
for e in fire_straight:1 fire_label:1 fire_label_chain:1 fire_symbol:1 fire_two_regs:2 \
         fire_three:2 fire_jt:1 fire_float:1 fire_other_reg_write:1; do
    f="${e%%:*}"; want="${e##*:}"
    n="$(hits "${TMP}/a.asm" "${f}")"
    ncases=$((ncases + 1)); total=$((total + want))
    [ "${n}" = "${want}" ] || fail "${f}: expected ${want} repeated MOV(s) deleted, found ${n}"
done
ncases=$((ncases + 1))
[ "$(tally "${TMP}/a.t")" = "${total}" ] || fail "-t tallies '$(tally "${TMP}/a.t")' deletions, expected ${total}"

# ---- stays: no deletion, function unchanged
for f in keep_other_text keep_write_alu keep_write_pop keep_write_load keep_write_in keep_write_imm_other \
         keep_jump_target keep_back_edge keep_call_target_a keep_call_target_b keep_mov_label_a \
         keep_mov_label_b_holder keep_data_label_a keep_data_label_holder keep_call \
         keep_volatile_dst keep_volatile_between keep_directive keep_after_ret; do
    ncases=$((ncases + 1))
    [ -n "$(fn "${A}" "${f}")" ] || { fail "${f}: not in repeat_imm.asm"; continue; }
    [ "$(hits "${TMP}/a.asm" "${f}")" = 0 ] || fail "${f}: a MOV was deleted"
    [ "$(fn "${A}" "${f}")" = "$(fn "${TMP}/a.asm" "${f}")" ] || fail "${f} was changed"
done

# ---- MOVS / SETS / CMPS
B="${HERE}/repeat_imm_blk.asm"
"${OPT}" "${B}" -fpeephole-repeat-imm -d -o "${TMP}/b.asm" >/dev/null 2>&1 || fail "repeat_imm_blk.asm: v32opt failed"
for f in keep_movs keep_sets keep_cmps; do
    ncases=$((ncases + 1))
    [ "$(hits "${TMP}/b.asm" "${f}")" = 0 ] || fail "${f}: a MOV after the block operation was deleted"
    [ "$(fn "${B}" "${f}")" = "$(fn "${TMP}/b.asm" "${f}")" ] || fail "${f} was changed"
done

# ---- --trigger-max: one slot per deleted MOV
"${OPT}" "${A}" -fpeephole-repeat-imm --trigger-max=0 -d -o "${TMP}/c0.asm" >/dev/null 2>&1
"${OPT}" "${A}" -fpeephole-repeat-imm --trigger-max=1 -d -o "${TMP}/c1.asm" >/dev/null 2>&1
ncases=$((ncases + 2))
[ "$(grep -c 'DEBUG peephole-repeat-imm' "${TMP}/c0.asm")" = 0 ] || fail "--trigger-max=0 still deleted a MOV"
[ "$(grep -c 'DEBUG peephole-repeat-imm' "${TMP}/c1.asm")" = 1 ] || fail "--trigger-max=1 did not delete exactly one MOV"

# ---- part of -O3 and of nothing below it: the unrolled clear16 (testing/frame-cleanup)
U="${HERE}/../frame-cleanup/clear16.asm"
"${OPT}" "${U}" -O3 -t -o "${TMP}/u3.asm" > "${TMP}/u3.t" 2>/dev/null
"${OPT}" "${U}" -O3 -fno-peephole-repeat-imm -t -o "${TMP}/un.asm" > "${TMP}/un.t" 2>/dev/null
"${OPT}" "${U}" -O2 -t -o "${TMP}/u2.asm" > "${TMP}/u2.t" 2>/dev/null
"${OPT}" "${U}" -Os -t -o "${TMP}/us.asm" > "${TMP}/us.t" 2>/dev/null
ncases=$((ncases + 4))
n="$(tally "${TMP}/u3.t")"
[ -n "${n}" ] && [ "${n}" -ge 15 ] || fail "clear16 -O3: expected the 15 repeated 'mov R0, 0' of the unrolled copies deleted, tally '${n}'"
[ -z "$(tally "${TMP}/un.t")" ] || fail "-O3 -fno-peephole-repeat-imm still runs the pass"
[ -z "$(tally "${TMP}/u2.t")" ] || fail "-O2 runs peephole-repeat-imm"
[ -z "$(tally "${TMP}/us.t")" ] || fail "-Os runs peephole-repeat-imm"

# ---- the optimized programs agree with the unoptimized ones in the simulator
cd "${DIFF}" || exit 1
V32OPT="${OPT}" WORK="${TMP}/work" STRICT=1 python3 run.py \
    --flags "-fpeephole-repeat-imm" --flags "-O2" --flags "-O3" "${A}" > "${TMP}/diff.txt" 2>&1
rc=$?
V32OPT="${OPT}" WORK="${TMP}/work2" STRICT=1 python3 run.py \
    --flags "-fpeephole-repeat-imm" "${B}" >> "${TMP}/diff.txt" 2>&1
[ "${rc}" = 0 ] && rc=$?
ncases=$((ncases + 4))
while IFS= read -r line; do
    echo "${line}"; status=1
done < <(grep -E '^(FAIL|ERROR)' "${TMP}/diff.txt" | sed -E 's/  +/ /g' | cut -c1-160)
if [ "${rc}" != 0 ] && [ "${status}" = 0 ]; then
    fail "harness exited ${rc}: $(head -3 "${TMP}/diff.txt" | tr '\n' ' ')"
fi

if [ "${status}" = 0 ]; then
    echo "PASS: peephole-repeat-imm (${ncases} checks)"
else
    echo "FAIL: peephole-repeat-imm (see above)"
fi
exit "${status}"
