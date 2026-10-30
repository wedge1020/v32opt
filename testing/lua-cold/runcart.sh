#!/bin/bash
# runcart.sh PROG.asm [WORDS@ADDR]  -- assemble PROG.asm as a cartridge, pack it with the minimal BIOS
# and run it headless in v32sim. Prints "insns=N" (instructions executed, from the address profile)
# and the dumped RAM words (default 1@2000, the benchmark's RESULT cell), one per line.
#   V32TOOLS  directory with the stock `assemble` and `packrom` (tests/ref/build.sh of v32sim builds them)
#   V32SIM    the simulator
HERE=$(cd "$(dirname "$0")" && pwd)
TOOLS=${V32TOOLS:-/home/claude/work/tools}
SIM=${V32SIM:-/home/claude/work/sim/bin/v32sim}
src=$1; dump=${2:-1@2000}
W=$(mktemp -d); trap 'rm -rf $W' EXIT
python3 - "$W/test.vtex" "$W/test.vsnd" <<'PY'
import struct, sys
open(sys.argv[1], "wb").write(b"V32-VTEX" + struct.pack("<II", 1, 1) + b"\xff\xff\xff\xff")
open(sys.argv[2], "wb").write(b"V32-VSND" + struct.pack("<I", 2) + struct.pack("<hhhh", 100, -100, 200, -200))
PY
pack() {  # type name asm
  local abase=""; [ "$1" = bios ] && abase="-b"
  "$TOOLS/assemble" $abase "$3" -o "$W/$2.vbin" >"$W/$2.log" 2>&1 || { echo "assemble failed:"; cat "$W/$2.log"; exit 2; }
  local tex="<textures />" snd="<sounds />"
  [ "$1" = bios ] && { tex='<textures><texture path="test.vtex" /></textures>'; snd='<sounds><sound path="test.vsnd" /></sounds>'; }
  cat > "$W/$2.xml" <<XML
<?xml version="1.0" encoding="UTF-8" standalone="no" ?>
<rom-definition version="1.0"><rom type="$1" title="$2" version="1.0" /><binary path="$2.vbin" />$tex$snd</rom-definition>
XML
  "$TOOLS/packrom" "$W/$2.xml" -o "$W/$2.v32" >"$W/$2.plog" 2>&1 || { echo "packrom failed:"; cat "$W/$2.plog"; exit 2; }
}
pack bios bios "$HERE/bios.asm"
pack cartridge prog "$src"
timeout 300 "$SIM" -q -r --fast -n -x -B "$W/bios.v32" --max-frames=2000 --dump="$dump" --profile-out="$W/prof" "$W/prog.v32" </dev/null >"$W/out" 2>"$W/err"
rc=$?
echo "insns=$(sed -n 's/^# cycles \([0-9]*\).*/\1/p' "$W/prof")  rc=$rc"
cat "$W/out" | head -40
[ $rc -ne 0 ] && head -3 "$W/err"
exit 0
