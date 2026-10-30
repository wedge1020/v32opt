# Source markers (`;@src`)

`v32lua -g` (and later `v32cc -g`) write `;@src FILE:LINE` lines (optionally `FILE:LINE:COL`, or `;@src -`
for "no source") before the instructions they describe. They are comments: the assembler ignores
them and a missing or wrong marker is always safe. `v32opt` keeps them correct through its passes,
so a debugger can step by source line at any optimization level.

## How v32opt carries them

On input each marker is attached to the instructions after it (`loc_file/loc_line/loc_col` in the
node); the marker lines themselves are removed from the list, so no pass has to skip them. On output
a marker is written wherever the location changes, at the first instruction of every function and after a
`;@src -`.

| Transformation | Result |
|---|---|
| rewrite in place | the instruction keeps its location |
| instruction deleted | its location is dropped with it; the next instruction keeps its own (a marker is written there if it differs from the previous one), so no line is invented |
| merged instructions (CSE, forwarding) | the surviving instruction keeps its own location |
| copies (unrolling, inlining, loop-invert) | each copy has the location of its original (for inlining: the callee's file and line) |
| moved block | each instruction keeps its location, and a marker is written at the move |
| instruction made by a pass | inherits the location of the instruction it replaces / sits next to |

## Options

* `-g` keep the markers (default when the input has any); `-g0` or `--strip-src` remove them.
* `--check-src` after optimization, verify that every instruction of a file with markers is covered by one
  (only `;@src -` regions may be unmarked) and that each function starts with a marker; exit status 3 and
  a message on stderr on a problem.

## Testing

`testing/srcmarks/run.sh` injects markers into every test program, optimizes at each level, runs
`--check-src`, and checks that the set of source lines present at `-O1..-O3` is a subset of the lines at
`-O0` (an optimized file may lose lines, never gain one). Without markers the output is byte-identical to
what the previous version produced.
