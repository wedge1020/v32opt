#!/usr/bin/env bash
##
## run.sh - regression test: two register bugs found on v32sim's oracle with v32lua 20261124-dev output.
##
##   savepair      liveness: a callee that saves an argument register around a call ("PUSH R1 / CALL g /
##                 POP R1") and reads it after the POP takes it as an input. v32opt 20261124-dev and earlier
##                 stopped at the POP, so the caller's "MOV R1, 7" was deleted as dead (peephole-dead-regs).
##                 At -O3 / -Os __f is inlined and the 7 lands in R0 directly; either way a 7 must still be loaded.
##                 v32lua's __table_index_miss has this shape: special/13_metatables, tables/19 and 20 failed.
##   promote_pop   promote-loops: a POP into a register is a write of it unless it gives back what the matching
##                 PUSH saved. peephole-copies turns "MOV R2, R0 / PUSH R2 ... POP R2" into "PUSH R0 ... POP R2";
##                 v32opt took that for a save of R2 and kept the loop counter in R2 (special/26_pico8_stat at
##                 -O2). promote_pop.lua (the .asm is its v32lua 20261124-dev output) gives number_n 100 and
##                 number_s 137 on the oracle with the fix and NaN for both without.
## TEETH: both checks fail on v32opt 20261124-dev as first released. Needs no Vircon32 tools.
## Usage: ./run.sh [path/to/v32opt]     (or V32OPT=path, default ../../bin/v32opt)
##
#########################################################################
HERE="$(cd "$(dirname "$0")" && pwd)"
OPT="${1:-${V32OPT:-${HERE}/../../bin/v32opt}}"
case "${OPT}" in /*) ;; *) OPT="$(pwd)/${OPT}" ;; esac
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT
status=0
ncases=0
fail() { echo "FAIL: $1"; status=1; }
[ -x "${OPT}" ] || { echo "FAIL: ${OPT} is not an executable"; exit 1; }

for o in -O1 -O2 -O3 -Os; do
    ncases=$((ncases + 1))
    "${OPT}" "${HERE}/savepair.asm" ${o} -o "${TMP}/s.asm" >/dev/null 2>&1 || { fail "savepair ${o}: v32opt failed"; continue; }
    # the 7 has to reach the store: as "MOV R1, 7" (-O1, -O2) or, once __f is inlined, folded into R0 (-O3, -Os)
    grep -qE "^[[:space:]]+MOV +R[01], *7([[:space:]]|;|$)" "${TMP}/s.asm" ||
        fail "savepair ${o}: the argument store MOV R1, 7 was deleted (nothing loads 7 any more)"
done

for o in -O2 -O3; do
    ncases=$((ncases + 1))
    "${OPT}" -L lua ${o} -d "${HERE}/promote_pop.asm" -o "${TMP}/p.asm" >/dev/null 2>&1 || { fail "promote_pop ${o}: v32opt failed"; continue; }
    if grep -q "PROMOTE-LOOPS: preheader load of \[BP-[0-9]*\] into R2" "${TMP}/p.asm"; then
        fail "promote_pop ${o}: a loop slot was promoted into R2, which the loop body POPs into"
    fi
done

[ ${status} = 0 ] && echo "lua-regs: all ${ncases} cases pass"
exit ${status}
