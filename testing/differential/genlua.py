#!/usr/bin/env python3
"""Random Lua programs for differential testing of v32lua output (README.md).

usage: genlua.py SEED OUTFILE.lua

Numeric Lua only (no printing, so the program runs without a BIOS): local
variables, numeric for / while / repeat loops with break, if/else, calls
that return one to three values and callers that use all of them (v32lua
returns the extra values in R2/R3), values passed through wrapper
functions untouched, nested calls inside expressions, and upvalues. The
results are folded into the globals r_0, r_1, ... which run_lua.py compares.

GENLUA_LIVE=1 adds (the default output for a seed is unchanged without it):
three helper functions h0..h2 that return two values, kept in the global table
ht and called through it (`ht[i](a, b)`), functions passed as arguments and
called (ap, ap2), the two results consumed after the indirect call, and loops
(numeric for / while / repeat) with an indirect call in the body, a value
carried from one iteration to the next, and a break in the middle.
"""
import os, random, sys
R = random.Random(int(sys.argv[1]))
NF = R.randint(3, 6)
TABLES = R.random() < 0.6        # tables and strings in the mix
LIVE = bool(os.environ.get('GENLUA_LIVE'))     # indirect calls, loop-carried values, calls in loops

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
        if LIVE and d < 3 and R.random() < 0.06:
            return '(%s)' % self.icall(d + 1, extra)         # parenthesised: exactly one value
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
        if LIVE and depth < 3 and R.random() < 0.12:
            return self.live_stmt(ind, depth)
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

    # ---- GENLUA_LIVE shapes -----------------------------------------------
    def icall(self, d, extra=()):
        """a call of an h function through the table, or with the function passed as an argument"""
        fn = 'ht[(%s) %% 3 + 1]' % self.expr(d + 1, extra)
        a, b = self.expr(d + 1, extra), self.expr(d + 1, extra)
        return R.choice(['%s(%s, %s)' % (fn, a, b), 'ap(%s, %s, %s)' % (fn, a, b), 'ap2(%s, %s, %s)' % (fn, a, b)])

    def live_stmt(self, ind, depth):
        sp = '  ' * ind
        self.uid += 1
        u = self.uid
        k = R.random()
        if k < 0.25:        # two results of an indirect call, both used after it
            a, b = R.sample(self.locs, 2) if len(self.locs) > 1 else (self.locs[0],) * 2
            return '%s%s, %s = %s\n%s%s = (%s * 3 + %s) %% 10007\n' % (
                sp, a, b, self.icall(1), sp, R.choice(self.locs), a, b)
        if k < 0.35:        # function value kept in a local, called twice around other work
            s = '%slocal fn%d = ht[(%s) %% 3 + 1]\n' % (sp, u, self.expr(1))
            s += '%slocal x%d, y%d = fn%d(%s, %s)\n' % (sp, u, u, u, self.expr(1), self.expr(1))
            s += self.block(ind, depth + 1, False)
            s += '%s%s = (x%d + y%d + (fn%d(%s, %s))) %% 10007\n' % (sp, R.choice(self.locs), u, u, u, self.expr(1), self.expr(1))
            return s
        # a loop with an indirect call, a carried value read at the top of the next iteration, a break in the middle
        cv, a, b = 'cv%d' % u, 'a%d' % u, 'b%d' % u
        s = '%slocal %s = %s\n' % (sp, cv, self.expr(1))
        kind = 'for' if k < 0.6 else ('while' if k < 0.8 else 'repeat')
        iv = 'j%d' % u
        if kind == 'for':
            s += '%sfor %s = 1, %s do\n' % (sp, iv, R.choice(['3', '4', '5']))
        elif kind == 'while':
            s += '%slocal %s = 0\n%swhile %s < %d do\n%s  %s = %s + 1\n' % (sp, iv, sp, iv, R.randint(2, 5), sp, iv, iv)
        else:
            s += '%slocal %s = 0\n%srepeat\n%s  %s = %s + 1\n' % (sp, iv, sp, sp, iv, iv)
        s += '%s  local %s, %s = ht[(%s + %s) %% 3 + 1](%s, %s)\n' % (sp, a, b, iv, self.expr(2, (iv,)), cv, self.expr(2, (iv,)))
        s += '%s  r_0 = (r_0 + %s) %% 10007\n' % (sp, a)
        s += '%s  if %s then break end\n' % (sp, self.cond((iv, a, b)))
        s += self.block(ind + 1, depth + 1, True, (iv,))
        s += '%s  %s = (%s + %s * 2) %% 10007\n' % (sp, cv, a, b)            # carried to the next iteration
        if kind == 'repeat':
            return s + '%suntil %s >= %d\n' % (sp, iv, R.randint(2, 4))
        return s + '%send\n' % sp

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
if LIVE:
    body += ('function h0(a, b)\n  return (a + b) %% 10007, (a * 3 + %d) %% 10007\nend\n\n'
             'function h1(a, b)\n  local t = (a - b) %% 97\n  if t > %d then return t, a end\n  return b, t\nend\n\n'
             'function h2(a, b)\n  local x, y = h0(b, a)\n  return y, (x + %d) %% 10007\nend\n\n'
             'function ap(fn, a, b)\n  return fn(a, b)\nend\n\n'
             'function ap2(fn, a, b)\n  local x, y = fn(a, b)\n  return y, x\nend\n\n') % (R.randint(0, 9), R.randint(10, 60), R.randint(0, 9))
for i in range(NF):
    fs.append(F(i))
    body += fs[-1].emit()

out = '-- generated by genlua.py\nr_0 = 0\nr_1 = 0\nr_2 = 0\nr_3 = 0\n\n' + body
out += 'function main()\n'
if LIVE: out += '  ht = {h0, h1, h2}\n'
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
