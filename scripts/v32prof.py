#!/usr/bin/env python3
"""v32prof.py - make a line profile of an assembly program for `v32opt --profile`.

    v32prof.py PROGRAM.asm -o PROGRAM.prof [--frames N] [--args "..."]

The program is assembled with debug output (v32as -g), run headless in v32sim
for N frames (default 60) or until it halts, with the instruction profile
turned on (v32sim --profile-out), and the per-address counts are mapped back
to the lines of PROGRAM.asm:

    # v32opt line profile v1
    # total <instructions executed>
    <line> <times that line ran>

Give v32opt the SAME unoptimized file you profiled:
    v32opt PROGRAM.asm --profile PROGRAM.prof -O3

The run is made reproducible with --fixed-clock, --seed 1 and --fixed-cycles
(v32sim from the address-profile release on). A program that needs input to
reach its hot paths will only profile what it does without it: put the
workload in the program, or run it longer with --frames.

Tools: V32AS, V32SIM environment variables, else ../v32as and ../v32sim next
to v32opt, else the PATH.
"""
import argparse, collections, os, re, shutil, struct, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))


def tool(env, rel, name):
    p = os.environ.get(env)
    if p: return p
    c = os.path.join(ROOT, rel)
    if os.access(c, os.X_OK): return c
    return shutil.which(name)


def make_profile(asm, out, frames=60, entry='0x20000000'):
    """Profile ASM and write the line profile to OUT; returns (instructions executed, lines covered)."""
    v32as, v32sim = tool('V32AS', 'v32as/bin/v32as', 'v32as'), tool('V32SIM', 'v32sim/bin/v32sim', 'v32sim')
    if not v32as or not v32sim:
        raise RuntimeError('v32as and v32sim are needed (V32AS, V32SIM)')
    work = tempfile.mkdtemp(prefix='v32prof.')
    try:
        src = os.path.abspath(asm)
        vbin = os.path.join(work, 'p.vbin')
        r = subprocess.run([v32as, '--standard', '-g', src, '-o', vbin], capture_output=True, text=True)
        if r.returncode:
            raise RuntimeError('v32as failed:\n' + (r.stdout + r.stderr)[-400:])
        dbg = vbin + '.debug'
        if not os.path.exists(dbg): dbg = os.path.join(work, 'p.debug')
        if not os.path.exists(dbg):
            raise RuntimeError('v32as wrote no debug file')
        lines = {}
        base = os.path.basename(src)
        for row in open(dbg):
            f = row.rstrip('\n').split(',')
            if len(f) >= 3 and f[0].startswith('0x') and os.path.basename(f[1]) == base:
                lines[int(f[0], 16)] = int(f[2])
        data = open(vbin, 'rb').read()[12:]
        hdr = [0] * 0x23
        hdr[0x22] = len(data) // 4
        v32 = os.path.join(work, 'p.v32')
        open(v32, 'wb').write(struct.pack('<35I', *hdr) + data)
        bios = os.path.join(work, 'bios.v32')
        open(bios, 'wb').write(struct.pack('<36I', *([0] * 0x22 + [1, 0])))
        prof = os.path.join(work, 'p.prof')
        r = subprocess.run([v32sim, '-r', '-x', '-n', '-B', bios, '-E', entry, '--frame-us=0',
                            '--max-frames=%d' % frames, '--fixed-clock', '--seed=1', '--fixed-cycles',
                            '--profile-out=' + prof, v32], capture_output=True, text=True, stdin=subprocess.DEVNULL)
        if not os.path.exists(prof):
            raise RuntimeError('v32sim wrote no profile (does this v32sim have --profile-out?)\n' + (r.stdout + r.stderr)[-300:])
        counts = collections.Counter()
        total = 0
        for row in open(prof):
            if row.startswith('# cycles'):
                total = int(row.split()[2])
            elif row[0] != '#':
                addr, cnt = row.split()[:2]
                ln = lines.get(int(addr, 16))
                if ln: counts[ln] += int(cnt)
        with open(out, 'w') as fh:
            fh.write('# v32opt line profile v1\n# total %d\n' % total)
            for ln in sorted(counts):
                fh.write('%d %d\n' % (ln, counts[ln]))
        return total, len(counts)
    finally:
        shutil.rmtree(work, ignore_errors=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('asm')
    ap.add_argument('-o', '--output', help='profile file (default: the program with a .prof extension)')
    ap.add_argument('--frames', type=int, default=60, help='frames to run (default 60; a halting program stops earlier)')
    ap.add_argument('--entry', default='0x20000000', help='entry point (default 0x20000000, the cartridge start)')
    a = ap.parse_args()
    out = a.output or os.path.splitext(a.asm)[0] + '.prof'
    try:
        total, n = make_profile(a.asm, out, a.frames, a.entry)
    except RuntimeError as e:
        sys.exit('v32prof: %s' % e)
    print('v32prof: %d instructions executed, %d lines covered -> %s' % (total, n, out))


if __name__ == '__main__':
    main()
