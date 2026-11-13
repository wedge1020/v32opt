#!/bin/sh
# Volatile accesses must survive every pass. For each optimization level: no marked access is
# misplaced_v.asm: ";@v" on instructions without a memory operand gives one warning and no "optimized away" report.
# lost (loop copying at -O3 may add more), none has turned into a register copy, and v32opt
# reports no warning.
OPT=${V32OPT:-../../bin/v32opt}
cd "$(dirname "$0")" || exit 2
in=$(grep -c ';@v' volatile.asm)
fail=0
for L in -O1 -O2 -O3 -Os; do
    $OPT volatile.asm $L -o /tmp/v32opt_vol.$$.asm 2>/tmp/v32opt_vol.$$.err
    out=$(grep -c ';@v' /tmp/v32opt_vol.$$.asm)
    # no volatile access may have turned into a register copy
    bad=$(grep ';@v' /tmp/v32opt_vol.$$.asm | grep -vc '\[')
    if [ "$out" -lt "$in" ] || [ "$bad" != 0 ] || [ -s /tmp/v32opt_vol.$$.err ]; then
        echo "FAIL volatile $L: $in accesses in, $out out, $bad lost their memory operand"; cat /tmp/v32opt_vol.$$.err
        fail=1
    else
        echo "ok   volatile $L ($out accesses kept)"
    fi
done
# ';@v' on an instruction with no memory operand (misplaced_v.asm): exactly one warning that says the hint marks
# loads and stores only, the exit status is 0 as before, no "optimized away" report, whatever the level
for L in -O0 -O1 -O2 -O3; do
    $OPT misplaced_v.asm $L -o /tmp/v32opt_vol.$$.asm 2>/tmp/v32opt_vol.$$.err; rc=$?
    w=$(grep -c "';@v' on an instruction that accesses no memory" /tmp/v32opt_vol.$$.err)
    a=$(grep -c "optimized away" /tmp/v32opt_vol.$$.err)
    o=$(grep -c "warning" /tmp/v32opt_vol.$$.err)
    if [ "$rc" != 0 ] || [ "$w" != 1 ] || [ "$a" != 0 ] || [ "$o" != 1 ]; then
        echo "FAIL misplaced ';@v' $L: rc=$rc, $w misplaced-hint warning(s), $a 'optimized away', $o warning line(s) in all"; cat /tmp/v32opt_vol.$$.err
        fail=1
    else
        echo "ok   misplaced ';@v' $L (one warning, rc 0)"
    fi
done
# the one real volatile load is still there
grep -q 'mov R0, \[200\] ;@v' /tmp/v32opt_vol.$$.asm || { echo "FAIL misplaced_v.asm: the volatile load is gone"; fail=1; }
rm -f /tmp/v32opt_vol.$$.asm /tmp/v32opt_vol.$$.err
exit $fail
