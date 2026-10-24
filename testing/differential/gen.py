#!/usr/bin/env python3
"""Random loop/branch/call heavy C programs for differential testing of the
promote passes (see README.md).

usage: gen.py SEED OUTFILE.c        (GEN_NF=<n> sets the number of functions)

Each program has a handful of functions with locals, parameters, for/while/
do loops, break/continue, early returns, switch, calls to earlier
functions, address-taken locals (a persistent pointer, a struct reached
directly and through a pointer, a local array, by-pointer calls) and ends by
folding every global into `result`, so a miscompile shows in the final RAM.
Loop bounds are small and loops are always bounded.
"""
import os, random, sys

seed = int(sys.argv[1])
R = random.Random(seed)
CALLY = bool(os.environ.get('GEN_CALLY'))      # many calls, deeply nested in expressions
NF = int(os.environ["GEN_NF"]) if "GEN_NF" in os.environ else R.randint(2, 5)

def const():
    return str(R.choice([0, 1, 2, 3, 5, 7, 10, 13, -1, -4, 100]))

class Fn:
    def __init__(self, idx, nparams):
        self.idx = idx
        self.nparams = nparams
        self.params = ['p%d' % i for i in range(nparams)]
        self.locals = ['l%d' % i for i in range(R.randint(2, 6))]
        self.loopvars = []
        self.accs = ['a%d' % i for i in range(R.choice([0, 1, 1, 2]))]   # read only through the global they feed
        self.depth = 0
        self.uid = 0
        self.arrloc = R.random() < 0.08          # local array -> address taken
        self.ptrloc = R.random() < 0.25          # persistent &local -> address taken
        self.stloc = R.random() < 0.20           # struct local reached directly and via pointer

    def var(self, writable=False):
        pool = list(self.locals) + (list(self.params) if self.params and (writable or True) else [])
        if not writable:
            pool += self.loopvars
        return R.choice(pool)

    def wvar(self):
        pool = list(self.locals) + list(self.params)
        return R.choice(pool)

    def expr(self, d=0):
        r = R.random()
        if CALLY and self.idx > 0 and d < 5 and self.depth == 0 and R.random() < 0.45:
            callee = R.randrange(self.idx)
            return 'f%d(%s)' % (callee, ', '.join(self.expr(d + 1) for _ in range(NPARAMS[callee])))
        if d > 2 or r < 0.30:
            c = R.random()
            if c < 0.5: return self.var()
            if c < 0.7: return const()
            if c < 0.85: return R.choice(['g0', 'g1', 'g2'])
            return 'garr[(%s) & 15]' % self.var()
        if r < 0.85:
            op = R.choice(['+', '-', '*', '^', '&', '|', '+', '+', '-'])
            return '(%s %s %s)' % (self.expr(d + 1), op, self.expr(d + 1))
        if r < 0.92:
            return '(%s %% 7)' % ('(%s & 255)' % self.expr(d + 1))
        if self.idx > 0:
            callee = R.randrange(self.idx)
            n = NPARAMS[callee]
            return 'f%d(%s)' % (callee, ', '.join(self.expr(d + 1) for _ in range(n)))
        return self.var()

    def cond(self):
        op = R.choice(['<', '>', '<=', '>=', '==', '!='])
        c = '%s %s %s' % (self.expr(1), op, self.expr(1))
        r = R.random()
        if r < 0.15: c = '(%s) && (%s %s %s)' % (c, self.var(), R.choice(['<', '!=']), const())
        elif r < 0.25: c = '(%s) || (%s > %s)' % (c, self.var(), const())
        return c

    def stmt(self, ind, loopctx):
        sp = '    ' * ind
        r = R.random()
        if self.depth >= 3: r = r * 0.55
        if r < 0.28:
            return '%s%s = %s;\n' % (sp, self.wvar(), self.expr())
        if r < 0.36:
            return '%sg%d = %s;\n' % (sp, R.randrange(3), self.expr())
        if r < 0.42:
            return '%sgarr[(%s) & 15] = %s;\n' % (sp, self.expr(1), self.expr())
        if r < 0.50:
            v = self.wvar()
            return '%s%s %s= %s;\n' % (sp, v, R.choice(['+', '-', '^', '|']), self.expr(1))
        if r < 0.54:
            return '%s%s++;\n' % (sp, self.wvar())
        if r < 0.58 and self.accs:
            a = R.choice(self.accs)
            return '%s%s += %s; g%d = %s;\n' % (sp, a, self.expr(2), R.randrange(3), a)
        if r < 0.565 and self.ptrloc:
            return '%s*pp %s= %s;\n' % (sp, R.choice(['', '+', '^']), self.expr(1))
        if r < 0.575 and self.stloc:
            f = R.choice(['a', 'b', 'c'])
            if R.random() < 0.5:
                return '%sst.%s %s= %s;\n' % (sp, f, R.choice(['', '+', '^']), self.expr(1))
            return '%sps->%s %s= %s;\n' % (sp, f, R.choice(['', '+']), self.expr(1))
        if r < 0.585:
            return '%sbump(&%s, %s);\n' % (sp, self.wvar(), const())
        if r < 0.60 and self.depth < 3:
            self.depth += 1
            s = '%sswitch ((%s) & 3) {\n' % (sp, self.expr(1))
            for cv in range(R.randint(1, 3)):
                s += '%scase %d:\n%s' % (sp, cv, self.block(ind + 1, loopctx))
                if R.random() < 0.7: s += '%s    break;\n' % sp
            s += '%sdefault:\n%s%s}\n' % (sp, self.block(ind + 1, loopctx), sp)
            self.depth -= 1
            return s
        if r < 0.58 and loopctx:
            return '%sif (%s) %s;\n' % (sp, self.cond(), R.choice(loopctx))
        if r < 0.61 and self.depth > 0:
            return '%sif (%s) return %s;\n' % (sp, self.cond(), self.expr(1))
        if r < 0.72:
            self.depth += 1
            s = '%sif (%s) {\n' % (sp, self.cond())
            s += self.block(ind + 1, loopctx)
            if R.random() < 0.5:
                s += '%s} else {\n' % sp + self.block(ind + 1, loopctx)
            s += '%s}\n' % sp
            self.depth -= 1
            return s
        if r < 0.85:
            self.depth += 1
            self.uid += 1
            lv = 'i%d_%d' % (self.idx, self.uid)
            self.locals_extra.append(lv)
            n = R.choice(['3', '4', '6', '(%s & 7)' % self.var(), '10'])
            s = '%sfor (%s = 0; %s < %s; %s++) {\n' % (sp, lv, lv, n, lv)
            self.loopvars.append(lv)
            s += self.block(ind + 1, ['break', 'continue'])
            self.loopvars.remove(lv)
            s += '%s}\n' % sp
            self.depth -= 1
            return s
        if r < 0.92:
            self.depth += 1
            self.uid += 1
            wv = 'w%d_%d' % (self.idx, self.uid)
            self.locals_extra.append(wv)
            s = '%s%s = 0;\n%swhile (%s < %d && (%s)) {\n' % (sp, wv, sp, wv, R.randint(2, 6), self.cond())
            s += '%s    %s++;\n' % (sp, wv)
            self.loopvars.append(wv)
            s += self.block(ind + 1, ['break', 'continue'])
            self.loopvars.remove(wv)
            s += '%s}\n' % sp
            self.depth -= 1
            return s
        self.depth += 1
        self.uid += 1
        dv = 'd%d_%d' % (self.idx, self.uid)
        self.locals_extra.append(dv)
        s = '%s%s = 0;\n%sdo {\n%s    %s++;\n' % (sp, dv, sp, sp, dv)
        self.loopvars.append(dv)
        s += self.block(ind + 1, ['break', 'continue'])
        self.loopvars.remove(dv)
        s += '%s} while (%s < %d);\n' % (sp, dv, R.randint(2, 5))
        self.depth -= 1
        return s

    def block(self, ind, loopctx):
        return ''.join(self.stmt(ind, loopctx) for _ in range(R.randint(1, 4)))

    def emit(self):
        self.locals_extra = []
        body = ''.join(self.stmt(1, []) for _ in range(R.randint(3, 7)))
        if self.stloc: body += '    result = result + st.a + st.b * 3 + ps->c * 5;\n'
        body += '    return %s;\n' % self.expr()
        decl = ''
        names = self.locals + self.locals_extra
        decl += '    int %s;\n' % ', '.join(names + self.accs)
        if self.arrloc: decl += '    int la[4];\n'
        if self.ptrloc: decl += '    int *pp;\n'
        if self.stloc: decl += '    struct ST st;\n    struct ST *ps;\n'
        init = ''.join('    %s = %s;\n' % (l, R.choice(self.params + [const()]) if self.params else const()) for l in self.locals)
        init += ''.join('    %s = 0;\n' % a for a in self.accs)
        if self.arrloc: init += '    la[0] = 1; la[1] = 2; la[2] = 3; la[3] = 4;\n'
        if self.ptrloc: init += '    pp = &%s;\n' % self.locals[0]
        if self.stloc: init += '    ps = &st; st.a = 1; st.b = 2; st.c = 3;\n'
        sig = 'int f%d(%s)' % (self.idx, ', '.join('int %s' % p for p in self.params) if self.params else 'void')
        return '%s {\n%s%s%s}\n\n' % (sig, decl, init, body)

NPARAMS = [R.randint(0, 3) for _ in range(NF)]
fns = []
out = '#include "check.h"\nint result;\nint g0, g1, g2;\nint garr[16];\nstruct ST { int a; int b; int c; };\nvoid bump(int *p, int k) { *p += k + 1; }\n\n'
for i in range(NF):
    fn = Fn(i, NPARAMS[i])
    fns.append(fn)
    out += fn.emit()

out += 'int main(void) {\n    int k;\n    g0 = 3; g1 = -2; g2 = 9;\n    for (k = 0; k < 16; k++) garr[k] = k * 5 - 7;\n'
for i in range(NF):
    for t in range(3):
        args = ', '.join(str(R.choice([0, 1, 2, 5, 9, -3, 17])) for _ in range(NPARAMS[i]))
        out += '    result = result * 31 + f%d(%s);\n' % (i, args)
out += '    for (k = 0; k < 16; k++) result = result * 17 + garr[k];\n'
out += '    result = result * 5 + g0 + g1 * 3 + g2 * 7;\n'
out += '    CHECK(1);\n    return 0;\n}\n'
open(sys.argv[2], 'w').write(out)
