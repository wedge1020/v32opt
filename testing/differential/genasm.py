#!/usr/bin/env python3
"""Random hand-written-style Vircon32 assembly for differential testing of register liveness (README.md).

usage: genasm.py SEED OUT.asm        (run it with `run.py` like the files in asm/; STRICT=1 applies)

C cannot express what this does: small functions that return results in an arbitrary
subset of R0..R7 (not just R0), that are called directly and through `mov rN, _label` /
`call rN`, that save registers with PUSH/POP (some of them registers they also read as
arguments), that tail-jump to or fall through into the next function, that call each
other in the middle, and one kind that reads a stack argument ([sp+1]). The main program
calls them in a random order and stores the registers it reads into consecutive RAM
words, some of them only after further calls, so a write that the optimizer wrongly
treats as dead changes the RAM image.

Determinism: the generator tracks which registers hold a defined value at every point
(`known`). Operations read only known registers; the main program stores only known
registers; a register that was used for an indirect call and holds a code address is
dropped from `known` (and a function reports such registers as `junk`, never as results, so
no code address reaches the RAM image: it would change with the code size). Functions touch only R0..R7; every register a function writes and
does not restore is part of its result set (its `outset`), including those of functions
it calls or jumps to. The result set of a callee defines what its callers may read.
Programs have no loops (calls and tail jumps go to higher-numbered functions only), so
they always terminate.
"""
import random, sys

R = random.Random(int(sys.argv[1]))
ALL = list(range(8))
NF = R.randint(3, 7)

def rn(x): return 'r%d' % x
def imm(): return R.choice([0, 1, 2, 3, 5, 7, 10, 13, 42, 100, -1, -7])

info = [None] * NF          # per function: dict(args, outset, stk, lines)

def setargs(args, known, touched, saved=()):
    """mov imm / copy into each argument register; `touched` collects the registers written"""
    out = []
    order = list(args); R.shuffle(order)
    for a in order:
        srcs = [x for x in known if x != a]
        if srcs and R.random() < 0.4: out.append('    mov %s, %s' % (rn(a), rn(R.choice(srcs))))
        else: out.append('    mov %s, %d' % (rn(a), imm()))
        known.add(a); touched.add(a)
    return out

