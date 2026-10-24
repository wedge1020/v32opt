# Feature request for v32sim: a headless per-address execution profile

**Requested by:** v32opt (the assembly optimizer)
**Priority:** high for v32opt's profile-guided optimization; useful to anyone tuning a cartridge
**Size:** small to medium (a counter array, one output writer, two options)

## Why

v32opt's loop passes (inversion, full and partial unrolling, index folding)
trade code size for speed, and whether the trade pays depends on how hot each
loop really is. Today that is a guess: the heuristics are fixed limits, and the
`bench.py --tune` search can only pick one global setting per program.
With a measured profile, v32opt could unroll only the loops that account for
a meaningful share of the cycles and leave the rest small.

What `v32sim -p` provides today is not enough for that:

* it reports per-**opcode** counts and a per-**subroutine** breakdown, which
  says which function is hot but not which *loop* or *basic block*;
* the report is only available from the interactive prompt (`profile`), so a
  headless run (`-r -x --max-frames=N`, the mode every test harness uses)
  cannot get at it;
* it depends on debug files for names.

What is wanted is the plain fact "this instruction address was executed N
times", written out when the run ends.

## Proposed interface

```
--profile-out=FILE        write the execution count of every executed
                          instruction address to FILE when the run ends
                          (implies instruction counting; no prompt needed)
--profile-format=FMT      "text" (default) or "bin"
```

Both work with `-r -x --max-frames=N` and with a run that reaches `HLT`.
They do not need `-p` and do not need debug files.

### Output, text format

One line per instruction address that was executed at least once, sorted by
address, fields separated by a single space, addresses in hex as the
simulator prints them elsewhere:

```
# v32sim address profile v1
# cycles 1505424 frames 6
0x20000000 1
0x20000004 1
0x20000008 128
0x2000000C 128
...
```

* The address is the address of the instruction's first word; the count is the
  number of times that instruction was *fetched and executed* (a taken jump
  counts the jump, not the skipped instructions).
* `# cycles` is the total number of instructions executed in the run, so a
  consumer can turn a count into a share of the run.
* Only cartridge/RAM code needs to be listed; BIOS addresses may be omitted
  with a `--profile-bios` switch or listed unconditionally, whichever is
  simpler (v32opt ignores addresses outside the cartridge).

### Output, binary format (optional, for big runs)

A header line `V32PROF1`, then little-endian 32-bit pairs `address, count`,
sorted by address. Same content as the text form.

## Semantics to get right

* Count by **instruction address**, not by label or subroutine, so that any
  consumer can build basic blocks and loops from the assembly itself. v32opt
  maps addresses back to source lines through the assembler's own output
  (v32as already knows the address of every instruction; see "Related" for a
  way to make that easier).
* Counting must be **deterministic**: the same cartridge run with the same
  `--fixed-clock`/`--seed` gives the same profile.
* A run stopped by `--max-frames` (exit status 2) must still write the file;
  v32opt profiles with a frame budget because games never reach `HLT`.
* Overhead: one increment per executed instruction. A flat array indexed by
  (address - region base) / 1 per region is plenty; there is no need for a
  hash table.
* `WAIT` (which skips to the next frame) should be counted once; the skipped
  cycles are not instructions.

## Related requests (smaller, optional)

1. **`--profile-out` with `--cart-asm-debug`.** When a cartridge asm debug file
   is loaded, add the label that covers each address as a third column
   (`0x20000008 128 __for_1_start`). This makes the profile readable by a
   human with no extra tool. v32opt does not require it.
2. **`--profile-top=N`.** Print the N hottest addresses (or the hottest
   labels, when names are available) to stderr at the end of the run, for a
   quick look without a tool.
3. **A deterministic cycle counter option** (`--fixed-cycles`, reads of
   `TIM_CycleCounter` return 0): games that seed their RNG from the cycle
   counter (crisp-game-lib does) cannot otherwise be compared word for word
   across builds of different speed. `--fixed-clock` and `--seed` (already
   added) cover the calendar clock and the RNG port only.
4. **`TIM_CurrentTime` in the host-clock path multiplies seconds by 60**
   (`tm_sec * 60` in `ioports.c`); it looks like a typo for a plain seconds
   sum. Not related to profiling, noticed on the way.

## What v32opt will do with it

* `bench.py --profile`: run each build once for a profile, then pass the
  hot-loop list to the optimizer.
* A new `v32opt --profile FILE` input (plus the assembler's address map):
  loop-unroll, loop-unroll-partial and loop-invert are applied to loops whose
  share of the cycles is above a threshold and skipped for cold ones, which
  keeps the code small where speed does not matter and lets the hot loops use
  larger factors than the global default.
* The same data can drive function-inlining decisions and report the hottest
  loops after a build.

## Acceptance tests

1. A cartridge with a loop `for (i = 0; i < 100; i++)` whose body is 5
   instructions: the profile shows each body instruction with count 100 (the
   loop test with 101 or 100 depending on the form) and the instructions
   before and after the loop with count 1.
2. `--max-frames=3` on a program that never halts still writes the file, with
   `# cycles` equal to the number of instructions executed.
3. Two runs of the same cartridge with `--fixed-clock --seed=1` produce
   byte-identical profile files.
4. The sum of all counts equals `# cycles` (excluding `WAIT` idling).
