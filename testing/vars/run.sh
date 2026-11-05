#!/bin/sh
# ';@var' / ';@home' through the optimizer's register promotion.
#   - at every level --check-vars and --check-src pass,
#   - -O0 adds no ';@home',
#   - leaf.asm / segment.asm / loops.asm at -O3: promoted variables are re-pointed at their
#     register (';@var NAME loc=Rn') and the move is recorded with ';@home',
#   - a variable whose slot vanished without a ';@home' is caught (exit status 3).
OPT=${V32OPT:-../../bin/v32opt}
cd "$(dirname "$0")" || exit 2
T=${TMPDIR:-/tmp}/v32opt_vars.$$; mkdir -p $T
fail=0; n=0
bad() { echo "FAIL $*"; fail=1; }
for f in leaf segment loops; do
  for L in -O0 -O1 -O2 -O3 -Os; do
    n=$((n+1))
    $OPT $f.asm $L --check-vars --check-src -o $T/$f.out.asm 2>$T/err || { bad "$f $L: $(head -2 $T/err)"; continue; }
    [ "$L" = -O0 ] && grep -q '^;@home' $T/$f.out.asm && bad "$f -O0 wrote ;@home"
  done
  $OPT $f.asm -O3 -o $T/$f.o3.asm 2>/dev/null
  grep -q '^;@var .* loc=R[0-9]' $T/$f.o3.asm || bad "$f -O3: no variable was moved to a register"
  grep -q '^;@home BP' $T/$f.o3.asm || bad "$f -O3: no ';@home BPn Rk'"
done
# the loops case: three promoted variables, nested block variable in a register
$OPT loops.asm -O3 -o $T/l.asm 2>/dev/null
[ "$(grep -c '^;@home BP' $T/l.asm)" -ge 3 ] || bad "loops -O3: fewer than 3 ';@home BP'"
# a call-terminated segment ends with ';@home Rk BPn' before the call
awk '/^;@home R[0-9]+ BP/{h=1;next} h&&/^ *call /{ok=1} {if(!/^;@/&&!/^ *$/)h=0} END{exit !ok}' $T/l.asm || bad "loops -O3: no ';@home Rk BPn' right before a call"
# a variable that is only written: the slot is either still used or the move is recorded
cat > $T/w.asm <<'EOA'
;@hints 1 abi=caller-save
;@lang c
;@fn __function_g lines=1..2 escapes=none
__function_g:
;@src w.c:1
  push BP
  mov BP, SP
  isub SP, 1
;@var v loc=BP-1 type=int
;@src w.c:2
  mov R0, 5
  mov [BP-1], R0
.return:
  mov SP, BP
  pop BP
  ret
EOA
$OPT $T/w.asm -O3 --check-vars -o $T/w.out.asm 2>/dev/null || bad "write-only variable: --check-vars failed"
grep -q '\[BP-1\]\|^;@home\|optimized-out\|^;@var v loc=R' $T/w.out.asm || bad "write-only variable: slot gone without a record"
n=$((n+1))
rm -rf $T
[ $fail = 0 ] && echo "ok   vars ($n runs)"
exit $fail
