#!/usr/bin/env python3
"""Random Lua programs for differential testing of v32lua output (README.md).

usage: genlua.py SEED OUTFILE.lua

Numeric Lua only (no printing, so the program runs without a BIOS): local
variables, numeric for / while / repeat loops with break, if/else, calls
that return one to three values and callers that use all of them (v32lua
returns the extra values in R2/R3), values passed through wrapper
functions untouched, nested calls inside expressions, and upvalues. The
results are folded into the globals r_0, r_1, ... which run_lua.py compares.
"""
import random, sys
R = random.Random(int(sys.argv[1]))
NF = R.randint(3, 6)
TABLES = R.random() < 0.6        # tables and strings in the mix

class F:
    def __init__(self, idx):
        self.idx = idx
        self.nret = R.randint(1, 3)
        self.npar = R.randint(1, 3)
        self.params = ['p%d' % i for i in range(self.npar)]
        self.locs = ['l%d' % i for i in range(R.randint(2, 5))]
        self.uid = 0
        self.wrapper = idx > 0 and R.random() < 0.2      # returns f(...) of an earlier function untouched
        self.wrap_of = R.randrange(idx) if self.wrapper else None
        if self.wrapper:
            self.nret = fs[self.wrap_of].nret
            self.npar = fs[self.wrap_of].npar
            self.params = ['p%d' % i for i in range(self.npar)]

    def var(self, extra=()):
        return R.choice(self.params + self.locs + list(extra))

    def num(self):
        return str(R.choice([0, 1, 2, 3, 5, 7, 10, 13]))

    def expr(self, d, extra=()):
        r = R.random()
        if d > 2 or r < 0.3:
            return self.var(extra) if R.random() < 0.7 else self.num()
        if r < 0.82:
            return '(%s %s %s)' % (self.expr(d + 1, extra), R.choice(['+', '-', '*', '+', '-']), self.expr(d + 1, extra))
        if r < 0.9:
            return '(%s %% 97)' % self.expr(d + 1, extra)
        if self.idx > 0:
            c = fs[R.randrange(self.idx)]
            return '(%s)' % self.call(c, d + 1, extra)        # parenthesised: exactly one value
        return self.var(extra)

    def call(self, c, d, extra=()):
        return 'f%d(%s)' % (c.idx, ', '.join(self.expr(d + 1, extra) for _ in range(c.npar)))

    def cond(self, extra=()):
        c = '%s %s %s' % (self.expr(1, extra), R.choice(['<', '>', '<=', '>=', '==', '~=']), self.expr(1, extra))
        r = R.random()
        if r < 0.15: c = '(%s) and (%s < %s)' % (c, self.var(extra), self.num())
        elif r < 0.25: c = '(%s) or (%s > %s)' % (c, self.var(extra), self.num())
        return c

    def stmt(self, ind, depth, loop):
        sp = '  ' * ind
        r = R.random()
        if depth >= 3: r *= 0.5
        if r < 0.3:
            return '%s%s = (%s) %% 10007\n' % (sp, R.choice(self.locs), self.expr(0))
        if r < 0.38:
            return '%sr_%d = (r_%d + %s) %% 10007\n' % (sp, 0, 0, self.expr(1))
        if r < 0.46 and self.idx > 0:
            c = fs[R.randrange(self.idx)]
            names = [R.choice(self.locs) for _ in range(c.nret)]
            return '%s%s = %s\n' % (sp, ', '.join(names), self.call(c, 1))
        if r < 0.5 and loop:
            return '%sif %s then %s end\n' % (sp, self.cond(), R.choice(['break']))
        if r < 0.52 and TABLES:
            k = R.choice(['t[%s %% 5 + 1] = %s' % (self.expr(2), self.expr(1)),
                          '%s = (%s + (t[%s %% 5 + 1] or 0)) %% 10007' % (R.choice(self.locs), R.choice(self.locs), self.expr(2)),
                          '%s = (#t + %s) %% 10007' % (R.choice(self.locs), self.expr(1)),
                          'tstr = "k" .. (%s %% 7)' % self.expr(1),
                          '%s = (#tstr * 3 + %s) %% 10007' % (R.choice(self.locs), self.expr(1)),
                          'if tstr == "k3" then %s = %s + 1 end' % ((R.choice(self.locs),) * 2),
                          'u = {%s, %s, [%d] = %s}' % (self.expr(2), self.expr(2), R.randint(3, 6), self.expr(2)),
                          '%s = (u[1] + u[2] + (u[%d] or 0)) %% 10007' % (R.choice(self.locs), R.randint(3, 6))])
            return sp + k + '\n'
        if r < 0.55 and depth > 0:
            return '%sif %s then return %s end\n' % (sp, self.cond(), ', '.join(self.expr(1) for _ in range(self.nret)))
        if r < 0.68:
            s = '%sif %s then\n' % (sp, self.cond()) + self.block(ind + 1, depth + 1, loop)
            if R.random() < 0.5: s += '%selse\n' % sp + self.block(ind + 1, depth + 1, loop)
            return s + '%send\n' % sp
        if r < 0.82:
            self.uid += 1
            v = 'i%d_%d' % (self.idx, self.uid)
            hi = R.choice(['3', '4', '5', 'p0 % 5 + 1'])
            s = '%sfor %s = 1, %s do\n' % (sp, v, hi)
            s += self.block(ind + 1, depth + 1, True, (v,))
            return s + '%send\n' % sp
        if r < 0.9:
            self.uid += 1
            w = 'w%d_%d' % (self.idx, self.uid)
            s = '%slocal %s = 0\n%swhile %s < %d and %s do\n%s  %s = %s + 1\n' % (sp, w, sp, w, R.randint(2, 5), self.cond(), sp, w, w)
            s += self.block(ind + 1, depth + 1, True, (w,))
            return s + '%send\n' % sp
        self.uid += 1
        w = 'u%d_%d' % (self.idx, self.uid)
        s = '%slocal %s = 0\n%srepeat\n%s  %s = %s + 1\n' % (sp, w, sp, sp, w, w)
        s += self.block(ind + 1, depth + 1, True, (w,))
        return s + '%suntil %s >= %d\n' % (sp, w, R.randint(2, 4))

    def block(self, ind, depth, loop, extra=()):
        # statements may read the loop variables in `extra` but only the stmt
        # generators above write locals/globals
        saved = self.locs
        out = ''.join(self.stmt(ind, depth, loop) for _ in range(R.randint(1, 3)))
        return out

    def emit(self):
        if self.wrapper:
            return 'function f%d(%s)\n  return f%d(%s)\nend\n\n' % (
                self.idx, ', '.join(self.params), self.wrap_of, ', '.join(self.params))
        s = 'function f%d(%s)\n' % (self.idx, ', '.join(self.params))
        if TABLES: s += '  local t = {}\n  local u = {0, 0}\n  local tstr = "k0"\n'
        for l in self.locs:
            s += '  local %s = %s\n' % (l, R.choice(self.params + [self.num()]))
        s += ''.join(self.stmt(1, 0, False) for _ in range(R.randint(3, 6)))
        s += '  return %s\nend\n\n' % ', '.join(self.expr(1) for _ in range(self.nret))
        return s

fs = []
body = ''
for i in range(NF):
    fs.append(F(i))
    body += fs[-1].emit()

out = '-- generated by genlua.py\nr_0 = 0\nr_1 = 0\nr_2 = 0\nr_3 = 0\n\n' + body
out += 'function main()\n'
for k in range(R.randint(4, 8)):
    c = R.choice(fs)
    args = ', '.join(str(R.choice([0, 1, 2, 5, 9, 17])) for _ in range(c.npar))
    names = ['a%d' % j for j in range(c.nret)]
    out += '  local %s = f%d(%s)\n' % (', '.join(names), c.idx, args)
    out += '  r_1 = (r_1 * 31 + %s) %% 10007\n' % ' + '.join(names)
    out += '  r_2 = (r_2 + %s * 3) %% 10007\n' % names[-1]
    if c.nret > 1:
        out += '  r_3 = (r_3 * 7 + %s - %s) %% 10007\n' % (names[0], names[-1])
out += 'end\n'
open(sys.argv[2], 'w').write(out)