def gen_fn(k):
    lab = '_f%d' % k
    stk = R.random() < 0.15 and k > 0
    L = [lab + ':']
    if stk:
        x = R.choice(ALL); outs = {x}
        L.append('    mov %s, [sp+1]' % rn(x))
        if R.random() < 0.7: L.append('    iadd %s, %d' % (rn(x), imm()))
        if R.random() < 0.6:
            y = R.choice([r for r in ALL if r != x]); outs.add(y)
            L.append('    mov %s, %s' % (rn(y), rn(x)))
            L.append('    imul %s, %d' % (rn(y), R.choice([2, 3, 5])))
        L.append('    ret')
        info[k] = dict(args=[], outset=outs, junk=set(), stk=True, lines=L)
        return
    args = sorted(R.sample(ALL, R.choice([0, 1, 1, 2])))
    S = R.sample(ALL, R.choice([0, 0, 1, 2]))               # registers saved at entry, restored before the exit
    OUT = [r for r in R.sample(ALL, R.randint(1, 4)) if r not in S] or [r for r in ALL if r not in S][:1]
    pool = OUT + S
    known = set(args)
    W = set()                                               # registers written and not restored (the result set)
    junk = set()                                            # registers left holding a code address
    for r in S: L.append('    push %s' % rn(r))
    def wr(x):
        known.add(x); junk.discard(x)
        if x not in S: W.add(x)
    nlab = [0]
    def newlab():
        nlab[0] += 1
        return '%s_%d' % (lab, nlab[0])
    for _ in range(R.randint(2, 6)):
        c = R.random()
        kn = sorted(known)
        if c < 0.25:
            x = R.choice(pool); L.append('    mov %s, %d' % (rn(x), imm())); wr(x)
        elif c < 0.42 and kn:
            x = R.choice(pool); L.append('    mov %s, %s' % (rn(x), rn(R.choice(kn)))); wr(x)
        elif c < 0.60:
            ks = [x for x in pool if x in known]
            if ks:
                x = R.choice(ks)
                op = R.choice(['iadd', 'isub', 'imul'])
                src = rn(R.choice(kn)) if R.random() < 0.5 else str(R.choice([1, 2, 3, 5]))
                L.append('    %s %s, %s' % (op, rn(x), src)); wr(x)
        elif c < 0.68:                                      # a dead store: overwritten at once
            x = R.choice(pool)
            L.append('    mov %s, %d' % (rn(x), imm())); wr(x)
            L.append('    mov %s, %d' % (rn(x), imm()))
        elif c < 0.78 and kn:                               # diamond: both arms define the same registers
            cr = R.choice(pool)
            D = sorted(set([cr] + R.sample(pool, R.randint(0, min(2, len(pool))))))
            lt, le = newlab(), newlab()
            L.append('    mov %s, %s' % (rn(cr), rn(R.choice(kn))))
            L.append('    %s %s, %d' % (R.choice(['ilt', 'igt', 'ine']), rn(cr), R.choice([0, 3, 7])))
            L.append('    %s %s, %s' % (R.choice(['jt', 'jf']), rn(cr), lt))
            for x in D: L.append('    mov %s, %d' % (rn(x), imm()))
            L.append('    jmp %s' % le)
            L.append(lt + ':')
            for x in D: L.append('    mov %s, %d' % (rn(x), imm()))
            L.append(le + ':')
            for x in D: wr(x)
        elif c < 0.92 and k < NF - 1:                       # call a later function
            J = R.randrange(k + 1, NF)
            I = info[J]
            if I['stk']:
                ks = sorted(known)
                if not ks: continue
                X = R.choice(ks)
                L.append('    push %s' % rn(X))
                L.append('    call _f%d' % J)
                if R.random() < 0.5:
                    L.append('    pop %s' % rn(X)); skip = {X}
                else:
                    L.append('    iadd sp, 1'); skip = set()
                for x in I['outset'] - skip:
                    wr(x)
                continue
            touched = set()
            L += setargs(I['args'], known, touched)
            for a in touched: wr(a)
            skip = set()
            ks = [x for x in known if x in I['outset']]
            if ks and R.random() < 0.4:                     # save a register around the call
                X = R.choice(ks); skip = {X}
                L.append('    push %s' % rn(X))
            if R.random() < 0.3:
                m = R.choice([x for x in ALL if x not in I['args']])
                L.append('    mov %s, _f%d' % (rn(m), J)); L.append('    call %s' % rn(m))
                known.discard(m)
                if m not in S and m not in skip: junk.add(m)
            else:
                L.append('    call _f%d' % J)
            if skip: L.append('    pop %s' % rn(R.choice(list(skip))))
            for x in I['junk'] - skip:
                known.discard(x); junk.add(x)
            for x in I['outset'] - skip: wr(x)
    for x in OUT:                                           # every result register defined on all paths
        if x not in known:
            L.append('    mov %s, %d' % (rn(x), imm())); wr(x)
    for r in reversed(S):
        L.append('    pop %s' % rn(r))
        junk.discard(r)
        if r not in args: known.discard(r)                  # back to the caller's value, which this function cannot vouch for
    tail = None
    if k < NF - 1 and R.random() < 0.35:
        tail = R.randrange(k + 1, NF)
        if info[tail]['stk']: tail = None
    if tail is None:
        L.append('    ret')
        outset = W - junk
    else:
        I = info[tail]
        touched = set()
        L += setargs(I['args'], known, touched)
        W |= touched
        for x in touched: junk.discard(x)
        junk = (junk | I['junk']) - I['outset']
        outset = (W | I['outset']) - junk
        if tail == k + 1 and R.random() < 0.4: L.append('    ; falls through into _f%d' % tail)
        else: L.append('    jmp _f%d' % tail)
    info[k] = dict(args=args, outset=outset, junk=junk, stk=False, lines=L, tail=tail)

for k in range(NF - 1, -1, -1): gen_fn(k)

# ---- main
M = ['_main:']
known = set()
slot = [0]
def store(regs):
    for x in regs:
        M.append('    mov [%d], %s' % (slot[0], rn(x))); slot[0] += 1
for _ in range(R.randint(6, 12)):
    J = R.randrange(NF)
    I = info[J]
    touched = set()
    if I['stk']:
        ks = sorted(known)
        X = R.choice(ks) if ks else None
        if X is None:
            X = 0; M.append('    mov r0, %d' % imm()); known.add(0)
        M.append('    push %s' % rn(X))
        if R.random() < 0.4:
            m = R.choice([8, 9]); M.append('    mov %s, _f%d' % (rn(m), J)); M.append('    call %s' % rn(m))
        else:
            M.append('    call _f%d' % J)
        if R.random() < 0.5: M.append('    pop %s' % rn(X)); gone = set()
        else: M.append('    iadd sp, 1'); gone = I['outset']
        known |= I['outset']
    else:
        M += setargs(I['args'], known, touched)
        r = R.random()
        if r < 0.45:
            M.append('    call _f%d' % J)
        else:
            m = R.choice([x for x in ALL + [8, 9] if x not in I['args']])
            M.append('    mov %s, _f%d' % (rn(m), J)); M.append('    call %s' % rn(m))
            known.discard(m)
        known -= I['junk']
        known |= I['outset']
    # read some results now, others (older ones) later
    now = sorted(I['outset']) if R.random() < 0.7 else R.sample(sorted(I['outset']), R.randint(1, len(I['outset'])))
    store([x for x in now if x in known])
    if known and R.random() < 0.3:
        store(R.sample(sorted(known), R.randint(1, min(3, len(known)))))
store(sorted(known))
M.append('    ret')

out = ['; generated by genasm.py', '    call _main', '    hlt', '']
for k in range(NF): out += info[k]['lines'] + ['']
out += M
open(sys.argv[2], 'w').write('\n'.join(out) + '\n')
