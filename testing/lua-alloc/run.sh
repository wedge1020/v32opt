#!/usr/bin/env bash
##
## run.sh - regression test: what v32opt does with v32lua 20261120-dev and later allocations and cart layout.
##
##   oom checks    __malloc / __malloc_owned never return 0 (they jump to __oom_handler themselves), so the
##                 "IEQ Rn, 0 / JT Rn, __oom_handler" after a call is dead (peephole-lua-checks). oom_unit.asm:
##                 CHECK-A goes; CHECK-B (tests a word loaded from the block, not the pointer) and CHECK-M
##                 (__malloc's own heap/stack test) stay. TEETH: without the __malloc_owned label (an older
##                 v32lua, whose __malloc returned 0), with -L c, and with -fno-peephole-lua-checks every check
##                 stays. none.asm / arena.asm (v32lua output, plain and --gc arena --gc-check): only
##                 __malloc's own test is left at -O1..-O3.
##   owner         arena.asm: the "MOV R0, <owner>" before each CALL __malloc_owned is an input of the call
##                 (liveness reads __malloc_owned's body) and stays at -O3.
##   gc-check      every "PUSH Rv / CALL __arena_check_global|store" stays.
##   layout        __v32lua_code_end .. __v32lua_rom_end (labels, instructions, directives) is unchanged at
##                 -O3. TEETH: V32OPT_LAYOUT_SELFTEST=1 (a RET appended after the string pool) makes v32opt
##                 exit 4 and write nothing; a file without __v32lua_code_end is not checked.
## The .asm files are v32lua 20261123-dev output of alloc_none.lua / alloc.lua (-w --no-build-scripts, the
## second with --gc arena --gc-check). Needs no Vircon32 tools.
## Usage: ./run.sh [path/to/v32opt]     (or V32OPT=path, default ../../bin/v32opt)
##
#########################################################################
HERE="$(cd "$(dirname "$0")" && pwd)"
OPT="${1:-${V32OPT:-${HERE}/../../bin/v32opt}}"
case "${OPT}" in /*) ;; *) OPT="$(pwd)/${OPT}" ;; esac
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT
status=0
ncases=0
fail() { echo "FAIL: $1"; status=1; }
[ -x "${OPT}" ] || { echo "FAIL: ${OPT} is not an executable"; exit 1; }

oomjumps() { grep -cE "^[[:space:]]+J[TF] +R[0-9]+, *__oom_handler" "$1"; }
has() { grep -q -- "$2" "$1"; }
# labels, instructions and directives from __v32lua_code_end on, comments and blank lines dropped
tailof() { awk '/^__v32lua_code_end:/{p=1} p' "$1" | sed 's/;.*//; s/[[:space:]][[:space:]]*/ /g; s/^ //; s/ $//' | grep -v '^$'; }

# ---- unit
U="${HERE}/oom_unit.asm"
"${OPT}" "${U}" -O1 -o "${TMP}/u.asm" >/dev/null 2>&1 || fail "oom_unit: v32opt failed"
ncases=$((ncases + 1))
has "${TMP}/u.asm" "CHECK-A" && fail "oom_unit -O1: CHECK-A (dead) is still there"
has "${TMP}/u.asm" "CHECK-B" || fail "oom_unit -O1: CHECK-B (live) was removed"
has "${TMP}/u.asm" "CHECK-M" || fail "oom_unit -O1: CHECK-M (__malloc's own) was removed"

sed 's/^__malloc_owned:$//; s/^.*;@fn __malloc_owned.*$//; s/__malloc_owned/__malloc/' "${U}" > "${TMP}/old.asm"
for v in "old:${TMP}/old.asm:-O1" "c:${U}:-O1 -L c" "off:${U}:-O3 -fno-peephole-lua-checks"; do
    name="${v%%:*}"; rest="${v#*:}"; file="${rest%%:*}"; args="${rest#*:}"
    ncases=$((ncases + 1))
    # shellcheck disable=SC2086
    "${OPT}" "${file}" ${args} -o "${TMP}/t.asm" >/dev/null 2>&1 || { fail "oom_unit ${name}: v32opt failed"; continue; }
    n="$(oomjumps "${TMP}/t.asm")"
    [ "${n}" -ge 3 ] || fail "oom_unit ${name} (${args}): ${n} __oom_handler jumps left, expected all 3"
