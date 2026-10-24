#!/usr/bin/env python3
"""bench.py - compare implementations of one program across languages,
compilers and v32opt settings: code size, cycles, and whether the answer is
right.

    bench.py SUITE_DIR [options]

SUITE_DIR holds implementations of the SAME program, one file each, in any
mix of
    *.asm   hand-written assembly            (assembled as is)
    *.c     C                                (v32cc, and the stock Vircon32 `compile`)
    *.cpp   C++                              (v32c++ -> C -> the stock compiler)
    *.lua   Lua                              (v32lua)
The contract is small: the program runs from start to HLT with no I/O and
stores its answer in a global named `result` (Lua: `r_result`; assembly:
`%define global_result <ram address>`). Every build is run in v32sim; the
cycle count is the number of instructions executed (one cycle each) from
the entry point to the HLT, and the size is the assembled program in words.
Every answer must equal the expected one (`--expect N`, or a file
`expect.txt` in the suite, or else the first build's) or the row is marked
WRONG instead of being compared.

Each implementation is built once per toolchain and then once per v32opt
scenario: the unoptimized build (-O0), -O1, -O2, -O3, -Os, and any
`--scenario NAME="FLAGS"` of your own, e.g.
    --scenario O3-no-unroll="-O3 -fno-loop-unroll -fno-loop-unroll-partial"

Reports (all relative to a BASELINE row, `--baseline impl/toolchain/scenario`,
default: the first implementation's first toolchain unoptimized):
  1. per implementation and toolchain, every scenario against its own -O0;
  2. all implementations side by side, each at its best scenario
     (fewest cycles), against the baseline.
`--csv FILE` / `--md FILE` / `--json FILE` write the numbers out.

Tools: v32as and v32sim are needed; v32opt unless `--no-opt`; v32cc, the
stock `compile`, v32c++ and v32lua only for the files that need them (a
missing one just skips those rows). Locations: the V32AS, V32SIM, V32OPT,
V32CC, V32STOCK, V32CXX, V32LUA environment variables, else the sibling
projects next to v32opt, else the PATH.
"""
import argparse, collections, csv, fnmatch, glob, json, os, re, shutil, struct, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
OPT_ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
ROOT = os.path.dirname(OPT_ROOT)
sys.path.insert(0, os.path.join(OPT_ROOT, 'testing', 'differential'))

NDUMP, CYC = 3002, 3000


def find_tool(env, rels, names):
    p = os.environ.get(env)
    if p: return p
    for r in rels:
        c = os.path.join(ROOT, r)
        if os.access(c, os.X_OK): return c
    for n in names:
        w = shutil.which(n)
        if w: return w
    return None


TOOLS = {
    'as':    find_tool('V32AS',  ['v32as/bin/v32as'], ['v32as']),
    'sim':   find_tool('V32SIM', ['v32sim/bin/v32sim'], ['v32sim']),
    'opt':   find_tool('V32OPT', ['v32opt/bin/v32opt'], ['v32opt']),
    'cc':    find_tool('V32CC',  ['v32cc/src/v32cc', 'v32cc/bin/v32cc'], ['v32cc']),
    'stock': find_tool('V32STOCK', [], ['compile']),
    'cxx':   find_tool('V32CXX', ['v32cxx/bin/v32c++', 'wedge1020/v32cxx/bin/v32c++'], ['v32c++']),
    'lua':   find_tool('V32LUA', ['v32lua/bin/v32lua', 'wedge1020/v32lua/bin/v32lua'], ['v32lua']),
}


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, timeout=1800, **kw)


# --------------------------------------------------------------------------
# building
# --------------------------------------------------------------------------
class Err(Exception):
    pass


def tail(r, n=240):
    return (r.stdout + r.stderr).strip()[-n:]


def to_stock(src):
    from tostock import to_stock as ts
    return ts(src)


