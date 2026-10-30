#!/bin/bash
# lua-checks: -fpeephole-lua-checks (-L lua): spills around calls with ';@fn clobbers=', loop facts, check removal
HERE=$(cd "$(dirname "$0")" && pwd); B=${V32OPT:-$HERE/../../bin/v32opt}; COLD=$HERE/../lua-cold
fail=0; W=$(mktemp -d); trap 'rm -rf $W' EXIT
ok() { echo "ok   $1"; }; bad() { echo "FAIL $1"; fail=1; }

# 1. a PUSH ... CALL ... POP that is a call ARGUMENT must stay (regression: __mv_to_table)
$B $HERE/spill.asm -O1 -L lua -o $W/s.out 2>/dev/null
[ "$(grep -c 'PUSH R5' $W/s.out)" = 2 ] && ok "push/pop around calls kept" || bad "push/pop around calls removed"

# 2. the benchmark: same result, fewer instructions, guards only go with the loop facts
for v in dirty clean; do
  python3 $COLD/mkbench.py $v $W/$v.asm
  ref=$(bash $COLD/runcart.sh $W/$v.asm | sed -n 2p)
  for L in -O1 -O2 -O3; do
    $B $W/$v.asm $L -L lua -o $W/$v$L.asm 2>/dev/null
    $B $W/$v.asm $L -L lua -fno-peephole-lua-checks -o $W/$v$L.off.asm 2>/dev/null
    r=$(bash $COLD/runcart.sh $W/$v$L.asm); r0=$(bash $COLD/runcart.sh $W/$v$L.off.asm)
    [ "$(echo "$r" | sed -n 2p)" = "$ref" ] || { bad "$v $L result"; continue; }
    n=$(echo "$r" | sed -n 1p | tr -dc 0-9); n0=$(echo "$r0" | sed -n 1p | tr -dc 0-9)
    [ "$n" -lt "$n0" ] && ok "$v $L: $n insns (checks off: $n0)" || bad "$v $L: $n not below $n0"
  done
  # without the ;@loop facts the key checks stay (the P3 guard after arithmetic is provable without them)
  grep -v '^ *;@loop' $W/$v.asm > $W/$v.nof.asm
  $B $W/$v.nof.asm -O1 -L lua -o $W/n1.asm 2>/dev/null; $B $W/$v.nof.asm -O1 -L lua -fno-peephole-lua-checks -o $W/n0.asm 2>/dev/null
  [ "$(grep -c 'ILT R0, 1' $W/n1.asm)" = "$(grep -c 'ILT R0, 1' $W/n0.asm)" ] && ok "$v: no loop facts -> the key (>= 1) check stays" || bad "$v: key checks removed without facts"
done
exit $fail
