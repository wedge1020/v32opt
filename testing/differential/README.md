# Differential testing (optional tooling)

The unit tests under `testing/` check what each pass *does* to the assembly.
These scripts check that the optimized program still *behaves* the same: C
sources are compiled with `v32cc`, optimized with `v32opt`, assembled with
`v32as` and run in `v32sim`, and the final RAM image, the self-check counters
and the cycle count (one per instruction on Vircon32) of every variant are
compared with the unoptimized build.

They need the sibling projects, built, in the same parent directory as this
one: `v32cc/src/v32cc`, `v32as/bin/v32as` and `v32sim/bin/v32sim` (override
with `V32CC`, `V32AS`, `V32SIM`; `V32OPT` selects the optimizer, default
`bin/v32opt`). No BIOS or cartridge packer is needed: `run.py` wraps the VBIN
in a minimal `.v32` itself and starts the simulator at the cartridge entry.
`v32cc` is used in `--standard` syntax (what `v32opt` is written for).

```bash
# a few hand-written programs, three configurations (STRICT=1: the whole RAM
# image must match the -O0 build, not just the self-check counters)
$ STRICT=1 python3 run.py --flags "-O2" \
                          --flags "-O2 -fno-promote-loops" \
                          --flags "-O3" programs/*.c

# random programs (seeds are reproducible)
$ for s in $(seq 1 100); do python3 gen.py $s work/g$s.c; done
$ STRICT=1 python3 run.py --flags "-O2" --flags "-O3" work/g*.c
```

Output is one line per program and configuration, then totals over the
programs where every variant finished (a run is given `MAXF` frames, default
40 = 10M cycles, before it counts as timed out and is skipped). The exit
status is non-zero if any variant failed its checks, differed in RAM (with
`STRICT=1`), or could not be built.

* `run.py`  the harness. Build products go in `work/` (`WORK=` to move it).
* `gen.py`  the random program generator; `GEN_NF=<n>` sets the number of
  functions (a few hundred makes a large input for timing the optimizer).
* `check.h` the self-checking support the programs use (`CHECK(cond)`;
  RAM[0] = failures, RAM[1] = checks), identical to the one in the `v32cc` tests.
* `programs/` hand-written cases: `alias.c` (a pointer made outside a loop
  that reaches a slot the loop uses), `edge.c` (goto into a loop, struct
  local reached through a pointer, nested loops, continue/break),
  `loops.c`, `goto.c`.

Programs that store code addresses in RAM (function pointers) differ in RAM
whenever the code size changes; compare them without `STRICT`.
