#!/bin/bash
# fuzz.sh -- generate random programs and compare optimized and unoptimized builds in v32sim.
#
#   usage: fuzz.sh [-n COUNT] [-s FIRST_SEED] [-g LIST] [-t SECONDS] [-k]
#     -n COUNT      programs per generator and mode (default 50: seeds FIRST .. FIRST+COUNT-1)
#     -s FIRST      first seed (default 1)
#     -g LIST       generators to run, comma separated (default: gen,gen-live,genlua,genlua-live,genasm
#                   and, when the stock compiler is available, genreg,genreg-live)
#     -t SECONDS    time limit of one run.py / run_lua.py call; a call that hits it counts as one ERROR
#                   (default 1800). The calls cover 20 programs each.
#     -k            keep going with the other generators after a failure (this is the default; kept for clarity)
#   environment:   V32OPT (optimizer, default bin/v32opt of this project), WORK (default differential/work/fuzz),
#                  MAXF (simulator frames before a run counts as SKIP, default 40), V32STOCK (stock `compile`, for genreg),
#                  FUZZ_FLAGS_C / FUZZ_FLAGS_LUA (override the flag sets, ';' separated)
#
# Generators and what they run:
#   gen          gen.py            C, v32cc     run.py        STRICT=1
#   gen-live     GEN_LIVE=1 gen.py                            STRICT=1  (function pointers, loop-carried values, multi-exit loops)
#   genlua       genlua.py         Lua          run_lua.py
#   genlua-live  GENLUA_LIVE=1 genlua.py                      (indirect calls, multi-value returns, loops with calls and breaks)
#   genasm       genasm.py         assembly     run.py        STRICT=1  (non-ABI result registers, call rN, tail jumps)
#   genreg       genreg.py         C, stock `compile` (COMPILER=stock)  STRICT=1
#   genreg-live  GENREG_LIVE=1 genreg.py
#
# Flag sets: -O1 -O2 -O3 -Os, and for C also "-O3 -floop-unroll-partial". -O2 already includes promote-regs,
# promote-leaf and promote-loops, so they are not repeated. The simulator runs with --fixed-clock --seed=1
# (set in run.py / run_lua.py), so runs repeat exactly.
#
# Output: per generator the number of programs, of FAIL / ERROR / SKIP lines, and the failing seeds; then a table.
# Exit status 1 if any FAIL or ERROR occurred (SKIP, a program over the frame limit, does not count), 2 on a setup problem.
HERE=$(cd "$(dirname "$0")" && pwd)
N=50; FIRST=1; GENS=""; TMO=1800
while getopts "n:s:g:t:kh" o; do
  case $o in
    n) N=$OPTARG;; s) FIRST=$OPTARG;; g) GENS=$OPTARG;; t) TMO=$OPTARG;; k) ;;
    *) sed -n '2,/^# Output/p' "$0" | sed 's/^# \{0,1\}//'; exit 2;;
  esac
done
export V32OPT=${V32OPT:-$HERE/../../bin/v32opt}
export MAXF=${MAXF:-40}
BASEW=${WORK:-$HERE/work/fuzz}
STOCKC=${V32STOCK:-}
[ -z "$STOCKC" ] && for c in /home/claude/stocktools/compile "$HERE"/../../../stocktools/compile; do [ -x "$c" ] && STOCKC=$c && break; done
[ -x "$V32OPT" ] || { echo "fuzz.sh: V32OPT=$V32OPT is not executable"; exit 2; }
if [ -z "$GENS" ]; then
  GENS=gen,gen-live,genlua,genlua-live,genasm
  [ -n "$STOCKC" ] && [ -x "$STOCKC" ] && GENS=$GENS,genreg,genreg-live
fi
FC=${FUZZ_FLAGS_C:--O1;-O2;-O3;-Os;-O3 -floop-unroll-partial}
FL=${FUZZ_FLAGS_LUA:--O1;-O2;-O3;-Os}
mkdir -p "$BASEW"
SUMMARY=$BASEW/summary.txt; : > "$SUMMARY"
rc=0

