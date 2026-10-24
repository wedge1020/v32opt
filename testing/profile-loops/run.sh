#!/usr/bin/env bash
##
## run.sh - regression test: --profile steers the loop passes.
##
## profile-loops.asm has three identical constant-trip loops. The script
## writes a synthetic profile in which the __hot_ loop is more than 2% of the
## run, the __cold_ loop less than 0.1%, and the __warm_ loop in between, then
## checks that with it
##   * the hot loop is partially unrolled with the larger factor (-floop-unroll-partial
##     on a 100-trip loop whose count the pass is told not to know...),
##   * the cold loop is left as it was (not unrolled, not even inverted),
##   * the warm loop is treated as without a profile.
## Needs no Vircon32 toolchain. Usage: ./run.sh [path/to/v32opt]
##
#########################################################################
OPT="${1:-../../bin/v32opt}"
HERE="$(cd "$(dirname "$0")" && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT
status=0
fail() { echo "FAIL: $1"; status=1; }
SRC="${HERE}/profile-loops.asm"

# profile: lines between __X_start: and __X_end: that are instructions
awk '
  /^__hot_start:/  {r="hot"}  /^__hot_end:/  {r=""}
  /^__cold_start:/ {r="cold"} /^__cold_end:/ {r=""}
  /^__warm_start:/ {r="warm"} /^__warm_end:/ {r=""}
  /^[ \t]+[A-Za-z]/ {
      n = (r=="hot") ? 100000 : (r=="warm") ? 2000 : 1
      print NR, n
  }' "${SRC}" > "${TMP}/body.txt"
{ echo "# v32opt line profile v1"; echo "# total 1000000"; cat "${TMP}/body.txt"; } > "${TMP}/p.prof"

fn() { awk -v f="__function_$2:" '$0 == f {on=1; next} on && /^__function_/ {exit} on {print}' "$1"; }

"${OPT}" "${SRC}" -O3 -floop-unroll-max-trips=4 -o "${TMP}/plain.asm" >/dev/null || fail "v32opt failed (no profile)"
"${OPT}" "${SRC}" -O3 -floop-unroll-max-trips=4 --profile "${TMP}/p.prof" -o "${TMP}/prof.asm" >/dev/null || fail "v32opt failed (profile)"

# without a profile no loop is fully unrolled (100 trips > 4)
for f in hot cold warm; do
    fn "${TMP}/plain.asm" "${f}" | grep -q "__${f}_end" || fail "${f}: unexpected change without a profile"
done
# with it, the hot loop has more copies of the body than the warm one
nh="$(fn "${TMP}/prof.asm" hot  | grep -c 'IADD R5, R4')"
nw="$(fn "${TMP}/prof.asm" warm | grep -c 'IADD R5, R4')"
nc="$(fn "${TMP}/prof.asm" cold | grep -c 'IADD R5, R4')"
[ "${nh}" -gt "${nw}" ] || fail "the hot loop (${nh} copies) is not unrolled more than the warm one (${nw})"
[ "${nc}" = 1 ] || fail "the cold loop was unrolled (${nc} copies of the body)"
a="$(fn "${SRC}" cold | grep -v '^;')"
b="$(fn "${TMP}/prof.asm" cold | grep -v '^;')"
[ "${a}" = "${b}" ] || fail "the cold loop was changed (not even inverted)"
# the warm loop is the same with and without the profile
norm() { grep -v '^;' | sed -E 's/v32opt_(loop|unroll)_[0-9]+/v32opt_N/g; s/_p[0-9]+/_pN/g'; }
a="$(fn "${TMP}/plain.asm" warm | norm)"
b="$(fn "${TMP}/prof.asm" warm | norm)"
[ "${a}" = "${b}" ] || fail "the warm loop changed with the profile"
# a profile that does not fit the input is ignored
printf '# v32opt line profile v1\n# total 100\n1 5\n2 5\n3 5\n' > "${TMP}/bad.prof"
"${OPT}" "${SRC}" -O3 --profile "${TMP}/bad.prof" -o "${TMP}/bad.asm" >/dev/null 2>"${TMP}/err.txt" || fail "v32opt failed (bad profile)"
grep -q "does not match" "${TMP}/err.txt" || fail "a mismatching profile was not reported"
[ "${status}" = 0 ] && echo "PASS: profile-loops"
exit "${status}"
