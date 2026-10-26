#!/bin/sh
# Labels and symbols differing only in case are different things (see case-sensitive.asm).
OPT=${V32OPT:-../../bin/v32opt}
cd "$(dirname "$0")" || exit 2
fail=0
chk() { # description, command that must succeed
    if eval "$2"; then echo "ok   $1"; else echo "FAIL $1"; fail=1; fi
}
for L in -O1 -O2 -O3 -Os; do
    $OPT case-sensitive.asm $L -o /tmp/v32opt_cs.$$.asm || { echo "FAIL v32opt $L"; fail=1; continue; }
    o=/tmp/v32opt_cs.$$.asm
    # both bodies exist (inlined or not): the *3 one and the +7 one, each exactly where its own call was
    chk "$L foo keeps its own body"  "grep -qi 'imul R0, 3' $o && grep -qi 'iadd R[0-9]*, 7' $o"
    chk "$L both globals are read"   "grep -q '\\[global_x\\]' $o && grep -q '\\[global_X\\]' $o"
    chk "$L Rate is a function"      "grep -q '^__function_Rate:' $o || ! grep -q 'call __function_Rate' $o"
done
rm -f /tmp/v32opt_cs.$$.asm
exit $fail
