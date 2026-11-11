#!/bin/bash
# runcart.sh PROG.asm [WORDS@ADDR]  -- assemble PROG.asm as a cartridge, pack it with the minimal BIOS
# and run it headless in v32sim. Prints "insns=N" (instructions executed, from the address profile)
# and the dumped RAM words (default 1@2000, the benchmark's RESULT cell), one per line.
#   V32TOOLS  directory with the stock `assemble` and `packrom` (/home/claude/stocktools has them); optional:
#             without them v32as and a stub BIOS are used (see below)
#   V32SIM, V32AS  the simulator, the assembler (default: the siblings of this project)
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../.." && pwd)
SIM=${V32SIM:-$ROOT/v32sim/bin/v32sim}
AS=${V32AS:-$ROOT/v32as/bin/v32as}
TOOLS=${V32TOOLS:-}
src=$1; dump=${2:-1@2000}
W=$(mktemp -d); trap 'rm -rf $W' EXIT
# Without the stock tools (V32TOOLS unset, or no `assemble`/`packrom` there) the program is assembled with v32as,
# wrapped in a cartridge by the python below (same 128-byte header as differential/run.py) and started at the
# cartridge entry (-E) behind a stub BIOS; bios.asm is not needed then, it only jumps to the cartridge.
STOCK=0; [ -n "$TOOLS" ] && [ -x "$TOOLS/assemble" ] && [ -x "$TOOLS/packrom" ] && STOCK=1
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
if [ $STOCK = 1 ]; then
  python3 - "$W/test.vtex" "$W/test.vsnd" <<'PY'
import struct, sys
open(sys.argv[1], "wb").write(b"V32-VTEX" + struct.pack("<II", 1, 1) + b"\xff\xff\xff\xff")
open(sys.argv[2], "wb").write(b"V32-VSND" + struct.pack("<I", 2) + struct.pack("<hhhh", 100, -100, 200, -200))
PY
  pack bios bios "$HERE/bios.asm"
  pack cartridge prog "$src"
  RUNOPTS="-B $W/bios.v32"
else
  "$AS" ${V32ASFLAGS---standard} "$src" -o "$W/prog.vbin" >"$W/prog.log" 2>&1 || { echo "assemble failed:"; cat "$W/prog.log"; exit 2; }
  python3 - "$W/prog.vbin" "$W/prog.v32" "$W/bios.v32" <<'PY'
import struct, sys
def hdr(sig, ntex, nsnd, prog, vid, aud):
    w = [0] * 32
    w[2], w[3] = 1, 0
    w[22], w[23] = ntex, nsnd
    w[24], w[25] = 128, prog
    w[26], w[27] = 128 + prog, vid
    w[28], w[29] = 128 + prog + vid, aud
    return sig + struct.pack('<32I', *w)[8:]
d = open(sys.argv[1], 'rb').read()
open(sys.argv[2], 'wb').write(hdr(b'V32-CART', 0, 0, len(d), 0, 0) + d)
prog = b'V32-VBIN' + struct.pack('<2I', 1, 0)
tex  = b'V32-VTEX' + struct.pack('<3I', 1, 1, 0)
snd  = b'V32-VSND' + struct.pack('<2I', 1, 0)
open(sys.argv[3], 'wb').write(hdr(b'V32-BIOS', 1, 1, len(prog), len(tex), len(snd)) + prog + tex + snd)
PY
  RUNOPTS="-B $W/bios.v32 -E 0x20000000"
fi
timeout 300 "$SIM" -q -r --fast -n -x $RUNOPTS --fixed-clock --seed=1 --max-frames=2000 --dump="$dump" --profile-out="$W/prof" "$W/prog.v32" </dev/null >"$W/out" 2>"$W/err"
rc=$?
echo "insns=$(sed -n 's/^# cycles \([0-9]*\).*/\1/p' "$W/prof")  rc=$rc"
cat "$W/out" | head -40
[ $rc -ne 0 ] && head -3 "$W/err"
exit 0
