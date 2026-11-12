# Pointer strength reduction (`-floop-strength`): design

Status: implemented as `-floop-strength` (src/peephole/loop_strength.c). The pointer step is placed immediately before the counter step, and an offset form (`arr[i+c]`) was added; otherwise as specified here.

## What it does

`loop-index` folds one constant base into the counter and gives up when the
bound is not an immediate or the counter is also used as a value. This pass
handles those loops with a second register, a pointer that is stepped with the
counter:

        MOV  R6, global_arr          ; preheader, once
        IADD R6, R4
    start:  MOV R0, R4 / MOV R1, [BP+2] / ILT R0, R1 / JF R0, end
        MOV  R1, [R6]                ; was MOV R1, global_arr / IADD R1, R4 / MOV R1, [R1]
        IADD R5, R1
        IADD R4, 1                   ; the step
        IADD R6, 1                   ; the pointer step, right after it
        JMP  start

Invariant: everywhere in the loop except between the two step instructions,
`Rp == BASE + K * Ri` in 32-bit wrap-around arithmetic. IADD, IMUL and SHL all
wrap, so the equality is exact and there is no overflow condition to check.

## Where it runs

A new pass `peephole_loop_strength()` in `src/peephole/loop_strength.c`, option
`loop-strength`, part of `-O3` (not `-Os`). It runs after `loop-index` (which is
cheaper, so it gets first choice; a loop it rewrote has no address groups
left) and before `loop-unroll-partial` and `loop-invert`, on the shape parsed by
`loop_parse_counted()`. The later loop passes must still accept the loop with
the extra step instruction; if one of them refuses it, fix that pass's test,
do not move this pass.

## Address groups

Consecutive instructions (comments may sit between them, labels may not), in
the body `[L.first, L.step)`:

| form | text | value of Rt | replaced by |
| --- | --- | --- | --- |
| unit | `MOV Rt, BASE` / `IADD Rt, Ri` | BASE + Ri | `MOV Rt, Rp` |
| scaled | `MOV Rt, BASE` / `MOV Ru, Ri` / `IMUL Ru, K` (or `SHL Ru, s`, K = 1 << s) / `IADD Rt, Ru` | BASE + K*Ri | `MOV Rt, Rp` |
| frame | `LEA Rt, [BP+off]` / `IADD Rt, Ri` | BP + off + Ri | `MOV Rt, Rp` (K = 1 only) |

`BASE` is an integer immediate operand (a number, a label, a `%define`; its
value is never needed) or a register `Rb` that is not written anywhere in the
loop. Before fixing the forms, compile `testing/differential/programs/*.c` and
the bench programs at `-O3 -fno-loop-strength` and list the address
computations that mention a loop counter: if the compilers emit another common
form (operands swapped, the scale applied first), add it to this table with the
same rules.

**Folding into the use.** If the next instruction `n` after the group mentions
Rt only as the base of one memory operand `[Rt+o]`, no label sits between the
group and `n`, and Rt is dead after `n` (`reg_dead_from(n->next, Rt)`) or `n` is
a `MOV Rt, [Rt+o]`, the operand becomes `[Rp+o]` and the whole group is deleted.
Otherwise the group becomes `MOV Rt, Rp`.

## Legality: refuse the loop when

1. `loop_parse_counted()` fails (so: `start` is referenced only by the back
   JMP, the step is `IADD/ISUB Ri, imm` right before it).
2. The body contains a CALL, a computed jump, a data directive, or an
   instruction with a volatile hint inside a group.
3. Ri is written in `[L.start, L.jmp)` by anything but `L.step`, or BP is
   written in the loop (frame form).
4. A label inside the loop is referenced from outside it (an entry that skips
   the preheader).
5. The loop is cold in the profile (`loop_heat() < 0`).

Refuse a group (the loop may still qualify through others) when

6. `Ru` of a scaled group is not dead after the group
   (`reg_dead_from(after, Ru)`): the group no longer computes it.
7. K is 0, or `K * step` does not fit an integer literal (`fits_int_literal`).
8. Rt, Ru or Rb is Ri, SP or BP; Rb is written in the loop.

## The pointer register

Use the rules of the promote passes, by exporting `frame_info()`,
`frame_save_reg()` and a register picker from `promote.c` through
`inc/promote.h` (thin wrappers, no copy of the logic):

* Rp is one of R1..R10, not mentioned anywhere in `[L.start, L.jmp]`, and proven
  dead at the preheader (`reg_dead_from(L.start, Rp)`). That proof covers every
  exit of the loop: a register dead before the loop on all paths may be
  overwritten.
* Caller-save ABI (`abi_caller_save()`): any such register.
* Callee-save ABI: a register the function already saves, else a fresh
  `PUSH/POP` of R4..R10 (never R2/R3) when `fi.can_save`; otherwise refuse.
  `testing/lint` must stay clean.
* At most two pointers per loop, one per distinct (BASE text, K), the one with
  the most groups first.

## Profit

Per iteration a group saves its length (folded) or its length minus one; each
pointer costs one step instruction. Take a pointer when `saved - 1 >= 1`. The
preheader (2 or 3 instructions) and a fresh save (2 per call) are paid once and
are not counted, as in promote-loops.

## Emitted code

* Preheader, linked immediately before the `start` label:
  K = 1: `MOV Rp, BASE` (or `LEA Rp, [BP+off]`) / `IADD Rp, Ri`;
  K != 1: `MOV Rp, Ri` / `IMUL Rp, K` / `IADD Rp, BASE`.
* Pointer step, linked immediately after `L.step`:
  `IADD Rp, K*c` for `IADD Ri, c`, `ISUB Rp, K*c` for `ISUB Ri, c`.

Apply each loop as soon as it is accepted (as promote does), inside one
liveness session; no label is added or removed. One `--trigger-max` slot per
rewritten loop. `-d` comments, `-v` tally, source markers and `;@var` records as
in `loop_index.c`.

## Not in this version

* Removing the counter when the pointer makes it redundant (needs an end
  pointer for a runtime bound).
* Loops with a CALL in the body (Rp would have to survive the callee).
* A scaled frame base.

## Acceptance

* `testing/differential`: `programs/*.c`, 100 `gen.py` seeds in default and
  `GEN_LIVE=1` modes, `asm/*.asm`: 0 problems at `-O2`, `-O3`, STRICT=1.
* A new `testing/loop-strength/run.sh`: each form fires, each refusal 1-8 does
  not, optimized and unoptimized programs agree in v32sim.
* `testing/lint`, `testing/srcmarks`, `testing/vars` and the loop suites pass.
* `bench.py` before/after on array loops: cycles drop, and no program in the
  differential corpus gets slower.