def base_asm(impl, tool, work):
    """Source -> assembly, by the given toolchain. Returns the .asm path."""
    name = os.path.splitext(os.path.basename(impl))[0]
    out = os.path.join(work, '%s.%s.base.asm' % (name, tool))
    if tool == 'asm':
        shutil.copy(impl, out)
    elif tool == 'v32cc':
        r = sh([TOOLS['cc'], '--standard', '--keep-data', '--data-base', '0', '-w', impl, '-o', out])
        if r.returncode: raise Err('v32cc: ' + tail(r))
    elif tool == 'stock':
        src = os.path.join(work, '%s.%s.c' % (name, tool))
        open(src, 'w').write(to_stock(open(impl).read()))
        r = sh([TOOLS['stock'], '-w', '-o', out, src])
        if r.returncode: raise Err('compile: ' + tail(r))
    elif tool == 'v32c++':
        c = os.path.join(work, '%s.%s.c' % (name, tool))
        r = sh([TOOLS['cxx'], '--no-build-scripts', '-o', c, impl])
        if r.returncode: raise Err('v32c++: ' + tail(r))
        r = sh([TOOLS['stock'], '-w', '-o', out, c])
        if r.returncode: raise Err('compile: ' + tail(r))
    elif tool == 'v32lua':
        r = sh([TOOLS['lua'], '-w', '--no-build-scripts', '-o', out, impl])
        if r.returncode: raise Err('v32lua: ' + tail(r))
    else:
        raise Err('unknown toolchain ' + tool)
    return out


def toolchains_for(impl):
    ext = os.path.splitext(impl)[1].lower()
    return {'.asm': ['asm'], '.c': ['v32cc', 'stock'], '.cpp': ['v32c++'], '.lua': ['v32lua']}.get(ext, [])


def needed(tool):
    return {'asm': [], 'v32cc': ['cc'], 'stock': ['stock'], 'v32c++': ['cxx', 'stock'], 'v32lua': ['lua']}[tool]


def with_counters(text, tool):
    """Store the cycle and frame counters just before the HLT that ends the run."""
    lines = text.split('\n')
    idx = [i for i, l in enumerate(lines) if l.strip().lower().startswith('hlt')]
    if not idx: raise Err('no HLT in the program')
    i = idx[0] if tool in ('asm', 'v32lua') else idx[-1]
    lines[i:i + 1] = ['  in R0, TIM_CycleCounter', '  mov [%d], R0' % CYC,
                      '  in R0, TIM_FrameCounter', '  mov [%d], R0' % (CYC + 1), '  hlt']
    return '\n'.join(lines)


def result_address(asm_text):
    m = re.search(r'%define\s+(?:global_result|var_r_result)\s+(0x[0-9A-Fa-f]+|\d+)', asm_text)
    if not m: raise Err('no `result` global found (C: int result; Lua: r_result; asm: %define global_result N)')
    return int(m.group(1), 0)


def bios_stub(work):
    p = os.path.join(work, 'bios.v32')
    if not os.path.exists(p):
        open(p, 'wb').write(struct.pack('<36I', *([0] * 0x22 + [1, 0])))
    return p


