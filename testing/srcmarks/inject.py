#!/usr/bin/env python3
"""inject.py IN.asm OUT.asm -- add ';@src FILE:LINE' markers to a compiler-style .asm the way a
compiler would: one marker per run of instructions, a new 'source line' every few instructions,
each function (__function_*) in its own block of 100 lines, and ';@src -' for the runtime
(everything before the first function)."""
import re, sys
src, dst = sys.argv[1:3]
out = []
line = 0
fn = 0
in_fn = False
cnt = 0
last = None
def emit(m):
    global last
    if m != last:
        out.append(m)
        last = m
for ln in open(src):
    t = ln.strip()
    m = re.match(r'(__function_\w+|__global_scope_initialization):', t)
    if m and not t.startswith('__function_') or (m and not re.search(r'_return:$', t)):
        fn += 1; line = fn * 100; in_fn = True; cnt = 0
    is_ins = bool(t) and not t.startswith(';') and not t.startswith('%') and not t.startswith('.') \
        and not t.endswith(':') and re.match(r'[A-Za-z]+\b', t) and t.split()[0].upper() in (
        'MOV','IADD','ISUB','IMUL','IDIV','JMP','JT','JF','CALL','RET','PUSH','POP','IEQ','INE','IGT','IGE','ILT','ILE',
        'FADD','FSUB','FMUL','FDIV','FEQ','FNE','FGT','FGE','FLT','FLE','CIF','CFI','AND','OR','XOR','SHL','NOT','FLR','LEA',
        'IMOD','FMOD','HLT','WAIT','OUT','IN','BNOT','IABS','FABS')
    if is_ins:
        if in_fn:
            if cnt % 3 == 0: line += 1
            cnt += 1
            emit(';@src prog.c:%d' % line)
        else:
            emit(';@src -')
    out.append(ln.rstrip('\n'))
open(dst, 'w').write('\n'.join(out) + '\n')
