#!/usr/bin/env python3
"""hinter.py -- add v32opt compiler hints (';@' lines) to v32cc output, as v32cc itself would.

A stand-in for the compiler side of doc/hints.md, used by the differential tests (HINTS=1) to
exercise v32opt's hint handling before / without a v32cc that writes them. It can only see the
assembly, so it is deliberately conservative:

  * abi=caller-save                      (v32cc's convention)
  * escapes=: every "LEA R, [BP-N]" is an address taken; the object it points to is assumed to extend
    from that word up to the top of the locals, so the list is  <lowest LEA'd offset>..-1.
    Words below the lowest LEA'd one are other objects and are reported as not escaping.
  * noinline / volatile: not derivable from assembly, so never emitted here.

usage: hinter.py in.asm [out.asm]      (in place if no out)
"""
import re, sys

def hint(text):
    lines = text.split('\n')
    out, fn_lines = [], []
    cur = None                                   # (label, index in out where its hint line goes)
    lea = {}
    for ln in lines:
        m = re.match(r'^(__function_[A-Za-z0-9_]*):\s*$', ln)
        if m and not m.group(1).endswith('_return'):
            cur = m.group(1); lea[cur] = []; fn_lines.append(cur)
        elif re.match(r'^[A-Za-z_]\w*:\s*$', ln) and not ln.startswith('.') and not ln.startswith('__function_'):
            pass
        m = re.match(r'^\s*lea\s+\w+\s*,\s*\[\s*BP\s*([+-])\s*(\d+)\s*\]', ln, re.I)
        if m and cur:
            off = int(m.group(2)) * (1 if m.group(1) == '+' else -1)
            lea[cur].append(off)
        out.append(ln)
    head = [';@hints 1 abi=caller-save']
    for fn in fn_lines:
        offs = [o for o in lea[fn] if o < 0]
        pos = [o for o in lea[fn] if o >= 0]
        if pos:
            esc = 'all'                          # the address of a parameter: not modelled here
        elif offs:
            esc = '%d..-1' % min(offs)
        else:
            esc = 'none'
        head.append(';@fn %s escapes=%s' % (fn, esc))
    return '\n'.join(head) + '\n' + '\n'.join(out)

def hint_file(path, out=None):
    text = hint(open(path).read())
    open(out or path, 'w').write(text)

if __name__ == '__main__':
    src = sys.argv[1]
    text = hint(open(src).read())
    open(sys.argv[2] if len(sys.argv) > 2 else src, 'w').write(text)
