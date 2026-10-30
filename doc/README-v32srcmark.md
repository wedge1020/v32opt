# v32srcmark for v32opt

Drop-in files (nothing here changes existing v32opt code):

    scripts/v32srcmark.py               the tool (Python 3, no dependencies)
    testing/srcmark/check_srcmark.py    test on canned stock-compiler output (no stock compiler needed)
    testing/srcmark/*.c, *.asm.in, *.asm.debug

Pipeline:  compile prog.c -g  ->  v32srcmark.py prog.asm  ->  v32opt  ->  v32as -g

Optional (proposal): `v32opt --stock-c FILE.asm` runs the pass itself when the input has
`__function_X:` labels and no `;@hints` header and a FILE.asm.debug beside it.
Design and limits: v32lua proposals/V32DBG-SPEC.md, "Tier B".
