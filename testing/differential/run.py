#!/usr/bin/env python3
"""Differential test: C -> v32cc -> [v32opt flags] -> v32as -> v32sim.

usage: run.py [--flags "<v32opt flags>"]... file.c ...

Every C file is compiled once (the baseline, no v32opt) and then once per
--flags set. All builds run in the v32sim simulator; a variant passes when
the self-check counters agree (RAM[0] = failures must be 0, RAM[1] = number
of checks) and, with STRICT=1, when the whole RAM image matches the
baseline's. Cycle counts (one per instruction on Vircon32) and code sizes
are printed alongside, and a summary of the totals closes the report.

Needs the sibling projects built next to this one (v32cc, v32as, v32sim) --
see README.md in this directory for the variables that point elsewhere.
Test programs `#include "check.h"` (the v32cc tests' CHECK(); a copy is in
this directory) and end with CHECK(...) calls; gen.py makes such programs.
"""
import os, re, struct, subprocess, sys, collections
from concurrent.futures import ThreadPoolExecutor

HERE  = os.path.dirname(os.path.abspath(__file__))
ROOT  = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
V32CC  = os.environ.get('V32CC',  os.path.join(ROOT, 'v32cc', 'src', 'v32cc'))
V32AS  = os.environ.get('V32AS',  os.path.join(ROOT, 'v32as', 'bin', 'v32as'))
V32SIM = os.environ.get('V32SIM', os.path.join(ROOT, 'v32sim', 'bin', 'v32sim'))
V32OPT = os.environ.get('V32OPT', os.path.join(HERE, '..', '..', 'bin', 'v32opt'))
WORK   = os.environ.get('WORK', os.path.join(HERE, 'work'))
STRICT = bool(os.environ.get('STRICT'))
MAXF   = os.environ.get('MAXF', '40')       # frames (250000 cycles each) before a run counts as timed out
NDUMP  = 3002                               # RAM words read back
CYC    = 3000                               # RAM words 3000/3001 receive the cycle / frame counters

def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, timeout=600, **kw)

def v32_file(vbin, out):
    """Wrap a VBIN (3-word header) in a minimal .v32 (35-word header)."""
    d = open(vbin, 'rb').read()[12:]
    hdr = [0] * 0x23
    hdr[0x22] = len(d) // 4
    open(out, 'wb').write(struct.pack('<35I', *hdr) + d)

def bios_stub():
    path = os.path.join(WORK, 'bios.v32')
    if not os.path.exists(path):
        open(path, 'wb').write(struct.pack('<36I', *([0] * 0x22 + [1, 0])))   # header + one word: never run (-E)
    return path

_BASE = {}      # source -> (asm, error): each program is compiled once, before the pool starts

def build_base(src):
    if src in _BASE: return _BASE[src]
    base = os.path.join(WORK, os.path.splitext(os.path.basename(src))[0])
    asm = base + '.base.asm'
    r = sh([V32CC, '--standard', '--keep-data', '--data-base', '0', '-w',
            '-I', HERE, src, '-o', asm])
    _BASE[src] = (asm, None) if r.returncode == 0 else (None, 'v32cc: ' + (r.stdout + r.stderr)[:300])
    return _BASE[src]

def with_counters(text):
    """Store the cycle and frame counters just before the program's final HLT."""
    lines = text.split('\n')
    for i in range(len(lines) - 1, -1, -1):
        if lines[i].strip().lower() == 'hlt':
            lines[i:i+1] = ['  in R0, TIM_CycleCounter', '  mov [%d], R0' % CYC,
                            '  in R0, TIM_FrameCounter', '  mov [%d], R0' % (CYC + 1), '  hlt']
            break
    return '\n'.join(lines)

