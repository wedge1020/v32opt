#!/usr/bin/env python3
"""lineset.py FILE.asm -- print the set of (file:line) that has at least one instruction, one per
line, sorted; also exits 2 if an instruction before the first marker exists in a file that has
markers, and lists functions whose first instruction has no marker of its own."""
import re, sys
INS = set('MOV IADD ISUB IMUL IDIV JMP JT JF CALL RET PUSH POP IEQ INE IGT IGE ILT ILE FADD FSUB FMUL FDIV FEQ FNE FGT FGE FLT FLE CIF CFI AND OR XOR SHL NOT FLR LEA IMOD FMOD HLT WAIT OUT IN BNOT IABS FABS'.split())
cur = None; seen = False; lines = set(); bad = 0; fnfirst = False; fnbad = []
fn = None
for ln in open(sys.argv[1]):
    t = ln.strip()
    if t.startswith(';@src'):
        cur = t[5:].strip(); seen = True; marker_since_label = True
        if fnfirst: fnfirst = False
        continue
    m = re.match(r'(__function_\w+|__global_scope_initialization):', t)
    if m and not t.endswith('_return:'):
        fnfirst = True; fn = m.group(1)
        continue
    if t and not t.startswith(';') and t.split()[0].upper() in INS:
        if fnfirst:
            fnbad.append(fn); fnfirst = False
        if cur is None and seen is False:
            bad += 1
        elif cur is not None and cur != '-':
            lines.add(cur)
for l in sorted(lines): print(l)
if fnbad:
    sys.stderr.write('functions whose first instruction has no marker: %s\n' % ' '.join(fnbad)); sys.exit(3)
if bad and seen: sys.exit(2)