done

# ---- v32lua output
for a in none arena; do
    A="${HERE}/${a}.asm"
    ncases=$((ncases + 1))
    has "${A}" "^__malloc_owned:" || fail "${a}.asm: no __malloc_owned label (regenerate with v32lua 20261120-dev or later)"
    [ "$(oomjumps "${A}")" -ge 10 ] || fail "${a}.asm: expected the runtime's post-call checks in the input"
    for o in -O1 -O2 -O3; do
        ncases=$((ncases + 1))
        rm -f "${TMP}/o.asm"
        "${OPT}" "${A}" ${o} -o "${TMP}/o.asm" >/dev/null 2>&1; rc=$?
        [ ${rc} = 0 ] || { fail "${a}.asm ${o}: exit ${rc}"; continue; }
        n="$(oomjumps "${TMP}/o.asm")"
        [ "${n}" = 1 ] || fail "${a}.asm ${o}: ${n} __oom_handler jumps left, expected only __malloc's own"
        # layout: the significant lines from __v32lua_code_end on are the same
        tailof "${A}" > "${TMP}/in.tail"; tailof "${TMP}/o.asm" > "${TMP}/out.tail"
        cmp -s "${TMP}/in.tail" "${TMP}/out.tail" || fail "${a}.asm ${o}: the layout after __v32lua_code_end changed"
        [ "$(tail -n 1 "${TMP}/out.tail")" = "__v32lua_rom_end:" ] || fail "${a}.asm ${o}: __v32lua_rom_end is not last"
    done
done

# ---- arena: owner stores and --gc-check calls survive
A="${HERE}/arena.asm"
"${OPT}" "${A}" -O3 -o "${TMP}/a3.asm" >/dev/null 2>&1
own() { grep -B1 -E "^[[:space:]]+CALL +__malloc_owned" "$1" | grep -cE "^[[:space:]]+MOV +R0,"; }
ncases=$((ncases + 1))
oi="$(own "${A}")"; oo="$(own "${TMP}/a3.asm")"
[ "${oi}" -ge 5 ] && [ "${oi}" = "${oo}" ] || fail "arena.asm -O3: MOV R0, owner before CALL __malloc_owned: ${oi} in, ${oo} out"
chk() { grep -B1 -E "^[[:space:]]+CALL +__arena_check_(global|store)" "$1" | grep -cE "^[[:space:]]+PUSH +R"; }
ncases=$((ncases + 1))
ci="$(chk "${A}")"; co="$(chk "${TMP}/a3.asm")"
[ "${ci}" -ge 1 ] && [ "${ci}" = "${co}" ] || fail "arena.asm -O3: PUSH before CALL __arena_check_*: ${ci} in, ${co} out"

# ---- layout check has teeth
ncases=$((ncases + 1))
rm -f "${TMP}/s.asm"
V32OPT_LAYOUT_SELFTEST=1 "${OPT}" "${HERE}/none.asm" -O1 -o "${TMP}/s.asm" >/dev/null 2>"${TMP}/s.err"; rc=$?
[ ${rc} = 4 ] || fail "layout selftest: exit ${rc}, expected 4"
[ -e "${TMP}/s.asm" ] && fail "layout selftest: an output file was written"
grep -q "__v32lua_code_end" "${TMP}/s.err" || fail "layout selftest: no message naming the label"
ncases=$((ncases + 1))
V32OPT_LAYOUT_SELFTEST=1 "${OPT}" "${U}" -O1 -o "${TMP}/s2.asm" >/dev/null 2>&1 || fail "layout selftest: a file without __v32lua_code_end was refused"

[ ${status} = 0 ] && echo "lua-alloc: all ${ncases} cases pass"
exit ${status}
