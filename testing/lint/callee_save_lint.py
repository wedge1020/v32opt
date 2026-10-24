#!/usr/bin/env python3
"""
callee_save_lint.py - static check for the clobbered-register bug class.

The stock Vircon32 C compiler is callee-save: a caller keeps live values in
R1..R8 across a CALL, so a function that writes one of R1..R10 must PUSH it
first.  v32opt's promote passes (and anything else that introduces a register)
must keep to that.  The compiler's own output is not perfectly clean (a
function that is only ever called at statement level may write R1 without
saving it), so this compares against the input:

    callee_save_lint.py INPUT.asm OPTIMIZED.asm [OPTIMIZED2.asm ...]

A "violation" is a (function, register) pair written but not pushed in that
function.  Any violation in an optimized file that is not in the input is
reported and the exit status is 1.  Inlining a callee that itself wrote an
unsaved register moves the write into the caller; that is covered: a function
is excused from what its (transitive) callees in the input wrote unsaved.

Registers written: the first operand of every instruction except PUSH, OUT,
the jumps, CALL, RET, HLT, WAIT, CMPS/MOVS/SETS (R11..R13 are not checked
anyway).  Memory destinations ([R1], [BP-2]) are not register writes.
"""
import re
import sys

NOWRITE = {'PUSH', 'POP', 'OUT', 'JMP', 'JT', 'JF', 'CALL', 'RET', 'HLT',
           'WAIT', 'CMPS', 'MOVS', 'SETS'}
FUNC = re.compile(r'^(__function_\w+?):$')
REG = re.compile(r'R(\d+)$', re.I)


def analyse(path):
    fn = None
    pushed, written, calls = {}, {}, {}
    for line in open(path):
        text = line.split(';')[0].strip()
        if not text:
            continue
        m = FUNC.match(text)
        if m and not m.group(1).endswith('_return'):
            fn = m.group(1)
            pushed[fn], written[fn], calls[fn] = set(), set(), set()
            continue
        if fn is None or text.endswith(':'):
            continue
        parts = text.split(None, 1)
        op = parts[0].upper()
        args = [a.strip() for a in parts[1].split(',')] if len(parts) > 1 else []
        reg = REG.match(args[0]) if args else None
        if op == 'PUSH' and reg:
            pushed[fn].add(int(reg.group(1)))
        elif op == 'CALL' and args and args[0].startswith('__function_'):
            calls[fn].add(args[0])
        elif op == 'POP' and reg:
            written[fn].add(int(reg.group(1)))      # POP is a write, but it
                                                    # is only fine if pushed
        elif op not in NOWRITE and reg:
            written[fn].add(int(reg.group(1)))
    viol = {(f, 'R%d' % r) for f in written for r in written[f] - pushed[f]
            if 1 <= r <= 10}
    return viol, calls


def inherited(base_viol, calls):
    """(function, register) pairs that inlining a callee may legitimately
    move into the caller: whatever a transitive callee already wrote unsaved."""
    by_fn = {}
    for f, r in base_viol:
        by_fn.setdefault(f, set()).add(r)
    out = set()
    for f in calls:
        seen, todo = set(), list(calls[f])
        while todo:
            g = todo.pop()
            if g in seen:
                continue
            seen.add(g)
            todo.extend(calls.get(g, ()))
            for r in by_fn.get(g, ()):
                out.add((f, r))
    return out


def main(argv):
    if len(argv) < 3:
        sys.exit(__doc__)
    base, calls = analyse(argv[1])
    base = base | inherited(base, calls)
    bad = 0
    for path in argv[2:]:
        new = sorted(analyse(path)[0] - base)
        for fn, r in new:
            print('%s: %s writes %s without saving it' % (path, fn, r))
        bad += len(new)
    return 1 if bad else 0


sys.exit(main(sys.argv))
