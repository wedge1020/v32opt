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
* `gen.py`  random C (`GEN_LIVE=1` below): loops, nesting, break/continue, early return, switch,
  calls, address-taken locals, struct locals, accumulators read only through
  a global. `GEN_NF=<n>` sets the number of functions (a few hundred makes a
  large input for timing the optimizer); `GEN_CALLY=1` nests calls in
  expressions.
* `genreg.py`  (`GENREG_LIVE=1` below) register-pressure C for the **callee-save** check: deeply
  nested expressions that keep up to a dozen partial results in registers
  across calls to functions the promote passes want to promote into. Only
  the stock compiler's output has that property (it saves the registers it
  uses and its callers rely on it), so run it with `COMPILER=stock`.
* `genlua.py`  random Lua (`GENLUA_LIVE=1` below): loops, multi-value returns used by their callers
  (v32lua returns extra values in R2/R3) and passed through wrappers,
  tables and strings, nested calls. The results are the globals `r_*`.
* `genasm.py`  random hand-written-style assembly (`genasm.py SEED OUT.asm`, run it like `asm/*.asm`):
  small functions that return results in a random subset of R0..R7, save registers with PUSH/POP (some
  they also read as arguments), read a stack argument (`[sp+1]`), call each other, tail-jump or fall
  through into the next function; the main program calls them directly and through
  `mov rN, _f` / `call rN` and stores every register it reads into consecutive RAM words, some only
  after further calls. The generator tracks which registers hold a defined value, so the programs are
  deterministic whatever the registers held at the start (checked by poisoning R0..R11 with two
  different sets of values and comparing the RAM image). No loops: always terminates. It exists for
  the shapes C cannot express (non-ABI result registers, indirect calls, shared epilogues); on the
  frozen baseline it finds the liveness gaps of `doc/liveness-audit.md`.
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
  table; recursion with registers kept across calls. Also:
  `live_*.asm`, probes for the gaps of `doc/liveness-audit.md` (`live_deep`,
  a callee that reads through `[BP+3]` a register its caller pushed before
  the argument; `live_trans`, a result register
  written by a callee's callee and read by `main`; `live_saveout`, a
  register written after its save was popped, which is a result; run by
  `testing/peephole-dead-regs`); `fold_*.asm`,
  constant folding into a block that is also entered by a `CALL` or by
  falling through (`testing/constant-folding-entries`);
  `save_args_v32cc.asm` / `save_args_stock.asm`, promotion in functions that
  stage call arguments with `MOV [SP+k], x` into one `ISUB` (v32cc layout)
  or into a second `ISUB` (stock layout) (`testing/promote-save`). The
  suites of `make tests` that run these programs say so in their `run.sh`.

### Liveness shapes (`GEN_LIVE=1`, `GENLUA_LIVE=1`, `GENREG_LIVE=1`)

Off by default: a seed gives the same program as before (checked by generating seeds 1..20, also with
`GEN_NF`/`GEN_CALLY`, with the old and new script and comparing the files). Each switch adds statements
that stress what `reg_dead_from()` has to know, in programs that stay deterministic and bounded:

* `GEN_LIVE=1` (`gen.py`): four small functions `h0..h3(int, int)` called directly and through a table
  `htab[]` and a local function-pointer variable `fp` (indirect calls in expressions, in loops, a pointer
  called twice around other work); a variable assigned at the end of a loop body and read at the top of
  the next iteration, not used after the loop; loops with calls in the body; loops with three exits
  (`break`, `return`, `goto` out) that read different variables at each exit. `htab[]` is cleared at the
  end of `main`, so no code address stays in RAM and `STRICT=1` still applies.
* `GENLUA_LIVE=1` (`genlua.py`): functions `h0..h2` returning two values, stored in the global table `ht`
  and called through it, functions passed as arguments and called (`ap`, `ap2`), both results used after
  the indirect call; `for`/`while`/`repeat` loops with an indirect call, a carried value and a `break`.
* `GENREG_LIVE=1` (`genreg.py`, `COMPILER=stock`): a loop with a carried value, a call of the callee in
  the body, and a `break` and a `return` that read different locals.

Programs that store code addresses in RAM (function pointers) differ in RAM
whenever the code size changes; compare them without `STRICT`.

## Fuzzing (`fuzz.sh`)

```bash
$ V32OPT=/path/to/v32opt bash fuzz.sh -n 100 -s 1          # 100 seeds per generator and mode
$ bash fuzz.sh -g genasm,gen-live -n 200 -s 1000            # some generators only, other seeds
```

`fuzz.sh` generates the seeds `FIRST .. FIRST+N-1` for `gen`, `gen-live` (`GEN_LIVE=1`), `genlua`,
`genlua-live`, `genasm` and, when the stock `compile` is built (`V32STOCK=`, or
`/home/claude/stocktools/compile`), `genreg` and `genreg-live`. It runs `run.py` (`STRICT=1`) or
`run_lua.py` over them in groups of 20, with `-O1 -O2 -O3 -Os` (C and assembly also
`-O3 -floop-unroll-partial`; the promote passes are part of `-O2`, so there is no separate set for
them), and prints per generator the number of programs, the FAIL / ERROR / SKIP lines and the failing
seeds. Programs and logs stay in `work/fuzz/<tag>/` (`WORK=`). The exit status is 1 when any program
FAILed or ERRORed (a SKIP is a program over `MAXF` frames: the random Lua and C programs are not all
short) and 2 when the set-up is wrong. `-t` limits one `run.py` call (default 1800 s). The simulator is
started with `--fixed-clock --seed=1`, so a run repeats exactly (`--fixed-cycles` is not used: the
harness reads the cycle counter).

To reduce a failure: lower the level (`-O1`, then single passes, `-fno-...`), then
`v32opt --trigger-max=N` to find the first transformation that breaks it (see `USAGE.md`); for the
generators, `GEN_NF=2` and fewer functions give shorter programs.

## Stock tools without SDL2

The stock `compile`, `assemble` and `packrom` build without SDL2 (it is used only for the path of the
executable): compile the sources listed in `DevelopmentTools/CMakeLists.txt` with g++ and a one-function
`SDL.h` that answers `SDL_GetBasePath()` from `/proc/self/exe`; copy `DevelopmentTools/Data/include`
next to `compile`. Built that way they are in `/home/claude/stocktools/`. `testing/lua-cold/runcart.sh`
(used by `testing/lua-audit/run.sh`) uses `assemble`/`packrom` when `V32TOOLS` names a directory with
them; without them it assembles with `v32as`, wraps the VBIN in a cartridge like `run.py` and starts
at the cartridge entry (`-E`) behind a stub BIOS, which gives the same results (one instruction fewer:
the BIOS's jump).

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