run_gen() {  # name
  local g=$1 tag script ext runner flags envs=() strict=1
  case $g in
    gen)         tag=g;  script=gen.py;    ext=c;   runner=run.py;     flags=$FC;;
    gen-live)    tag=gl; script=gen.py;    ext=c;   runner=run.py;     flags=$FC; envs=(GEN_LIVE=1);;
    genlua)      tag=l;  script=genlua.py; ext=lua; runner=run_lua.py; flags=$FL; strict=0;;
    genlua-live) tag=ll; script=genlua.py; ext=lua; runner=run_lua.py; flags=$FL; envs=(GENLUA_LIVE=1); strict=0;;
    genasm)      tag=a;  script=genasm.py; ext=asm; runner=run.py;     flags=$FC;;
    genreg)      tag=r;  script=genreg.py; ext=c;   runner=run.py;     flags=$FC; envs=(COMPILER=stock V32STOCK=$STOCKC);;
    genreg-live) tag=rl; script=genreg.py; ext=c;   runner=run.py;     flags=$FC; envs=(COMPILER=stock V32STOCK=$STOCKC GENREG_LIVE=1);;
    *) echo "fuzz.sh: unknown generator $g"; return;;
  esac
  local d=$BASEW/$tag; rm -rf "$d"; mkdir -p "$d"
  local log=$d/log.txt; : > "$log"
  local -a fargs=(); local IFS_SAVE=$IFS
  IFS=';' read -ra fs <<< "$flags"; IFS=$IFS_SAVE
  for f in "${fs[@]}"; do fargs+=(--flags "$f"); done
  local last=$((FIRST + N - 1)) s=$FIRST made=0
  while [ $s -le $last ]; do
    local e=$((s + 19)); [ $e -gt $last ] && e=$last
    local files=()
    for q in $(seq $s $e); do
      if env "${envs[@]}" python3 "$HERE/$script" $q "$d/$tag$q.$ext" 2>>"$log"; then files+=("$d/$tag$q.$ext"); else echo "ERROR $tag$q.$ext generator failed" >> "$log"; fi
    done
    made=$((made + ${#files[@]}))
    if [ ${#files[@]} -gt 0 ]; then
      env "${envs[@]}" WORK="$d" STRICT=$([ $strict = 1 ] && echo 1) \
        timeout $TMO python3 "$HERE/$runner" "${fargs[@]}" "${files[@]}" >> "$log" 2>&1
      local st=$?
      [ $st -eq 124 ] && echo "ERROR $tag$s-$tag$e.$ext run timed out after ${TMO}s" >> "$log"
    fi
    s=$((e + 1))
  done
  python3 - "$g" "$tag" "$ext" "$log" "$made" "$FIRST" <<'PY' | tee -a "$SUMMARY"
import re, sys
g, tag, ext, log, made, first = sys.argv[1:7]
fail = err = skip = 0
seeds, skipped = set(), set()
for l in open(log, errors='replace'):
    m = re.match(r'(FAIL|ERROR|SKIP)\s+(\S+)', l)
    if not m: continue
    kind, name = m.groups()
    mm = re.match(r'%s(\d+)\.%s' % (tag, ext), name)
    if kind == 'SKIP':
        skip += 1
        if mm: skipped.add(int(mm.group(1)))
    else:
        if kind == 'FAIL': fail += 1
        else: err += 1
        if mm: seeds.add(int(mm.group(1)))
        else: seeds.add(name)
print('%-12s programs=%-4s FAIL=%-3d ERROR=%-3d SKIP=%-3d (%d programs over the frame limit)  failing seeds: %s' % (
    g, made, fail, err, skip, len(skipped), ' '.join(map(str, sorted(seeds, key=lambda x: (isinstance(x, str), x if isinstance(x, int) else 0, str(x))))) or '-'))
sys.exit(1 if fail or err else 0)
PY
  return ${PIPESTATUS[0]}
}

echo "fuzz.sh: V32OPT=$V32OPT seeds $FIRST..$((FIRST + N - 1)) generators: $GENS"
IFS=',' read -ra GL <<< "$GENS"
for g in "${GL[@]}"; do
  run_gen "$g"; [ $? -ne 0 ] && rc=1
done
echo
echo "== summary (logs and programs in $BASEW/<tag>/) =="
cat "$SUMMARY"
[ $rc -ne 0 ] && echo "fuzz.sh: FAILED"
exit $rc
