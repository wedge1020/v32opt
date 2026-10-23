#!/usr/bin/env bash
##
## run.sh - regression test: the promote-leaf / promote-regs / promote-loops
##          passes on promote.asm.
##
## promote.asm holds one small function per scenario (see its comments).
## The script optimizes it with each pass on its own, in -d mode, and checks
## per function that the pass promoted what it should and left alone what it
## must: the address-taken, jump-into-loop and shared-exit cases (each of
## which the passes once got wrong), nested loops under a CALL, the
## return-register passthrough, parameters (never promoted), SP escape and a frame-less
## function. It also checks where the loads and stores land (preheader before
## the loop label, store after the exit label, nothing before the epilogue).
## Needs no Vircon32 toolchain. Usage: ./run.sh [path/to/v32opt]
##
#########################################################################
OPT="${1:-../../bin/v32opt}"
HERE="$(cd "$(dirname "$0")" && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT
status=0
fail() { echo "FAIL: $1"; status=1; }

# fn_text <asm> <function>: the function's lines
fn_text() { awk -v f="__function_$2:" -v e="__function_$2_return:" \
            '$0 == f {on=1} on {print} $0 == e {exit}' "$1"; }
# promoted <asm> <function> <pass-tag>: number of "PROMOTE-..." notes
promoted() { fn_text "$1" "$2" | grep -c "PROMOTE-$3"; }
# line_of <text> <regex>: first line number matching
line_of() { printf '%s\n' "$1" | grep -n -m1 -E "$2" | cut -d: -f1; }

for pass in loops regs leaf; do
    "${OPT}" "${HERE}/promote.asm" -fpromote-${pass} -d -o "${TMP}/${pass}.asm" || fail "v32opt failed (promote-${pass})"
done
P="${TMP}/loops.asm"

# ---- promote-loops ---------------------------------------------------------
[ "$(promoted "${P}" loop_basic LOOPS)" -ge 3 ] || fail "loops: loop_basic was not promoted"
T="$(fn_text "${P}" loop_basic)"
ld="$(line_of "${T}" 'preheader load of \[BP-1\]')"; lab="$(line_of "${T}" '^__for_1_start:')"
ex="$(line_of "${T}" '^__for_1_end:')"; st="$(line_of "${T}" 'flush R[0-9]+ back to \[BP-1\]')"
[ -n "${ld}" ] && [ -n "${lab}" ] && [ "${ld}" -lt "${lab}" ] || fail "loops: preheader load is not before the loop label"
[ -n "${st}" ] && [ -n "${ex}" ] && [ "${st}" -gt "${ex}" ] || fail "loops: exit store is not after the exit label"
# [BP-2] (the counter) is never read after the loop: no store back for it
printf '%s\n' "${T}" | grep -q 'flush R[0-9]* back to \[BP-2\]' && fail "loops: stored the loop counter back though nothing reads it"
T="$(fn_text "${P}" loop_temp)"
printf '%s\n' "${T}" | grep -q 'preheader load of \[BP-1\]' || fail "loops: loop_temp's [BP-1] needs its preheader load"
printf '%s\n' "${T}" | grep -q 'preheader load of \[BP-2\]' && fail "loops: loaded a scratch slot the loop always writes first"
printf '%s\n' "${T}" | grep -q 'no preheader load' || fail "loops: loop_temp's [BP-2] was not promoted"
printf '%s\n' "${T}" | grep -q 'flush R[0-9]* back to \[BP-2\]' && fail "loops: stored a scratch slot nobody reads"
printf '%s\n' "${T}" | grep -q 'flush R[0-9]* back to \[BP-1\]' || fail "loops: loop_temp's [BP-1] needs its exit store"
T="$(fn_text "${P}" reentered)"
printf '%s\n' "${T}" | grep -q 'flush R[0-9]* back to \[BP-1\]' || fail "loops: reentered: the inner loop's accumulator must be stored at its exit (the outer loop re-enters it)"
for fn in alias_before_loop goto_into_loop shared_exit; do
    [ "$(promoted "${P}" ${fn} LOOPS)" = 0 ] || fail "loops: ${fn} must not be promoted"
done
T="$(fn_text "${P}" outer_call)"
[ "$(promoted "${P}" outer_call LOOPS)" -ge 2 ] || fail "loops: the loop nested under a CALL was not promoted"
lab5="$(line_of "${T}" '^__for_5_start:')"; lab6="$(line_of "${T}" '^__for_6_start:')"
ld="$(line_of "${T}" 'preheader load')"
[ -n "${ld}" ] && [ "${ld}" -gt "${lab5}" ] && [ "${ld}" -lt "${lab6}" ] || fail "loops: outer_call's preheader load is not just before the inner loop"

# ---- promote-regs ----------------------------------------------------------
P="${TMP}/regs.asm"
[ "$(promoted "${P}" passthrough REGS)" -ge 1 ] || fail "regs: passthrough was not promoted"
fn_text "${P}" passthrough | grep -E 'load of .* into R[23] ' && fail "regs: promoted into R2/R3, which carry callee return values through"
fn_text "${P}" passthrough | grep -q 'later-segment load of \[BP-1\] into R[4-9]' || fail "regs: passthrough should have used a register above R3"
for fn in param_ro param_rw sp_escape naked alias_before_loop; do
    [ "$(promoted "${P}" ${fn} REGS)" = 0 ] || fail "regs: ${fn} must not be promoted"
done
# the frame is dead at the epilogue: no store of a promoted slot before it
for fn in loop_basic passthrough; do
    fn_text "${P}" ${fn} | grep -E '^    MOV \[BP' && fail "regs: ${fn} stores a slot back before the epilogue"
done

# ---- promote-leaf ----------------------------------------------------------
P="${TMP}/leaf.asm"
[ "$(promoted "${P}" loop_basic LEAF)" -ge 2 ] || fail "leaf: loop_basic was not promoted"
for fn in outer_call passthrough param_ro param_rw sp_escape naked alias_before_loop; do
    [ "$(promoted "${P}" ${fn} LEAF)" = 0 ] || fail "leaf: ${fn} must not be promoted"
done
fn_text "${P}" loop_basic | grep -E '^    MOV \[BP' && fail "leaf: loop_basic stores a slot back before the epilogue"

[ ${status} = 0 ] && echo "PASS: promote"
exit ${status}
