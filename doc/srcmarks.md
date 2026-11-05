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

## Variables: `;@var`, `;@scope`, `;@home` (V32DBG)

A compiler with `-g` writes `;@var NAME loc=... type=...` where a variable becomes live and
`;@scope { ... }` around its block. Register promotion moves variables out of their frame slots, so
`v32opt` keeps those records true:

* A slot promoted to a register over a range of code: the `;@var` lines in that range are rewritten
  (`loc=BP-1` becomes `loc=R4`). The range ends where the register is stored back or a call ends the
  segment; there the original `loc=` is written again.
* The move itself is recorded with `;@home FROM TO` (`;@home BP-1 R4` at the load, `;@home R4 BP-1`
  at the store back or before the call). `v32as` turns it into the matching `.vbin.vars` ranges.
  A promotion that drops the slot for good is written `;@home R4 optimized-out`.
* Inlining does not copy the callee's `;@var` / `;@scope` lines into the caller (they would name
  the callee's frame in the caller's code).
* Best effort: a later cleanup pass that renames a promoted register is not followed, so a range
  can be a few instructions approximate. Variables are never dropped silently.

`--check-vars` snapshots the frame slots each function uses before the passes, and afterwards exits
with status 3 if a `;@var loc=BP+n` points at a slot that no instruction uses any more and that no
`;@home BPn ...` accounts for, if `;@scope` braces do not balance, or if a `;@home` is malformed.

Not done: a variable whose stores are all dead (dead-store elimination) is not yet marked
`optimized-out`; `--check-vars` reports it only when the slot also disappeared.

`testing/vars/run.sh` runs `leaf.asm` (one promoted loop variable), `segment.asm` (a promoted value that
is saved before a call) and `loops.asm` (three variables and a nested block) at every level.

## Testing

`testing/srcmarks/run.sh` injects markers into every test program, optimizes at each level, runs
`--check-src`, and checks that the set of source lines present at `-O1..-O3` is a subset of the lines at
`-O0` (an optimized file may lose lines, never gain one). Without markers the output is byte-identical to
what the previous version produced.
