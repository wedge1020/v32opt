#!/bin/sh
# Symbol names longer than 31 characters must not be compared by their first 31 (long-names.asm).
OPT=${V32OPT:-../../bin/v32opt}
cd "$(dirname "$0")" || exit 2
fail=0
for L in -O1 -O2 -O3 -Os; do
    $OPT long-names.asm $L -o /tmp/v32opt_ln.$$.asm || { echo "FAIL v32opt $L"; fail=1; continue; }
    if grep -q '\[global_a_very_long_symbol_name_angle\], R' /tmp/v32opt_ln.$$.asm; then echo "ok   $L store of the shorter-named global kept"; else echo "FAIL $L store of the shorter-named global deleted"; fail=1; fi
done
rm -f /tmp/v32opt_ln.$$.asm
exit $fail
