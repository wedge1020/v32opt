# Where the cycles are: notes on the loop passes

Status: dead-register/copy cleanup (-O1), loop inversion and full
unrolling of constant-trip loops (-O3) are implemented. Pointer strength
reduction and partial unrolling below are still proposals.

Measured on the simulator (`v32sim`), where every instruction is one cycle,
so cycles = instructions executed. Two loops, each hand-optimized one step at
a time from what `v32opt -O2` (with the promote passes) emits for `v32cc
--standard` output. Every variant was assembled, run, and checked against the
original result (`testing/differential/` has the harness; the variants
below were applied to the emitted `.asm` by hand). Cycle figures are per call
of the function, including about 40 cycles (sum) / 14 cycles (clear) of
call, prologue and calling-loop overhead.

## 1. `int sum(int n)`: `for (i = 0; i < n; i++) s += arr[i];`, n = 40

| step | cycles / call | vs. -O2 |
| --- | ---: | ---: |
| `-O2` as emitted before promotion | 681 | |
| + promotion (promote-regs / -leaf; what `-O2` now emits, about) | 682 | +0% |
| + dead register writes / copies removed (below) | 399 | -41% |
| + loop inversion | 359 | -47% |
| + pointer instead of `global_arr + i` | 320 | -53% |
| + unrolled x4 (remainder loop for `n % 4`) | 167 | -75% |

## 2. `void clear16(void)`: `for (i = 0; i < 16; i++) arr[i] = 0;`

| step | cycles / call | vs. -O2 |
| --- | ---: | ---: |
| `-O2` + promotion, as emitted | 237 | |
| + dead register writes / copies removed | 171 | -28% |
| + loop inversion | 137 | -42% |
| + pointer instead of `global_arr + i` | 107 | -55% |
| fully unrolled (the trip count is a constant) | 41 | -83% |

## What each step is

**Cleanup after promotion (the one that matters first).** Promotion turns
`mov R0,[BP-2]` into `mov R0,R4`: the same cycle count. What it buys is that
the copies can now be cancelled -- and nothing cancels them yet. A loop
counter's increment is still `mov R0,R4 / mov R1,R4 / iadd R1,1 / mov R4,R1`
(one instruction would do: `iadd R4,1`), `ilt R0,R6` is still preceded by
`mov R1,R6`, whose R1 nobody reads. Two passes, both driven by `liveness.c`
(already used by `peephole-zero-test`):

* *dead register write elimination*: an instruction that only writes a
  register nobody reads afterwards (`reg_dead_from(next, Rd)`) goes. Pure
  ALU/MOV/compare/convert only; not IN, POP, IDIV/IMOD, anything that can
  fault or has effects.
* *copy coalescing* in straight-line windows: `mov Rt,Rs / ... OP Rt,X /
  ... mov Rs,Rt` with Rs untouched in between and Rt dead afterwards
  becomes `OP Rs,X` (the window's `Rt` occurrences renamed to `Rs`).

`is_live_out_register()` makes R2 and R3 live at every `RET`, because
v32lua returns extra values there. In `-L c` that blocks half of the
temporaries (the compiler's expression temporaries are R0, R1, R2, ...):
letting it depend on the language mode is part of making these work.

With this in place, promoting parameters (read-only `[BP+N]`) starts to pay
too -- the loop bound `n` becomes `ilt R0,R6` -- and, if no `[BP-N]` is left,
the `ISUB SP,N` can go, which lets `omit-frame-pointers` take the 5
instructions of the frame (push/mov/isub/mov/pop) off every call of small
leaf functions.

**Loop inversion.** The compilers emit a top-tested loop:

    start: <test> ; jf R0,end ; <body> ; continue: <step> ; jmp start ; end:

two branches per iteration. Testing at the bottom,

    <test> ; jf R0,end ; start: <body> ; continue: <step> ; <test> ; jt R0,start ; end:

has one, at the price of a second copy of the test (2-4 instructions). Safe
when nothing else branches to `start` (the checks `promote-loops` already
does) and the test is straight-line register code. -1 cycle per iteration.

**Strength reduction of array indexing.** `mov R1,global_arr / iadd R1,R4 /
mov R1,[R1]` per element becomes one pointer register stepped by the
element size, once the index is a promoted register that is only used as
`base + i`. -2 cycles per iteration here.

**Unrolling.** Per iteration the loop overhead (test, step, branch) is 5 of
the 9 instructions that are left after the cleanup. Two cases:

* constant, small trip count (very common in game code: 4, 8, 16, 64): unroll
  completely, give each copy its own constant `i`, and let
  `constant-folding` and the immediate passes turn `global_arr + i` into
  absolute addresses and `imul i,3` into constants. This is the -83% above.
* unknown count: unroll by 2 or 4 with a remainder loop (as in the table).
  Needs the induction variable (`i += c`, compared with a loop-invariant
  bound, not otherwise written in the body).

Start with bodies that are straight-line (no inner labels, so no label
cloning), at most ~8 instructions, factor limited so the loop stays under
~64 instructions. Code size grows; the cartridge has room, a frame has
250000 cycles.

## Order that makes sense

1. dead register write elimination + copy coalescing (everything below
   builds on it, and it makes the promote passes pay off on their own);
2. loop inversion (simple, local, -1/iteration);
3. full unrolling of constant-trip loops (biggest single win, needs 1 and
   the constant folder);
4. pointer strength reduction, partial unrolling, parameter promotion and
   frame shrinking.

## A note on the compiler's labels

`v32cc` writes `.local` labels by default (`.return:`, `.for_3_start:`, ...),
which repeat in every function. `v32opt` scopes them to their function when it
reads the file (`.return` -> `.__sc17__return` internally, restored on output),
so the optimizer sees unique names and the default syntax optimizes as well as
`--standard`.
