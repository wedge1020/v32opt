#!/bin/sh
# Volatile accesses must survive every pass. For each optimization level: no marked access is
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
rm -f /tmp/v32opt_vol.$$.asm /tmp/v32opt_vol.$$.err
exit $fail
