#!/usr/bin/env bash
##
## run.sh - regression test: loop-strength (pointer strength reduction, -O3).
##
## Inputs (all assembly, so no compiler is needed):
##   psr.asm, psr_hints.asm, psr_stock.asm   psr.c compiled once by
##       v32cc --standard --keep-data --data-base 0 -w -I ../differential psr.c
##       (psr_hints.asm: with --emit-hints, so the ';@hints' header and the
##       frame-escape lines are there; psr_stock.asm: the stock compiler on
##       the stock-dialect copy of psr.c, which is what
##       COMPILER=stock python3 ../differential/run.py leaves as psr.base.asm)
##   psr_r.asm       hand-written, in compiler shape: ok_unit fires, the a*
##       functions are the same loop with exactly one reason to refuse
## In every f_* function the pass must fire (the '-d' comment LOOP-STRENGTH: is
## in the function), in every r*_/a* function it must not. -O3 -fno-loop-strength
## leaves no pointer step; a strength-reduced loop is still partially unrolled
## and inverted at -O3; the pass is off at -O2 and -Os. Every input is then run in
## v32sim at -O2, -O3 and -O3 -fno-loop-strength and its whole RAM image must
## equal the unoptimized program's (testing/differential/run.py), which needs
## v32as and v32sim built next to this project.
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

# number of '-d' LOOP-STRENGTH comments in function $2 of file $1
# (the function runs to the next __function_ label that is not its own _return)
ls_in() {
    awk -v f="__function_$2:" '
        $0 == f { on = 1; next }
        on && /^__function_/ && $0 !~ /_return:$/ { exit }
        on && /LOOP-STRENGTH:/ { n++ }
        END { print n + 0 }' "$1" 2>/dev/null
}
# body of function $2 of file $1
fn() { awk -v f="__function_$2:" '$0 == f {on=1; next} on && /^__function_/ && $0 !~ /_return:$/ {exit} on {print}' "$1" 2>/dev/null; }
# the pass's tally in '-t' output
tally() { sed -n 's/^loop-strength:\([0-9]*\)$/\1/p' "$1"; }

# expect NAME FILE FUNCTION COUNT
expect() {
    local n
    n="$(ls_in "$2" "$3")"
    ncases=$((ncases + 1))
    [ "${n}" = "$4" ] || fail "$1 ${3}: expected $4 LOOP-STRENGTH rewrite(s), found ${n}"
}

for v in psr psr_hints psr_stock psr_r; do
    "${OPT}" "${HERE}/${v}.asm" -O3 -d -o "${TMP}/${v}.o3.asm" >/dev/null 2>&1 || fail "${v}: v32opt -O3 failed"
    "${OPT}" "${HERE}/${v}.asm" -O3 -t -o "${TMP}/${v}.o3t.asm" >"${TMP}/${v}.t" 2>/dev/null
    "${OPT}" "${HERE}/${v}.asm" -O3 -fno-loop-strength -d -o "${TMP}/${v}.no.asm" >/dev/null 2>&1 || fail "${v}: v32opt -O3 -fno-loop-strength failed"
    "${OPT}" "${HERE}/${v}.asm" -O3 -fno-loop-strength -t -o "${TMP}/${v}.not.asm" >"${TMP}/${v}.no.t" 2>/dev/null
    "${OPT}" "${HERE}/${v}.asm" -O2 -d -o "${TMP}/${v}.o2.asm" >/dev/null 2>&1
    "${OPT}" "${HERE}/${v}.asm" -Os -d -o "${TMP}/${v}.os.asm" >/dev/null 2>&1
done

# ---- the forms: one function each (f_two has two arrays, so two pointers)
for v in psr psr_hints psr_stock; do
    for f in f_unit f_value f_scaled f_down f_step2 f_offset; do
        expect "${v}" "${TMP}/${v}.o3.asm" "${f}" 1
    done
    expect "${v}" "${TMP}/${v}.o3.asm" f_two 2
done
# the frame form (LEA [BP+off]) needs the frame-escape lines of --emit-hints
expect psr_hints "${TMP}/psr_hints.o3.asm" f_frame 1
expect psr "${TMP}/psr.o3.asm" f_frame 0
expect psr_stock "${TMP}/psr_stock.o3.asm" f_frame 0
expect psr_r "${TMP}/psr_r.o3.asm" ok_unit 1

# ---- refusals: the loop is left alone (no pointer register, no preheader)
for v in psr psr_hints psr_stock; do
    for f in r1_shape r2_call r3_counter r4_goto r8_base; do
        expect "${v}" "${TMP}/${v}.o3.asm" "${f}" 0
    done
