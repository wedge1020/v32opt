#!/usr/bin/env python3
"""mkbench.py clean|dirty OUT.asm -- fill in the template.
clean: the helpers really only write R0 and say so (';@fn ... clobbers=R0');
dirty: the helpers trash R1..R10 and say nothing."""
import sys
kind, out = sys.argv[1:3]
s = open(__file__.replace('mkbench.py', 'bench.asm.in')).read()
if kind == 'clean':
    s = s.replace('@@COERCE_HINT@@', ';@fn __arith_coerce clobbers=R0').replace('@@GET_HINT@@', ';@fn __builtin_table_get clobbers=R0')
    s = s.replace(';@@COERCE_BODY@@', '; (only R0 is written)').replace(';@@GET_BODY@@', '; (only R0 is written)')
else:
    s = s.replace('@@COERCE_HINT@@', '; (no clobbers= hint)').replace('@@GET_HINT@@', '; (no clobbers= hint)')
    junk = ''.join('    MOV R%d, 0x5A5A%04d\n' % (r, r) for r in range(1, 11))
    s = s.replace(';@@COERCE_BODY@@', '; trashes R1..R10\n' + junk).replace(';@@GET_BODY@@', '; trashes R1..R10\n' + junk)
open(out, 'w').write(s)
