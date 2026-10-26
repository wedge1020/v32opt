#!/bin/sh
# The compiler's requests (doc/hints.md, "Requests"): per-function inline limits (alwaysinline, inline, hot, cold)
# and per-loop unrolling/heat (';@loop'). The inputs are v32cc output (requests_*.c says how they were made). Only
# the optimizer is run; tests/optexec.py in v32cc executes the optimized programs.
#
#   inline: every body is 9 or 15 instructions; the default limit is 8
#   loops:  every loop body is one XOR, so the XORs of a function are the copies of the body
OPT=${V32OPT:-../../bin/v32opt}
cd "$(dirname "$0")" || exit 2
T=${TMPDIR:-/tmp}/v32opt_req.$$
fail=0
ok()  { echo "ok   $1"; }
bad() { echo "FAIL $1"; fail=1; }

# --- inline ---------------------------------------------------------------------------------------------------------
called() { grep -ci "call __function_$1_s0" "$2"; }          # calls of a function left in the output

$OPT requests_inline.asm -O0 -finline -o "$T.i.asm" || exit 2
for f in always kw hot cold_always; do
    [ "$(called $f "$T.i.asm")" = 0 ] && ok "inline: $f is inlined" || bad "inline: $f should be inlined"
done
for f in plain kw_not cold_tiny never; do
    [ "$(called $f "$T.i.asm")" != 0 ] && ok "inline: $f stays a call" || bad "inline: $f should stay a call"
done

# the same program without its hints: everything small enough is inlined, nothing else
grep -v '^;@' requests_inline.asm > "$T.n.asm"
$OPT "$T.n.asm" -O0 -finline -o "$T.ni.asm" || exit 2
for f in plain always kw kw_not hot; do
    [ "$(called $f "$T.ni.asm")" != 0 ] && ok "inline, no hints: $f is too big for the limit" || bad "inline, no hints: $f"
done
for f in cold_always cold_tiny never; do
    [ "$(called $f "$T.ni.asm")" = 0 ] && ok "inline, no hints: $f is inlined" || bad "inline, no hints: $f"
done

# -finline-max=0 switches the size limit off; only alwaysinline gets past it
$OPT requests_inline.asm -O0 -finline -finline-max=0 -o "$T.z.asm" || exit 2
for f in always cold_always; do
    [ "$(called $f "$T.z.asm")" = 0 ] && ok "inline-max=0: $f (alwaysinline) is inlined" || bad "inline-max=0: $f"
done
for f in plain kw hot cold_tiny; do
    [ "$(called $f "$T.z.asm")" != 0 ] && ok "inline-max=0: $f stays a call" || bad "inline-max=0: $f"
done
$OPT requests_inline.asm -O0 -fno-inline -o "$T.o.asm" || exit 2
cmp -s "$T.o.asm" requests_inline.asm && ok "no inlining without -finline: the hints do not switch it on" || bad "-fno-inline changed the program"

# --- loops ----------------------------------------------------------------------------------------------------------
copies() { awk "/^__function_$1:/{f=1;next} /^__function_/{f=0} f" "$2" | grep -ci '^[[:space:]]*xor'; }
want() {     # <function> <copies with the hints> <copies without>
    got=$(copies "$1" "$T.l.asm"); wo=$(copies "$1" "$T.ln.asm")
    [ "$got" = "$2" ] && ok "loops: $1 has $2 copies of its body" || bad "loops: $1 has $got copies, want $2"
    [ "$wo" = "$3" ] && ok "loops, no hints: $1 has $3" || bad "loops, no hints: $1 has $wo, want $3"
}
$OPT requests_loops.asm -O3 -floop-unroll-partial -o "$T.l.asm" || exit 2
grep -v '^;@' requests_loops.asm > "$T.ln.asm"
$OPT "$T.ln.asm" -O3 -floop-unroll-partial -o "$T.ln.asm.o" && mv "$T.ln.asm.o" "$T.ln.asm"
want plain4        4 4      # a short constant loop is unrolled completely anyway
want plain20       5 5      # 4 copies and the remainder loop
want nounroll4     1 4      # '#pragma nounroll'
want unroll40      40 5     # '#pragma unroll 40': a complete unrolling past the usual limit
want unroll_full30 30 5     # '#pragma unroll'
want unroll_part   7 5      # '#pragma unroll 6': 6 copies and the remainder loop
want nounroll_n    1 5
want hot_loop20    20 5     # 'v32cc loop hot': twice the usual limit
want cold_loop4    1 4      # 'v32cc loop cold'
want cold_unroll4  4 4      # an explicit unroll beats cold
want hot_fn20      20 5     # '__attribute__((hot))' on the function
want cold_fn4      1 4      # '__attribute__((cold))'
want cold_fn_n     1 5

# a measured profile has the last word over the hints, both ways. The profile is made here: every instruction of one
# function runs <count> times in a run of 1,000,000 instructions (--profile FILE, doc/hints.md).
prof() {     # <function> <count> -> $T.prof
    { echo "# v32opt line profile v1"; echo "# total 1000000"
      awk -v n="$2" "/^__function_$1:/{f=1;next} /^__function_/{f=0} f && !/^[[:space:]]*;/ && !/^[[:space:]]*\$/ && !/:[[:space:]]*\$/ {print NR, n}" requests_loops.asm
    } > "$T.prof"
}
prof hot_loop20 1                  # hinted hot, the profile says it hardly ran
$OPT requests_loops.asm -O3 -floop-unroll-partial --profile "$T.prof" -o "$T.p.asm" || exit 2
got=$(copies hot_loop20 "$T.p.asm")
[ "$got" = 1 ] && ok "profile: a cold loop is left alone whatever the hint says" || bad "profile: hot_loop20 has $got copies, want 1"
prof cold_loop4 400000             # hinted cold, the profile says it is the whole run
$OPT requests_loops.asm -O3 -floop-unroll-partial --profile "$T.prof" -o "$T.p.asm" || exit 2
got=$(copies cold_loop4 "$T.p.asm")
[ "$got" = 4 ] && ok "profile: a hot loop is unrolled whatever the hint says" || bad "profile: cold_loop4 has $got copies, want 4"

rm -f "$T".*
exit $fail