def run_one(src, flags, tag):
    asm, err = build_base(src)
    if err: return dict(err=err)
    base = asm.replace('.base.asm', '') + '.' + tag
    text = open(asm).read()
    if flags is not None:
        r = sh([V32OPT, asm, '-o', base + '.opt.asm'] + flags.split())
        if r.returncode: return dict(err='v32opt: ' + (r.stdout + r.stderr)[:300])
        text = open(base + '.opt.asm').read()
    final = base + '.asm'
    open(final, 'w').write(with_counters(text))
    r = sh([V32AS, '--standard', final, '-o', base + '.vbin'])
    if r.returncode: return dict(err='v32as: ' + (r.stdout + r.stderr)[:300])
    v32_file(base + '.vbin', base + '.v32')
    r = sh([V32SIM, '-r', '-x', '-n', '-B', bios_stub(), '-E', '0x20000000', '--frame-us=0',
            '--max-frames=' + MAXF, '--dump=%d' % NDUMP, base + '.v32'], stdin=subprocess.DEVNULL)
    if r.returncode == 2: return dict(err='TIMEOUT')
    if r.returncode != 0: return dict(err='v32sim rc=%d: %s' % (r.returncode, (r.stdout + r.stderr)[-300:]))
    vals = [int(l) for l in r.stdout.split('\n') if re.fullmatch(r'-?\d+', l.strip())]
    if len(vals) < NDUMP: return dict(err='short RAM dump')
    return dict(fails=vals[0], checks=vals[1], cyc=vals[CYC] + 250000 * vals[CYC + 1],
                words=(os.path.getsize(base + '.vbin') - 12) // 4, ram=vals[:CYC])

def main():
    args, sets = sys.argv[1:], []
    while args and args[0] == '--flags':
        sets.append(args[1]); args = args[2:]
    files = args
    if not files:
        print(__doc__); sys.exit(2)
    missing = [(n, p) for n, p in (('V32CC', V32CC), ('V32AS', V32AS), ('V32SIM', V32SIM), ('V32OPT', V32OPT))
               if not os.access(p, os.X_OK)]
    if missing:
        for n, p in missing: print('%s: %s is not an executable (build it, or point %s at it)' % (n, p, n))
        sys.exit(2)
    os.makedirs(WORK, exist_ok=True)
    allsets = [None] + sets
    tags = ['b'] + ['f%d' % i for i in range(len(sets))]
    for f in files: build_base(f)                       # (once per file, before the pool starts)
    jobs = [(f, fl, tg) for f in files for fl, tg in zip(allsets, tags)]
    with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as ex:
        res = list(ex.map(lambda j: run_one(*j), jobs))
    problems, k = 0, 0
    totals = collections.defaultdict(lambda: [0, 0])
    for f in files:
        base = res[k]; rows = []
        for fl in allsets:
            r = res[k]; k += 1
            name, label = os.path.basename(f), fl or '-O0 (baseline)'
            if r.get('err') == 'TIMEOUT' or base.get('err') == 'TIMEOUT':
                print('SKIP  %-14s %-52s timeout' % (name, label)); rows = None; continue
            if 'err' in r:
                print('ERROR %-14s %-52s %s' % (name, label, r['err'])); problems += 1; rows = None; continue
            same = 'ram' in base and r['ram'] == base['ram']
            ok = r['fails'] == 0 and r['checks'] == base['checks'] and (same or not STRICT)
            problems += not ok
            print('%-5s %-14s %-52s checks=%d cycles=%d words=%d%s' % (
                'ok' if ok else 'FAIL', name, label, r['checks'], r['cyc'], r['words'],
                '' if same else '  RAM differs from baseline'))
            if rows is not None: rows.append((label, r))
        if rows and len(rows) == len(allsets):
            for label, r in rows:
                totals[label][0] += r['cyc']; totals[label][1] += r['words']
    if totals:
        print('\ntotals over the programs where every variant ran:')
        b = totals['-O0 (baseline)']
        for label, (c, w) in totals.items():
            print('  %-52s cycles %12d (%+6.2f%%)  words %9d (%+6.2f%%)' % (
                label, c, 100.0 * (c - b[0]) / max(1, b[0]), w, 100.0 * (w - b[1]) / max(1, b[1])))
    print('%d problem(s)' % problems)
    sys.exit(1 if problems else 0)

main()