done
# psr_r.asm: a2_call CALL in the body, a2_dir directive in the body, a2_vol ';@v' inside a
# group, a3_bp BP written in the loop, a5_cold ';@loop cold', a6_ru the scaled group's
# temporary is read later, a7_k0 scale 0, a8_rbri the base register is the counter,
# a8_rbw the base register is written in the loop
"${OPT}" "${HERE}/psr_r.asm" -floop-strength -d -o "${TMP}/psr_r.solo.asm" >/dev/null 2>&1 || fail "psr_r: v32opt -floop-strength failed"
for f in a2_call a2_dir a2_vol a3_bp a5_cold a6_ru a7_k0 a8_rbri a8_rbw; do
    # on its own the pass sees the loops as written ...
    expect "psr_r -floop-strength" "${TMP}/psr_r.solo.asm" "${f}" 0
    # ... at -O3 an earlier pass reshapes a3_bp (IADD BP, 1 / ISUB BP, 1 cancel), so the
    # loop is legitimately rewritten there
    [ "${f}" = a3_bp ] || expect psr_r "${TMP}/psr_r.o3.asm" "${f}" 0
done
expect "psr_r -floop-strength" "${TMP}/psr_r.solo.asm" ok_unit 1

# ---- the tally (one trigger slot per rewritten loop: f_two has two pointers, hence two
# comments for one loop; psr_hints.asm also has loops in main; psr_r.asm: ok_unit and a3_bp, see
# above) and the pass being off elsewhere
for e in psr:7 psr_hints:10 psr_stock:7 psr_r:2; do
    v="${e%%:*}"
    t="$(tally "${TMP}/${v}.t")"
    ncases=$((ncases + 1))
    [ "${t}" = "${e##*:}" ] || fail "${v}: -O3 -t tallies '${t}' loop(s), expected ${e##*:}"
    [ -z "$(tally "${TMP}/${v}.no.t")" ] || fail "${v}: -fno-loop-strength still tallies the pass"
    grep -q 'LOOP-STRENGTH:' "${TMP}/${v}.no.asm" 2>/dev/null && fail "${v}: -O3 -fno-loop-strength left a pointer register"
    grep -q 'LOOP-STRENGTH:' "${TMP}/${v}.o2.asm" 2>/dev/null && fail "${v}: -O2 ran loop-strength"
    grep -q 'LOOP-STRENGTH:' "${TMP}/${v}.os.asm" 2>/dev/null && fail "${v}: -Os ran loop-strength"
done

# ---- --trigger-max: one slot per loop; 0 leaves every loop alone
"${OPT}" "${HERE}/psr_r.asm" -floop-strength --trigger-max=0 -d -o "${TMP}/cap0.asm" >/dev/null 2>&1
ncases=$((ncases + 1))
[ "$(ls_in "${TMP}/cap0.asm" ok_unit)" = 0 ] || fail "--trigger-max=0 did not stop loop-strength"

# ---- a strength-reduced loop is still unrolled and inverted at -O3
# f_unit: the pointer step is repeated in every one of the 4 copies of the body, the
# copies' labels are there and so is the inverted (do-while) main loop
for v in psr psr_hints psr_stock; do
    b="$(fn "${TMP}/${v}.o3.asm" f_unit)"
    ncases=$((ncases + 1))
    n="$(printf '%s\n' "${b}" | grep -c '^ *IADD R[0-9]*, 1$')"
    [ "${n}" -ge 4 ] || fail "${v} f_unit: expected the pointer step in 4 unrolled copies, found ${n}"
    printf '%s\n' "${b}" | grep -q 'continue_p4:' || fail "${v} f_unit: not partially unrolled (no 4th copy)"
    printf '%s\n' "${b}" | grep -q '^__v32opt_loop_[0-9]*:' || fail "${v} f_unit: not inverted (no __v32opt_loop label)"
    b0="$(fn "${TMP}/${v}.no.asm" f_unit)"
    printf '%s\n' "${b0}" | grep -q 'continue_p4:' || fail "${v} f_unit -fno-loop-strength: the control is not unrolled either"
done

# ---- the optimized program agrees with the unoptimized one in the simulator
files=("${HERE}/psr.asm" "${HERE}/psr_hints.asm" "${HERE}/psr_stock.asm" "${HERE}/psr_r.asm")
cd "${DIFF}" || exit 1
V32OPT="${OPT}" WORK="${TMP}/work" STRICT=1 python3 run.py \
    --flags "-O2" --flags "-O3" --flags "-O3 -fno-loop-strength" \
    "${files[@]}" > "${TMP}/diff.txt" 2>&1
rc=$?
ncases=$((ncases + ${#files[@]} * 3))
while IFS= read -r line; do
    echo "${line}"; status=1
done < <(grep -E '^(FAIL|ERROR)' "${TMP}/diff.txt" | sed -E 's/  +/ /g' | cut -c1-160)
if [ "${rc}" != 0 ] && [ "${status}" = 0 ]; then
    fail "harness exited ${rc}: $(head -3 "${TMP}/diff.txt" | tr '\n' ' ')"
fi

if [ "${status}" = 0 ]; then
    echo "PASS: loop-strength (${ncases} checks)"
else
    echo "FAIL: loop-strength (see above)"
fi
exit "${status}"
