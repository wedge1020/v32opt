#!/usr/bin/env python3
"""Register-pressure programs for callee-save checking (see README.md).

usage: genreg.py SEED OUTFILE.c

The stock Vircon32 C compiler keeps live temporaries in R1, R2, ... across a
CALL and relies on every callee to save the registers it uses. This makes a
few leaf and non-leaf functions with several locals and loops (so the
promote passes want registers for them) and calls them from deeply nested
expressions, where up to a dozen partial results are alive in registers at
the time of each call. A callee that clobbers a register it did not save
shows up as a different result.

GENREG_LIVE=1 adds, to each function, a loop whose body calls the callee (mid functions) or
does arithmetic (leaf functions), carries a value from the end of one iteration to the top
of the next, and leaves through a break and through a return from inside the loop that read
different locals. The default output for a seed is unchanged without it.
"""
import os, random, sys
R = random.Random(int(sys.argv[1]))
LIVE = bool(os.environ.get('GENREG_LIVE'))
NL = R.randint(2, 4)          # leaf functions
NM = R.randint(1, 2)          # functions that call a leaf

def body(name, nloc, callee):
    loc = ['l%d' % i for i in range(nloc)]
    s = 'int %s(int p, int q) {\n    int %s, i, j;\n' % (name, ', '.join(loc))
    for i, l in enumerate(loc):
        s += '    %s = %s;\n' % (l, R.choice(['p', 'q', 'p + %d' % i, 'q ^ %d' % (i + 1), '%d' % i]))
    for _ in range(R.randint(1, 2)):
        n = R.choice([2, 3, 4])
        s += '    for (i = 0; i < %d; i++) {\n' % n
        for _ in range(R.randint(2, 4)):
            a, b, c = R.choice(loc), R.choice(loc), R.choice(loc)
            s += '        %s = %s %s (%s %s i);\n' % (a, b, R.choice(['+', '^', '-', '|']), c, R.choice(['+', '^', '&']))
        if R.random() < 0.5:
            s += '        for (j = 0; j < 2; j++) %s += %s + j;\n' % (R.choice(loc), R.choice(loc))
        s += '    }\n'
    if LIVE:
        c, t, x, y = [R.choice(loc) for _ in range(4)]
        s += '    %s = %s;\n    for (i = 0; i < 3; i++) {\n        %s = %s + %s;\n' % (c, R.choice(loc), t, c, R.choice(loc))
        if callee: s += '        %s = %s(%s, %s);\n' % (R.choice(loc), callee, t, R.choice(loc))
        else: s += '        %s = %s * 3 + i;\n' % (R.choice(loc), t)
        s += '        if (%s > %d) break;\n        if (%s == %d) return %s + %s;\n        %s = %s ^ %s;\n    }\n' % (
            x, R.choice([20, 50, 200]), y, R.choice([3, 7, 11]), R.choice(loc), R.choice(loc), c, t, R.choice(loc))
    if callee:
        s += '    %s = %s(%s, %s);\n' % (R.choice(loc), callee, R.choice(loc), R.choice(loc))
        for _ in range(2):
            a, b = R.choice(loc), R.choice(loc)
            s += '    %s = %s + %s;\n' % (a, a, b)
    s += '    return %s;\n}\n\n' % ' + '.join(R.sample(loc, min(len(loc), 3)))
    return s

fnames = ['leaf%d' % i for i in range(NL)] + ['mid%d' % i for i in range(NM)]
out = '#include "check.h"\nint result;\nint g0, g1, g2;\n\n'
for i in range(NL):
    out += body('leaf%d' % i, R.randint(3, 6), None)
for i in range(NM):
    out += body('mid%d' % i, R.randint(3, 5), 'leaf%d' % R.randrange(NL))

def expr(d):
    if d == 0:
        c = R.random()
        if c < 0.5: return '%s(%d, %d)' % (R.choice(fnames), R.randint(0, 5), R.randint(0, 5))
        return R.choice(['g0', 'g1', 'g2', str(R.randint(1, 9))])
    # the call sits deep on the right, so everything to its left is live across it
    left = R.choice(['g0', 'g1', 'g2', str(R.randint(1, 9)), '%s(%d, %d)' % (R.choice(fnames), R.randint(0, 3), R.randint(0, 3))])
    return '(%s %s %s)' % (left, R.choice(['+', '^', '*', '-']), expr(d - 1))

out += 'int main(void) {\n    int k;\n    g0 = 3; g1 = 5; g2 = 7;\n'
for k in range(R.randint(3, 6)):
    out += '    result = result * 3 + %s;\n' % expr(R.randint(4, 9))
out += '    CHECK(1);\n    return 0;\n}\n'
open(sys.argv[2], 'w').write(out)
