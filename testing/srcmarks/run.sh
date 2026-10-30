#!/bin/sh
# Source markers (';@src') through the optimizer. For each .asm in the test corpus: add markers
# with inject.py, then at every optimization level check that
#   - the output is still valid for --check-src (no unmarked instruction, every function's first
#     instruction has a marker),
#   - -g0 gives the same program as running without markers (the markers cost nothing),
#   - the set of source lines with code at -O1..-O3 / -Os is a subset of the set at -O0:
#     optimization may lose lines (dead code) but never invents one.
OPT=${V32OPT:-../../bin/v32opt}
cd "$(dirname "$0")" || exit 2
T=${TMPDIR:-/tmp}/v32opt_src.$$; mkdir -p $T
fail=0; n=0
for f in $(find .. -name '*.asm' -not -path '../srcmarks/*' -not -name '*Opt.asm' | sort); do
  b=$(echo $f | tr '/.' '__')
  python3 inject.py $f $T/$b.asm
  for M in c lua; do
    $OPT $T/$b.asm -O0 -L $M -o $T/$b.O0.asm 2>/dev/null
    python3 lineset.py $T/$b.O0.asm > $T/$b.O0.lines 2>/dev/null
    for L in -O1 -O2 -O3 -Os "-O3 -fno-dce" "-O2 -fno-dce"; do
      n=$((n+1))
      $OPT $T/$b.asm $L -L $M --check-src -o $T/$b.out.asm 2>$T/$b.err || { echo "FAIL check-src $f $L $M"; head -3 $T/$b.err; fail=1; continue; }
      $OPT $T/$b.asm $L -L $M -g0 -o $T/$b.strip.asm 2>/dev/null
      $OPT $f $L -L $M -o $T/$b.plain.asm 2>/dev/null
      if ! cmp -s $T/$b.strip.asm $T/$b.plain.asm; then echo "FAIL strip differs from unmarked $f $L $M"; fail=1; fi
      python3 lineset.py $T/$b.out.asm > $T/$b.lines 2>$T/$b.lerr || { echo "FAIL lineset $f $L $M: $(cat $T/$b.lerr)"; fail=1; }
      extra=$(comm -13 $T/$b.O0.lines $T/$b.lines)
      if [ -n "$extra" ]; then echo "FAIL invented source lines $f $L $M: $extra"; fail=1; fi
    done
  done
done
rm -rf $T
[ $fail = 0 ] && echo "ok   srcmarks ($n runs)"
exit $fail
