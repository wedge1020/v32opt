#!/usr/bin/env python3
"""Rewrite the test programs into the stock Vircon32 C compiler's dialect.

usage: tostock.py in.c > out.c

The stock `compile` has no ternary operator or __LINE__, wants arrays as
`int[16] name` and pointers as `int* p`, calls struct types by their bare
name, and requires `void main`. The programs in this directory only use the
common subset, so a few regular expressions are enough.
"""
import re, sys


def to_stock(src):
    lines = src.split('\n')
    out = []
    in_main = False
    for i, l in enumerate(lines, 1):
        if '#include "check.h"' in l:
            out.append(l); continue
        l = re.sub(r'\bCHECK\((.*)\);', lambda m: 'if (%s) __check(1, %d); else __check(0, %d);' % (m.group(1), i, i), l)
        l = re.sub(r'\bstruct\s+ST\s*\*\s*', 'ST* ', l)
        l = re.sub(r'\bstruct\s+ST\s+(?=\w+\s*;)', 'ST ', l)
        l = re.sub(r'\bint\s*\*\s*(\w)', r'int* \1', l)
        l = re.sub(r'\b(int)\s+(\w+)((?:\[\d+\])+)\s*;', r'\1\3 \2;', l)
        if re.match(r'\s*int\s+main\s*\(\s*void\s*\)', l):
            l = l.replace('int main', 'void main'); in_main = True
        elif in_main:
            l = re.sub(r'\breturn\s+0\s*;', 'return;', l)
        out.append(l)
    return '\n'.join(out)
if __name__ == '__main__':
    print(to_stock(open(sys.argv[1]).read()))
