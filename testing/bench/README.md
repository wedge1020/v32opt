# Benchmarking across languages, compilers and optimizer settings

`bench.py` builds every implementation found in a *suite* directory with every
toolchain that applies to it, optionally runs each result through `v32opt`
at several settings, executes everything in `v32sim` and reports **size**
(assembled words) and **cycles** (instructions executed, one cycle each, from
the entry point to the final `HLT`), checking on the way that every build still
computes the right answer.

```bash
python3 bench.py primes                                       # the defaults
python3 bench.py primes --scenarios O2,O3 --csv out.csv --md out.md
python3 bench.py primes --scenario O3-noloops="-O3 -fno-loop-unroll -fno-loop-unroll-partial"
python3 bench.py primes --impl 'primes_t*' --baseline primes/v32cc/-O0
```

## The suite

`primes/` holds one program in five implementations:

| file | language | built with |
| --- | --- | --- |
| `primes.asm` | assembly, plain loops | `v32as` |
| `primes_tuned.asm` | assembly, loops tested at the bottom | `v32as` |
| `primes.c` | C | `v32cc` **and** the stock Vircon32 `compile` |
| `primes.cpp` | C++ (a class with the variants as methods) | `v32c++` -> C -> stock `compile` |
| `primes.lua` | Lua | `v32lua` (and `v32opt -L lua`) |

The program counts the primes up to N=512 by trial division with the eight
variants of the v32lua `primes` demo (brute force, early break, odds only,
stop at the square root, and their combinations) and stores the **sum of
the eight counts** in a global named `result`: 8 x 97 = **776**
(`expect.txt`). There is no display or input, so the run is deterministic and
fast.

## Dropping in another implementation

Add a file to the suite directory; it is picked up by its extension.

* **The contract**: no I/O; run from start to `HLT`; leave the answer in
  `result` -- a C/C++ global `int result;`, a Lua global `r_result`, or in
  assembly a `%define global_result <ram address>` with the program storing
  the answer there. To change the workload, change it in *every* file (and
  `expect.txt`): a build whose answer differs is reported as `WRONG` and kept
  out of the comparisons.
* **C** is compiled twice, by `v32cc` (`--standard`) and by the stock compiler;
  stick to the common subset (`int`, loops, functions, globals; no `?:`,
  arrays are rewritten from `int a[N]` to the stock `int[N] a` form by
  `testing/differential/tostock.py`).
* **Assembly** is assembled as is, in `--standard` syntax (lowercase or
  uppercase mnemonics, `_labels`). Whatever `v32opt` does to it is shown too.
* A second file in the same language is just another row
  (`primes_sieve.c`, `primes_fast.asm`, ...).

## The report

1. For every implementation and toolchain, each scenario against that build's
   own unoptimized (`-O0`) output: how much `v32opt` (and which setting) gains.
2. All builds side by side, each at its **fastest** scenario, against the
   **baseline** row (default: the first file, first toolchain, `-O0`; change it
   with `--baseline impl/toolchain/scenario`): `speed vs base` is
   baseline cycles / cycles (above 1 = faster), `size vs base` the word count
   difference.
3. All builds unoptimized: what the languages and compilers give before
   `v32opt` is involved.

`--csv`, `--md` and `--json` write the numbers out; the exit status is 1 if a
build failed or gave a wrong answer.

## Tuning a build: `--tune`

`--tune` searches, for every build, for the `v32opt` command line that suits
**that program**: starting from `-O3` (`--tune-from`), it tries switching each
pass off (`-fno-inline`, `-fno-loop-unroll-partial`, ...) and moving each of
the loop limits (`-floop-unroll-max-trips=...`, `-floop-unroll-partial-factor=...`,
see the *Tuning the loop passes* table in the main README), keeps the single
change that helps most, and repeats (at most `--tune-steps`, default 8) until
nothing improves. Every candidate is a full build and run, and one with a wrong
answer is discarded. The result is an extra `tuned` row in the tables, plus the
chosen flags and the steps that led there.

```bash
python3 bench.py primes --tune                          # fewest cycles
python3 bench.py primes --tune --tune-metric product    # cycles x size
python3 bench.py primes --tune --tune-growth 25         # at most 25% bigger than -O0
python3 bench.py primes --tune --tune-metric size       # smallest
```

Ties are broken by the other metric, so a flag that changes nothing in cycles but
saves code is still taken. The flag line it prints is what to put in the
program's build script.

## Scenarios

Default: `-O0` (no optimizer) plus `-O1,-O2,-O3,-Os`. `--scenarios O2,O3`
chooses the levels, and `--scenario NAME="FLAGS"` adds any `v32opt` command
line, e.g. one pass off or a single pass on top of a level:
`--scenario O2+unroll="-O2 -floop-unroll"`. Lua builds get `-L lua` added
automatically.

## Tools

`v32as` and `v32sim` are required, `v32opt` unless `--no-opt`; `v32cc`, the
stock `compile`, `v32c++`, `v32lua` only for the files that need them (a
missing one skips those rows with a note). They are looked up in the
`V32AS`, `V32SIM`, `V32OPT`, `V32CC`, `V32STOCK`, `V32CXX` and `V32LUA`
environment variables, then in the sibling projects next to `v32opt`
(`../v32as/bin/v32as`, ...), then on the `PATH`. Needs Python 3.

## A sample run (primes, N=512)

The per-build tables (every scenario against that build's own `-O0`) come first; the
summaries are:

```
== all builds, each at its fastest scenario (against the baseline)
build               scenario  words   cycles  speed vs base  size vs base
------------------  --------  -----  -------  -------------  ------------
primes / stock           -O3    998  1106526          1.55x       +165.4%
primes / v32cc           -O3   1014  1106528          1.55x       +169.7%
primes / v32c++          -O3   1061  1106557          1.55x       +182.2%
primes / asm             -O3    428  1505424          1.14x        +13.8%
primes_tuned / asm       -O3    436  1505424          1.14x        +16.0%
primes / v32lua          -O3  12254  2814256          0.61x      +3159.0%

== all builds unoptimized (-O0)
build               words   cycles  speed vs base  size vs base
------------------  -----  -------  -------------  ------------
primes_tuned / asm    392  1508572          1.14x         +4.3%
primes / asm          376  1716669          1.00x         +0.0%
primes / v32cc        826  2961476          0.58x       +119.7%
primes / stock        821  2961485          0.58x       +118.4%
primes / v32c++      1553  2961526          0.58x       +313.0%
primes / v32lua     12833  4956274          0.35x      +3313.0%
```