def build_and_run(impl, tool, scen, flags, work, maxf):
    """-> dict(words, cycles, result) or dict(err=...)"""
    try:
        name = os.path.splitext(os.path.basename(impl))[0]
        tag = '%s.%s.%s' % (name, tool, re.sub(r'[^A-Za-z0-9_.-]', '_', scen))
        asm = BASE[(impl, tool)]
        if isinstance(asm, Err): raise asm
        text = open(asm).read()
        addr = result_address(text)
        if flags is not None:
            opt = os.path.join(work, tag + '.opt.asm')
            cmd = [TOOLS['opt'], asm, '-o', opt] + flags.split()
            if tool == 'v32lua': cmd += ['-L', 'lua']
            r = sh(cmd)
            if r.returncode: raise Err('v32opt: ' + tail(r))
            text = open(opt).read()
        final = os.path.join(work, tag + '.asm')
        open(final, 'w').write(with_counters(text, tool))
        vbin = os.path.join(work, tag + '.vbin')
        r = sh([TOOLS['as'], '--standard', final, '-o', vbin])
        if r.returncode: raise Err('v32as: ' + tail(r))
        data = open(vbin, 'rb').read()[12:]
        hdr = [0] * 0x23
        hdr[0x22] = len(data) // 4
        v32 = os.path.join(work, tag + '.v32')
        open(v32, 'wb').write(struct.pack('<35I', *hdr) + data)
        r = sh([TOOLS['sim'], '-r', '-x', '-n', '-B', bios_stub(work), '-E', '0x20000000', '--frame-us=0',
                '--max-frames=%d' % maxf, '--dump=%d' % NDUMP, v32], stdin=subprocess.DEVNULL)
        if r.returncode == 2: raise Err('TIMEOUT (%d frames)' % maxf)
        if r.returncode != 0: raise Err('v32sim rc=%d: %s' % (r.returncode, tail(r)))
        vals = [int(l) for l in r.stdout.split('\n') if re.fullmatch(r'-?\d+', l.strip())]
        if len(vals) < NDUMP: raise Err('short RAM dump')
        value = vals[addr]
        if tool == 'v32lua':                        # Lua numbers are 32-bit floats
            f = struct.unpack('<f', struct.pack('<i', value))[0]
            value = int(f) if f == f and abs(f) < 2 ** 31 and f == int(f) else value
        return dict(words=len(data) // 4, cycles=vals[CYC] + 250000 * vals[CYC + 1], result=value)
    except Err as e:
        return dict(err=str(e))
    except subprocess.TimeoutExpired:
        return dict(err='TIMEOUT (host)')


BASE = {}


# --------------------------------------------------------------------------
# reporting
# --------------------------------------------------------------------------
def pct(a, b):
    return 100.0 * (a - b) / b if b else float('nan')


def fmt_row(cells, widths):
    return '  '.join(str(c).rjust(w) if i else str(c).ljust(w) for i, (c, w) in enumerate(zip(cells, widths)))


def table(rows, header):
    widths = [max(len(str(x)) for x in col) for col in zip(header, *rows)]
    out = [fmt_row(header, widths), fmt_row(['-' * w for w in widths], widths)]
    out += [fmt_row(r, widths) for r in rows]
    return '\n'.join(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('suite')
    ap.add_argument('--impl', action='append', help='only these implementation files (glob on the base name)')
    ap.add_argument('--scenarios', default='O1,O2,O3,Os', help='comma list of v32opt levels (default O1,O2,O3,Os)')
    ap.add_argument('--scenario', action='append', default=[], metavar='NAME="FLAGS"', help='an extra v32opt setting')
    ap.add_argument('--no-opt', action='store_true', help='only the unoptimized builds')
    ap.add_argument('--baseline', help='impl/toolchain/scenario (default: first impl, first toolchain, -O0)')
    ap.add_argument('--expect', type=int, help='the expected `result`')
    ap.add_argument('--maxf', type=int, default=int(os.environ.get('MAXF', '4000')), help='frames before a run counts as timed out')
    ap.add_argument('-j', type=int, default=os.cpu_count() or 4)
    ap.add_argument('--work', help='work directory (default: a temporary one, removed afterwards)')
    ap.add_argument('--csv'); ap.add_argument('--md'); ap.add_argument('--json')
    a = ap.parse_args()

    for t in ('as', 'sim'):
        if not TOOLS[t]:
            sys.exit('bench: %s not found (set V32%s)' % (t, t.upper()))
    if not a.no_opt and not TOOLS['opt']:
        sys.exit('bench: v32opt not found (set V32OPT, or use --no-opt)')

    files = sorted(f for ext in ('asm', 'c', 'cpp', 'lua') for f in glob.glob(os.path.join(a.suite, '*.' + ext)))
    if a.impl:
        files = [f for f in files if any(fnmatch.fnmatch(os.path.splitext(os.path.basename(f))[0], p) for p in a.impl)]
    if not files: sys.exit('bench: no implementations in ' + a.suite)

    scen = [('-O0', None)]
    if not a.no_opt:
        for lv in [s for s in a.scenarios.split(',') if s]:
            scen.append(('-' + lv, '-' + lv))
        for s in a.scenario:
            n, _, f = s.partition('=')
            scen.append((n, f.strip('"\' ')))

    expect = a.expect
    ef = os.path.join(a.suite, 'expect.txt')
    if expect is None and os.path.exists(ef):
        expect = int(open(ef).read().split()[0])

    work = a.work or tempfile.mkdtemp(prefix='v32bench.')
    os.makedirs(work, exist_ok=True)

    builds = []                                   # (impl, tool)
    skipped = []
    for f in files:
        for tool in toolchains_for(f):
            miss = [n for n in needed(tool) if not TOOLS[n]]
            if miss: skipped.append((f, tool, miss)); continue
            builds.append((f, tool))
    for f, tool, miss in skipped:
        print('skip %s with %s: %s not found' % (os.path.basename(f), tool, ', '.join(miss)))

    for f, tool in builds:                         # sources -> assembly, once each
        try: BASE[(f, tool)] = base_asm(f, tool, work)
        except Err as e: BASE[(f, tool)] = e

    jobs = [(f, tool, sname, flags) for f, tool in builds for sname, flags in scen]
    with ThreadPoolExecutor(max_workers=a.j) as ex:
        res = list(ex.map(lambda j: build_and_run(j[0], j[1], j[2], j[3], work, a.maxf), jobs))
    R = collections.OrderedDict()
    for j, r in zip(jobs, res):
        R[(os.path.splitext(os.path.basename(j[0]))[0], j[1], j[2])] = r

    if expect is None:
        for r in R.values():
            if 'err' not in r: expect = r['result']; break
    for r in R.values():
        if 'err' not in r and expect is not None and r['result'] != expect:
            r['wrong'] = True

    keys = list(R.keys())
    base_key = None
    if a.baseline:
        base_key = tuple(a.baseline.split('/'))
        if base_key not in R: sys.exit('bench: no such baseline row: ' + a.baseline)
    elif keys:
        base_key = keys[0]
    b = R.get(base_key) if base_key else None
    if not b or 'err' in b:
        print('bench: the baseline row failed: %s' % (b or {}).get('err', 'missing'))
        b = None

    out = []
    out.append('suite: %s    expected result: %s    baseline: %s' % (a.suite, expect, '/'.join(base_key) if base_key else '-'))
    out.append('')
    # 1. each implementation/toolchain: scenarios against its own -O0
    groups = collections.OrderedDict()
    for k in keys: groups.setdefault(k[:2], []).append(k)
    for g, ks in groups.items():
        o0 = R[g + ('-O0',)]
        out.append('== %s / %s' % g)
        rows = []
        for k in ks:
            r = R[k]
            if 'err' in r:
                rows.append([k[2], '-', '-', '-', '-', r['err'][:70]]); continue
            if r.get('wrong'):
                rows.append([k[2], r['words'], r['cycles'], '-', '-', 'WRONG result %s' % r['result']]); continue
            if 'err' in o0 or o0.get('wrong'):
                rows.append([k[2], r['words'], r['cycles'], '', '', '']); continue
            rows.append([k[2], r['words'], r['cycles'], '%+.1f%%' % pct(r['words'], o0['words']),
                         '%+.1f%%' % pct(r['cycles'], o0['cycles']), ''])
        out.append(table(rows, ['scenario', 'words', 'cycles', 'size vs -O0', 'cycles vs -O0', 'note']))
        out.append('')
    # 2. everything side by side at each build's best scenario
    out.append('== all builds, each at its fastest scenario (against the baseline)')
    best = []
    for g, ks in groups.items():
        good = [k for k in ks if 'err' not in R[k] and not R[k].get('wrong')]
        if not good: continue
        k = min(good, key=lambda k: (R[k]['cycles'], R[k]['words']))
        best.append(k)
    best.sort(key=lambda k: R[k]['cycles'])
    rows = []
    for k in best:
        r = R[k]
        row = ['%s / %s' % k[:2], k[2], r['words'], r['cycles']]
        if b:
            row += ['%.2fx' % (b['cycles'] / r['cycles']) if r['cycles'] else '-',
                    '%+.1f%%' % pct(r['words'], b['words'])]
        rows.append(row)
    hdr = ['build', 'scenario', 'words', 'cycles'] + (['speed vs base', 'size vs base'] if b else [])
    out.append(table(rows, hdr))
    out.append('')
    # the same, but every build unoptimized: what the compilers alone give
    out.append('== all builds unoptimized (-O0)')
    rows = []
    for g in groups:
        r = R[g + ('-O0',)]
        if 'err' in r or r.get('wrong'): continue
        row = ['%s / %s' % g, r['words'], r['cycles']]
        if b: row += ['%.2fx' % (b['cycles'] / r['cycles']), '%+.1f%%' % pct(r['words'], b['words'])]
        rows.append((r['cycles'], row))
    out.append(table([r for _, r in sorted(rows)], ['build', 'words', 'cycles'] + (['speed vs base', 'size vs base'] if b else [])))
    text = '\n'.join(out)
    print(text)

    flat = [dict(impl=k[0], toolchain=k[1], scenario=k[2], **R[k]) for k in keys]
    if a.csv:
        with open(a.csv, 'w', newline='') as fh:
            w = csv.DictWriter(fh, fieldnames=['impl', 'toolchain', 'scenario', 'words', 'cycles', 'result', 'wrong', 'err'], extrasaction='ignore')
            w.writeheader(); w.writerows(flat)
    if a.json:
        json.dump(flat, open(a.json, 'w'), indent=1)
    if a.md:
        open(a.md, 'w').write('```\n' + text + '\n```\n')
    if not a.work:
        shutil.rmtree(work, ignore_errors=True)
    bad = [k for k in keys if 'err' in R[k] or R[k].get('wrong')]
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
