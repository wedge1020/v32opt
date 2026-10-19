#!/usr/bin/env bash
##
## run.sh - regression test: assembly lines of any length.
##
## long-lines.asm has a pointer table of 800 functions (over 13000
## characters on one line; the last functions are named only at its far end)
## and string literals of 1500 and 30000 characters. v32opt must
##   - accept the file (it once refused any line over 1023 characters),
##   - write every string and pointer line back byte for byte,
##   - keep every function named in the pointer table, even those past
##     the first 8192 characters, while still removing the one function
##     nothing refers to.
## Needs no Vircon32 toolchain. Usage: ./run.sh [path/to/v32opt]
##
#########################################################################
OPT="${1:-../../bin/v32opt}"
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$(mktemp)"
trap 'rm -f "${OUT}"' EXIT
fail() { echo "FAIL: $1"; exit 1; }

"${OPT}" -O2 "${HERE}/long-lines.asm" -o "${OUT}" || fail "v32opt refused the file"
cmp <(grep -E '^ *(string|pointer)' "${HERE}/long-lines.asm") \
    <(grep -E '^ *(string|pointer)' "${OUT}") || fail "a data line was altered"
[ "$(grep -c '^__function_f[0-9]*:' "${OUT}")" = 800 ] || fail "a function in the pointer table was removed"
grep -q '^__function_f799:' "${OUT}" || fail "the last function in the table was removed"
grep -q '^__function_dead:' "${OUT}" && fail "the unreferenced function was kept"
echo "PASS: long-lines"
