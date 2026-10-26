# Differential testing (optional tooling)

The unit tests under `testing/` check what each pass *does* to the assembly.
These scripts check that the optimized program still *behaves* the same: a
program is compiled, optimized with `v32opt`, assembled with `v32as` and run
in `v32sim`, and the final RAM image (or, for Lua, the program's result
globals), the self-check counters and the cycle count (one per instruction
on Vircon32) of every variant are compared with the unoptimized build.

Three front ends, one harness:

| Script | Input | Compiler |
| --- | --- | --- |
| `run.py` | `.c` | `v32cc` (default), or the stock Vircon32 `compile` with `COMPILER=stock` |
| `run.py` | `.asm` | none: hand-written assembly is used as is |
| `run_lua.py` | `.lua` | `v32lua` (the optimizer is run with `-L lua`) |

It needs the sibling projects, built, in the same parent directory as this
one: `v32cc/src/v32cc`, `v32as/bin/v32as`, `v32sim/bin/v32sim` and, for Lua,
`v32lua/bin/v32lua` (override with `V32CC`, `V32AS`, `V32SIM`, `V32LUA`;
`V32OPT` selects the optimizer, default `bin/v32opt`). The stock compiler is
`compile` from `ComputerSoftware/DevelopmentTools` (`V32STOCK=...`). No BIOS
or cartridge packer is needed: the harness wraps the VBIN in a minimal
`.v32` itself and starts the simulator at the cartridge entry. C is compiled
in `--standard` syntax (what `v32opt` is written for; v32cc's default
`.local` labels repeat in every function and defeat the liveness analysis).

```bash
# hand-written C and assembly cases, three configurations (STRICT=1: the
# whole RAM image must match the -O0 build, not just the self-check counters)
$ STRICT=1 python3 run.py --flags "-O2" --flags "-O3" programs/*.c asm/*.asm

# the same C through the stock compiler (a different calling convention!)
$ COMPILER=stock V32STOCK=~/ComputerSoftware/build/compile STRICT=1 \
      python3 run.py --flags "-O2" programs/*.c

# random programs (seeds are reproducible)
$ for s in $(seq 1 100); do python3 gen.py $s work/g$s.c; done
$ STRICT=1 python3 run.py --flags "-O2" --flags "-O3" work/g*.c

# random Lua
$ for s in $(seq 1 100); do python3 genlua.py $s work/l$s.lua; done
$ python3 run_lua.py --flags "-O2" --flags "-O3" work/l*.lua
```

Output is one line per program and configuration, then totals over the
programs where every variant finished (a run is given `MAXF` frames, default
40 = 10M cycles, before it counts as timed out and is skipped). The exit
status is non-zero if any variant failed its checks, differed from the
baseline, or could not be built.

* `run.py`, `run_lua.py`  the harnesses. Build products go in `work/`
  (`WORK=` to move it).
* `gen.py`  random C: loops, nesting, break/continue, early return, switch,
  calls, address-taken locals, struct locals, accumulators read only through
  a global. `GEN_NF=<n>` sets the number of functions (a few hundred makes a
  large input for timing the optimizer); `GEN_CALLY=1` nests calls in
  expressions.
* `genreg.py`  register-pressure C for the **callee-save** check: deeply
  nested expressions that keep up to a dozen partial results in registers
  across calls to functions the promote passes want to promote into. Only
  the stock compiler's output has that property (it saves the registers it
  uses and its callers rely on it), so run it with `COMPILER=stock`.
* `genlua.py`  random Lua: loops, multi-value returns used by their callers
  (v32lua returns extra values in R2/R3) and passed through wrappers,
  tables and strings, nested calls. The results are the globals `r_*`.
* `check.h`, `check_stock.h`  the self-checking support the C programs use
  (`CHECK(cond)`; RAM[0] = failures, RAM[1] = checks) for v32cc and for the
  stock compiler; `tostock.py` rewrites a test program into the stock
  dialect (no `?:`/`__LINE__`, `int[N] name`, `int* p`, `void main`).
* `programs/` hand-written C: `alias.c` (a pointer made outside a loop that
  reaches a slot the loop uses), `edge.c` (goto into a loop, struct local
  reached through a pointer, nested loops, continue/break), `reenter.c` (a
  loop re-entered by an enclosing one), `loops.c`, `goto.c`.
* `asm/` hand-written assembly: registers read by callees, saved by
  callees, passed through them; liveness over branches, loops and a jump
  table; recursion with registers kept across calls.

Programs that store code addresses in RAM (function pointers) differ in RAM
whenever the code size changes; compare them without `STRICT`.

## Known limits of what this proves

The random programs are bounded loops over `int` arithmetic: they find
miscompiles in the register and frame logic, not in every instruction
pattern a real game contains (floating point, the GPU/SPU ports, MOVS/SETS
block operations are untouched by them). The Lua programs avoid printing and
the GPU, since the simulator runs them without a BIOS.

## Compiler hints (`HINTS=1`)

`HINTS=1 python3 run.py ...` first runs `hinter.py` on each compiled program. It adds
`;@hints 1 abi=caller-save` and a conservative `;@fn ... escapes=` line per function
(computed from the `LEA [BP-n]` offsets), so the hint-driven passes are compared with
the unoptimized program in v32sim. See `doc/hints.md`.

The harness builds ROMs in v32sim's strict format (128-byte header, `V32-CART` /
`V32-BIOS`, version 1.0), via `v32_file()` and `bios_stub()` in `run.py`.

`EMIT=1` makes `run.py` pass `--emit-hints` to v32cc, so the hints come from the compiler
itself (use instead of `HINTS=1`; needs a v32cc that writes them).
