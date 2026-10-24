# Using v32opt: a plain-language guide

This guide is for people who **make Vircon32 games and programs** and want them
to run faster or take less space. It assumes you can build your project but
does not assume you know anything about compilers. For the details of every
optimization see the [README](README.md); for every option, `man v32opt`.

Contents

1. [What v32opt does, and the one rule](#1-what-v32opt-does-and-the-one-rule)
2. [Quick start](#2-quick-start)
3. [Choosing a level](#3-choosing-a-level)
4. [Checking that the optimized program is still right](#4-checking-that-the-optimized-program-is-still-right)
5. [When something breaks](#5-when-something-breaks)
6. [Tuning the loop passes](#6-tuning-the-loop-passes)
7. [Profiling: letting your program tell v32opt what matters](#7-profiling-letting-your-program-tell-v32opt-what-matters)
8. [Benchmarking: comparing versions, languages and settings](#8-benchmarking-comparing-versions-languages-and-settings)
9. [Putting it in your build script](#9-putting-it-in-your-build-script)
10. [Questions and troubleshooting](#10-questions-and-troubleshooting)

---

## 1. What v32opt does, and the one rule

Your compiler (the Vircon32 C compiler, `v32c++`, `v32lua`) turns your source
into **assembly**, a text file of machine instructions. `v32opt` reads that file
and writes a better one: fewer instructions, so a smaller cartridge and a
program that runs in fewer **cycles** (on Vircon32 every instruction is one
cycle, 250,000 per frame).

```
your source  -->  compiler  -->  game.asm  -->  v32opt  -->  gameOpt.asm  -->  assemble  -->  .vbin  -->  cartridge
```

> **The one rule: never ship an optimized build you have not played.**
> `v32opt` is carefully tested, but it rewrites your program. Treat its output
> like a new build: run it, look at the whole game, and keep the unoptimized one
> to compare against. Section 4 shows how to check more rigorously.

## 2. Quick start

```bash
compile src/main.c -o obj/main.asm                    # 1. your compiler, as always
v32opt obj/main.asm -O2 -o obj/mainOpt.asm            # 2. optimize
assemble obj/mainOpt.asm -o obj/mainOpt.vbin          # 3. assemble the optimized file
packrom rom.xml -o game.v32                           # 4. pack (rom.xml points at mainOpt.vbin)
```

That is all most projects need. Add `-v` to see what it did, or `-t` for a short
summary:

```
$ v32opt obj/main.asm -O2 -o obj/mainOpt.asm -v
...
Optimization complete: 18432 total optimizations applied.
```

If your compiler is `v32lua`, add `-L lua`. If you use `v32cc`, give it
`--standard` (explained in section 10).

## 3. Choosing a level

| Level | What it does | Typical effect on cycles / size | Risk |
| --- | --- | --- | --- |
| `-O0` | nothing (the default) | | none |
| `-O1` | tidies the compiler's output | 3% fewer cycles, 4-6% smaller | very low |
| `-O2` | + keeps variables in registers, removes dead code and unused functions | 5-7% fewer cycles, 6-14% smaller | low |
| `-O3` | + inlines small functions, turns loops inside out, unrolls loops | 9-11% fewer cycles (much more on loop-heavy code); size anywhere from 13% smaller to 25% bigger than `-O0`, depending on how many loops it copies | moderate |
| `-Os` | `-O3` without the loop copying | like `-O2` plus inlining | moderate |

(Measured on a few hundred generated programs; your game will differ. Loop-heavy
code does much better: the primes benchmark below runs **63% faster** at `-O3`.)

Rules of thumb:

* Start with `-O2`. Always safe to try.
* Use `-O3` when speed matters (a frame that does not fit in 250,000 cycles).
  It makes some code bigger because it **copies** loop bodies; that is the trade.
* Use `-Os` when the cartridge is too big.
* You can mix: `-O3 -fno-inline` is `-O3` without inlining; `-O2 -floop-unroll` adds
  one extra pass. Names are in `v32opt --help` and the README.

## 4. Checking that the optimized program is still right

From quickest to most thorough:

1. **Play it.** Run the original and the optimized cartridge side by side in the
   Vircon32 emulator and play the same session. Look at graphics, sound, saves.
2. **Compare numbers, not just looks.** If your program ends with results in
   memory (a test, a benchmark), `v32sim` can print memory after a run
   (`v32sim -r -x --max-frames=300 --dump=200@0 game.v32`) so you can compare
   the original's and the optimized one's output with `diff`. Use
   `--fixed-clock --seed=1 --fixed-cycles` so both runs see the same "time" and
   random numbers; otherwise a game that seeds its random numbers from the clock
   will legitimately differ.
3. **Run the checks that come with v32opt** (developers; see `testing/`):
   `make tests` (every pass on small cases), `testing/lint/run.sh` (no pass may
   use a register your code did not), and `testing/differential/` (random programs
   compared before and after). These need the Vircon32 tools built next to
   v32opt; `testing/differential/README.md` explains.

A difference of **one frame** in a long run (a counter that is 298 instead of 297
after 300 frames) is normal: the optimized program is faster, so it reaches
things sooner.

## 5. When something breaks

An optimization pass can be wrong for *your* code even if it is right in every
test. You can find which one in a few minutes.

**Step 1: find the level.** Rebuild with `-O1`, then `-O2`, then `-O3`. The first
level where the bug appears tells you which family of passes to suspect.

**Step 2: find the pass.** Turn passes off one at a time with `-fno-<name>`:

```bash
v32opt game.asm -O3 -fno-inline                   # still broken?
v32opt game.asm -O3 -fno-loop-unroll -fno-loop-unroll-partial
v32opt game.asm -O3 -fno-promote-regs -fno-promote-leaf -fno-promote-loops
```

`v32opt --help` lists all the names. When switching one off fixes it, you have
found the guilty pass: keep it off in your build and please report it with the
`game.asm` that triggers it.

**Step 3 (optional): find the exact change.** `--trigger-max=N` lets only the first
N transformations happen. Raise N until the bug appears; the last one applied is
the culprit. Add `-d` and every changed place is marked in the output with a
`; [DEBUG pass-name]` comment so you can find it.

```bash
v32opt game.asm -O3 --trigger-max=500 -d -o gameOpt.asm     # fine?  try 1000, 2000, ...
```

## 6. Tuning the loop passes

`-O3` has a single set of limits that suits an average program. Yours may want
different ones. The loop passes copy loop bodies; these settings say how much
copying is allowed:

| Setting | Default | What it does |
| --- | --- | --- |
| `-floop-unroll-max-trips=N` | 16 | loops that run a fixed number of times, up to N, are replaced by N copies |
| `-floop-unroll-max-size=N` | 96 | ... if the result is at most N instructions |
| `-floop-unroll-partial-factor=N` | 0 (automatic: 4, or 2 for big bodies) | loops with an unknown length run N rounds at a time; 2 to 8 |
| `-floop-unroll-partial-max-body=N` | 24 | longest loop body that gets that treatment |
| `-floop-unroll-partial-min-body=N` | 1 | shortest |
| `-floop-invert-max-test=N` | 8 | longest loop test copied by loop inversion |

A bigger factor or limit: faster loops, more code. Smaller: the reverse.
Try by hand:

```bash
v32opt game.asm -O3 -floop-unroll-partial-factor=8 -o gameOpt.asm
```

...or let the **benchmark tool search for you** (section 8: `--tune`), then paste
the flags it prints into your build script.

## 7. Profiling: letting your program tell v32opt what matters

Copying a loop is only worth it if the loop **runs a lot**. A loop that runs once
per level does not need to be faster; making it bigger just wastes cartridge
space. A **profile** is a measurement of how often each line of your program
runs; with one, `v32opt` unrolls the hot loops harder and leaves the cold ones
alone.

You need `v32sim` from the version that has `--profile-out` (the `dev` branch at
the time of writing) and `v32as`.

```bash
# 1. profile the UNOPTIMIZED assembly: runs it in the simulator for 600 frames
scripts/v32prof.py obj/main.asm -o obj/main.prof --frames 600
#    v32prof: 13120400 instructions executed, 18211 lines covered -> obj/main.prof

# 2. give the SAME file and the profile to v32opt
v32opt obj/main.asm -O3 --profile obj/main.prof -o obj/mainOpt.asm
```

What to know:

* **Profile the same file you optimize.** The profile is by line number. If the
  file changes, profile again. (A profile that does not fit is detected and
  ignored with a message.)
* **The program runs by itself**, with no controller. If your game sits at a title
  screen until a button is pressed, that is all the profile will see. Either run
  it longer (`--frames`), or give the program a "demo mode" or fixed workload for
  profiling (a benchmark, a replay), or profile a build that starts in the
  interesting part.
* A loop is **hot** if it takes at least 2% of all executed instructions, **cold**
  if under 0.1%. Change that with `-fprofile-hot=PCT` and `-fprofile-cold=PCT`.
* It only changes the loop passes (`-O3`). At `-O2` the profile makes no difference.
* On the primes benchmark, the profile-guided `-O3` is as fast as the best global
  setting but with about **10% less code**, because only the loops that matter grew.
* `v32prof.py` needs `V32AS` and `V32SIM` to find the tools (environment variables,
  or `v32as`/`v32sim` on your `PATH`, or next to the `v32opt` directory).

## 8. Benchmarking: comparing versions, languages and settings

`testing/bench/bench.py` builds one program in several forms, runs each in the
simulator, and prints **size** (words) and **cycles** side by side. Use it to:

* see how much `v32opt` gains on your code, level by level;
* compare **languages and compilers** on the same job (hand assembly, C with the
  two C compilers, C++, Lua);
* find the best `v32opt` settings (`--tune`) or see what a profile buys (`--profile`).

### A benchmark is a folder

```
testing/bench/primes/
    primes.c          C (built by v32cc AND the stock compiler)
    primes.cpp        C++ (v32c++ -> C -> stock compiler)
    primes.lua        Lua (v32lua)
    primes.asm        hand-written assembly
    primes_tuned.asm  another assembly version
    expect.txt        the right answer: 776
```

All of them solve **the same problem** and store the answer in a variable named
`result` (Lua: `r_result`; assembly: `%define global_result <address>`). The tool
checks the answer, so a version that computes the wrong thing is flagged, not
compared. To add your own: copy a file, keep the same `result`, change the
algorithm, and put it in the folder. Nothing else to register.

### Running it

```bash
cd testing/bench
python3 bench.py primes                                   # defaults: -O0, O1, O2, O3, Os
python3 bench.py primes --scenarios O2,O3 --csv out.csv   # fewer levels, save the numbers
python3 bench.py primes --tune                            # search for the best v32opt settings
python3 bench.py primes --profile                         # add profile-guided rows
python3 bench.py primes --impl primes_tuned               # only some files
```

It needs `v32as` and `v32sim`, plus the compilers for the languages you drop in.
A missing compiler just skips those rows (the output says so). To point it at a
tool that is not found automatically, set `V32STOCK=/path/to/compile`,
`V32CXX=/path/to/v32c++`, `V32CC`, `V32LUA`, `V32SIM`, `V32AS` or `V32OPT`.

### Reading the result

First, one table per build: each scenario compared with that build's own `-O0`
("how much did v32opt help?"):

```
== primes / v32cc
scenario  words   cycles  size vs -O0  cycles vs -O0
-O0         826  2961476        +0.0%          +0.0%
-O2         562  1719887       -32.0%         -41.9%
-O3        1014  1106528       +22.8%         -62.6%
```

Then every build side by side, each at its fastest setting, against a baseline
(by default the first file unoptimized; choose with
`--baseline primes/v32cc/-O0`). **"speed vs base" above 1.00x means faster than
the baseline**; "size vs base" is the code size difference:

```
build               scenario  words   cycles  speed vs base  size vs base
primes / stock      -O3+prof   1154  1013109          1.69x       +206.9%
primes / asm        -O3         428  1505424          1.14x        +13.8%
primes / v32lua     -O3       12254  2814256          0.61x      +3159.0%
```

Cycles are exact (the simulator counts instructions). Size is in 32-bit words and
includes everything the program contains. Read the languages comparison as a
*rough* guide: it measures how each compiler handles this one program.

### `--tune`: let it find your settings

```bash
python3 bench.py primes --tune                          # fewest cycles
python3 bench.py primes --tune --tune-metric product    # best cycles x size
python3 bench.py primes --tune --tune-growth 25         # at most 25% bigger than -O0
```

For every build it starts at `-O3`, tries switching each pass off and nudging each
limit from section 6, keeps the one change that helps most, and repeats (up to 8
times). The end of the report names the winning settings:

```
primes / v32cc: -O3 -floop-unroll-partial-factor=8
    -O3: 1106528 cycles, 1014 words  ->  tuned: 1013848 cycles (-8.4%), 1302 words (+28.4%)
```

Copy the flags into your build script. It runs your program many times, so on a
large program it can take minutes per step.

## 9. Putting it in your build script

A typical `Make.sh`, with the optimizer optional so a machine without it still
builds:

```bash
OPT_FLAGS="-O3 -floop-unroll-partial-factor=4"

compile src/main.c -o obj/main.asm || exit 1

if command -v v32opt >/dev/null && [ "$SKIP_V32OPT" != 1 ]; then
    # refresh the profile when the program changed (optional)
    scripts/v32prof.py obj/main.asm -o obj/main.prof --frames 600 \
        && PROFILE="--profile obj/main.prof"
    v32opt obj/main.asm -o obj/mainOpt.asm $OPT_FLAGS $PROFILE && ASM=obj/mainOpt.asm
fi

assemble ${ASM:-obj/main.asm} -o obj/main.vbin
```

Build both versions into separate cartridges (`rom.xml` and `rom-opt.xml`) while
you develop, and compare them.

## 10. Questions and troubleshooting

**"It made no difference."** Check `-v`: if it reports few optimizations, the
input may be hand-written assembly (the big wins come from compiler-shaped code:
frames, loops), or compiled with `v32cc` without `--standard`.

**Why `v32cc --standard`?** `v32cc`'s default output uses short repeating labels
like `.return:` in every function. `v32opt` cannot tell them apart, so many passes
quietly skip such code. With `--standard` the labels are unique and everything
works.

**Lua?** Use `-L lua`. The loop passes do not apply to Lua's loops (their shape
differs); you still get `-O1` and `-O2`.

**Do not use `-L lua` on C output** (or the reverse): the two assume different
register conventions and the result will be wrong.

**It runs, but is bigger.** `-O3` copies code. Use `-O2`, or `-Os`, or lower
the loop limits (section 6), or use a profile (section 7) so only hot loops grow.

**The profile step says "v32sim wrote no profile".** Your `v32sim` is too old:
you need one with `--profile-out`.

**The profile is ignored.** The file changed after profiling, or you profiled a
different file than you optimized. Profile again.

**My game differs by a frame or two between builds.** Expected (section 4). It
should not differ in what is drawn or played.

**Where do I report a bug?** In the project repository. Include the `.asm` file
(the unoptimized one) and the exact `v32opt` command line. If you followed
section 5 you can say which pass and which `--trigger-max` value.
