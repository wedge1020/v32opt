#!/bin/bash
# lua-audit: differential test of v32opt -L lua on REAL v32lua output.
#   usage: run.sh V32LUA_DIR  [levels]     V32LUA_DIR holds bin/v32lua and tests/audit/*.lua
# For each audit program: compile with `v32lua -g`, run the unoptimized assembly and the -O1/-O2/-O3 results in v32sim,
# and compare every global variable (the %define var_NAME lines are the RAM words 0..max). Needs V32TOOLS/V32SIM like
# testing/lua-cold/runcart.sh.
HERE=$(cd "$(dirname "$0")" && pwd)
LUA=${1:?usage: run.sh V32LUA_DIR [levels]}; LEVELS=${2:-"-O1 -O2 -O3"}
B=${V32OPT:-$HERE/../../bin/v32opt}; RUN=$HERE/../lua-cold/runcart.sh
W=$(mktemp -d); trap 'rm -rf $W' EXIT
fail=0; n=0
for t in $LUA/tests/audit/*.lua $LUA/tests/audit/pico8/*.lua; do
  [ -f "$t" ] || continue; case $t in *.ref.lua) continue;; esac
  b=$(basename $t .lua); d=$W/$b; mkdir -p $d; cp $t $d/; ( cd $d && $LUA/bin/v32lua -w -g -o $b.asm $b.lua >/dev/null 2>&1 ) || continue
  [ -f $d/$b.asm ] || continue
  max=$(sed -n 's/^%define  *var_[A-Za-z0-9_]* *0x\([0-9A-Fa-f]*\).*/\1/p' $d/$b.asm | while read h; do echo $((16#$h)); done | sort -n | tail -1)
  [ -n "$max" ] || continue
  words=$((max+1))
  ref=$(bash $RUN $d/$b.asm $words@0 2>&1); rrc=$(echo "$ref" | head -1)
  case $rrc in *"rc=0"*) ;; *) echo "skip $b (unoptimized run: $rrc)"; continue;; esac
  refv=$(echo "$ref" | tail -n +2)
  for L in $LEVELS; do
    $B $d/$b.asm $L -L lua -o $d/$b$L.asm 2>$d/err || { echo "FAIL $b $L: v32opt failed"; fail=1; continue; }
    out=$(bash $RUN $d/$b$L.asm $words@0 2>&1); orc=$(echo "$out" | head -1)
    case $orc in *"rc=0"*) ;; *) echo "FAIL $b $L: $orc"; fail=1; continue;; esac
    if [ "$(echo "$out" | tail -n +2)" = "$refv" ]; then n=$((n+1)); else echo "FAIL $b $L: globals differ"; fail=1; fi
  done
done
echo "$n optimized runs match the unoptimized program"
exit $fail
