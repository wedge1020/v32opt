#!/bin/sh
# "__function_X_return:" is function X's internal label only when X exists; otherwise it is a function
# (see return-label.asm). Every label the output keeps must still be defined where it is referenced.
OPT=${V32OPT:-../../bin/v32opt}
cd "$(dirname "$0")" || exit 2
fail=0
chk() { # description, command that must succeed
    if eval "$2"; then echo "ok   $1"; else echo "FAIL $1"; fail=1; fi
}
for L in -O1 -O2 -O3 -Os; do
    o=/tmp/v32opt_rl.$$.asm
    $OPT return-label.asm $L -o $o || { echo "FAIL v32opt $L"; fail=1; continue; }
    chk "$L get_return (no hints) survives"       "grep -q '^__function_get_return:' $o"
    chk "$L bar_return (';@fn') survives"         "grep -q '^__function_bar_return:' $o"
    chk "$L foo_return stays inside foo"          "grep -q '^__function_foo_return:' $o || ! grep -q '__function_foo_return' $o"
    # no label is referenced without being defined
    chk "$L no undefined function labels"         "(for l in \$(grep -o '__function_[A-Za-z0-9_]*' $o | sort -u); do grep -q \"^\$l:\" $o || exit 1; done)"
done
rm -f /tmp/v32opt_rl.$$.asm
exit $fail
