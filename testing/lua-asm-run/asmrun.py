#!/usr/bin/env python3
"""asmrun.py - run v32lua-compiled assembly in v32sim and print its result globals.

usage: asmrun.py [--opt V32OPT] [--timeout SEC] [--no-lua] FILE.asm [-- V32OPT-FLAGS...]

Without flags after "--" the file is assembled and run as it is (the baseline). With flags, v32opt
is run first (with -L lua unless --no-lua) and its output is assembled and run. The program's
results are the globals named r_* (the '%define var_r_name addr' lines v32lua writes); the line
printed is  "name=value name=value ..."  in file order. A string literal is a pointer into the
ROM, which moves when the code changes size: its text is printed instead.
Exit status: 0 done; 1 a tool failed (message on stdout, starting with ERROR); 3 v32opt timed
out (prints TIMEOUT); 4 the simulator timed out.
Used by the run.sh of lua-function-bounds, lua-loop-var and liveness-trust: it does the job of
differential/run_lua.py for an .asm that is checked in (no Lua compiler is needed).
Needs v32as and v32sim built next to this project (V32AS, V32SIM override).
"""
import os, re, shutil, struct, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
V32AS = os.environ.get('V32AS', os.path.join(ROOT, 'v32as', 'bin', 'v32as'))
V32SIM = os.environ.get('V32SIM', os.path.join(ROOT, 'v32sim', 'bin', 'v32sim'))
MAXF = os.environ.get('MAXF', '40')
NDUMP = 3002


def _hdr(sig, ntex, nsnd, prog, vid, aud):
    w = [0] * 32
    w[2], w[3] = 1, 0
    w[22], w[23] = ntex, nsnd
    w[24], w[25] = 128, prog
    w[26], w[27] = 128 + prog, vid
    w[28], w[29] = 128 + prog + vid, aud
    return sig + struct.pack('<32I', *w)[8:]


def sh(cmd, timeout=300):
    return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, stdin=subprocess.DEVNULL)


def main():
    a = sys.argv[1:]
    opt = os.environ.get('V32OPT', os.path.join(HERE, '..', '..', 'bin', 'v32opt'))
    tmo, lua = 120, True
    while a and a[0].startswith('--') and a[0] != '--':
        if a[0] == '--opt': opt = a[1]; a = a[2:]
        elif a[0] == '--timeout': tmo = int(a[1]); a = a[2:]
        elif a[0] == '--no-lua': lua = False; a = a[1:]
        else: print('ERROR bad option', a[0]); return 1
    if not a: print(__doc__); return 1
    src, flags = a[0], (a[2:] if len(a) > 1 and a[1] == '--' else [])
    tmp = tempfile.mkdtemp(prefix='asmrun.')
    try:
        return run(src, flags, opt, tmo, lua, tmp)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def run(src, flags, opt, tmo, lua, tmp):
    asm = src
    if flags:
        asm = os.path.join(tmp, 'opt.asm')
        try:
            r = sh([opt, src, '-o', asm] + (['-L', 'lua'] if lua else []) + flags, timeout=tmo)
        except subprocess.TimeoutExpired:
            print('TIMEOUT'); return 3
        if r.returncode: print('ERROR v32opt rc=%d: %s' % (r.returncode, (r.stdout + r.stderr)[:200].replace('\n', ' '))); return 1
    names = re.findall(r'%define\s+var_r_(\w+)\s+(0x[0-9A-Fa-f]+|\d+)', open(src).read())
    vbin, v32 = os.path.join(tmp, 'p.vbin'), os.path.join(tmp, 'p.v32')
    r = sh([V32AS, '--standard', asm, '-o', vbin])
    if r.returncode: print('ERROR v32as: ' + (r.stdout + r.stderr)[:200].replace('\n', ' ')); return 1
    d = open(vbin, 'rb').read()
    open(v32, 'wb').write(_hdr(b'V32-CART', 0, 0, len(d), 0, 0) + d)
    bios = os.path.join(tmp, 'bios.v32')
    prog = b'V32-VBIN' + struct.pack('<2I', 1, 0)
    tex = b'V32-VTEX' + struct.pack('<3I', 1, 1, 0)
    snd = b'V32-VSND' + struct.pack('<2I', 1, 0)
    open(bios, 'wb').write(_hdr(b'V32-BIOS', 1, 1, len(prog), len(tex), len(snd)) + prog + tex + snd)
    base = [V32SIM, '-r', '-x', '-n', '-B', bios, '-E', '0x20000000', '--frame-us=0', '--fixed-clock', '--seed=1']
    r = sh(base + ['--max-frames=' + MAXF, '--dump=%d' % NDUMP, v32])
    if r.returncode == 2: print('TIMEOUT sim'); return 4
    if r.returncode: print('ERROR v32sim rc=%d' % r.returncode); return 1
    vals = [int(l) for l in r.stdout.split('\n') if re.fullmatch(r'-?\d+', l.strip())]
    if len(vals) < NDUMP: print('ERROR short RAM dump'); return 1
    res = [(n, vals[int(x, 0)]) for n, x in names]
    if any((v & 0xFFC00000) == 0x7FC00000 for _, v in res):
        rom = sh(base + ['--max-frames=1', '--dump=%d@0x20000000' % (os.path.getsize(vbin) // 4 + 8), v32])
        words = [int(l) for l in rom.stdout.split('\n') if re.fullmatch(r'-?\d+', l.strip())]
        def deref(v):
            if (v & 0xFFC00000) != 0x7FC00000: return str(v)
            off, text = v & 0x3FFFFF, []
            while off < len(words) and words[off] and len(text) < 4096:
                text.append(words[off]); off += 1
            return 'str:' + ','.join(map(str, text))
        res = [(n, deref(v)) for n, v in res]
    print(' '.join('%s=%s' % (n, v) for n, v in res))
    return 0


sys.exit(main())
