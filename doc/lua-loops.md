# Lua loops: what to match, and what not to

Status: decision and pass specifications, from the loop catalog of real v32lua
output (353 loops in 74 programs; 648 in the extended set of 232). L1 and L2 are
implemented (`peephole-copies`, `peephole-jumps`), L3 as `lua-loop-var`
(`src/peephole/lua_checks.c`; v32lua writes `var=`), L4 as part of `promote-loops`
(`-fno-promote-loops-calls` turns it off; `src/promote.c`).

Implementation status (2026-11-13): L1, L2, L3 and L4 are implemented.
* L1, float immediates in `peephole-copies`, and L2, the `JMP` over labels in `peephole-jumps`: with the compiler-ABI
  rule of `liveness.c` (a function reached through `__builtin_exec` keeps the ABI registers at its `RET`, so the
  promote passes find free registers), 40 random Lua programs at `-O2` take 32,668,581 cycles against 33,247,006 with
  the original optimizer (-1.7%).
* L3, `lua-loop-var`: 32,473,596 cycles (a further -0.6%; measured with the patched v32lua that writes `var=`).
  It rewrites the loops of the directed programs of `testing/lua-loop-var`; it skips a loop in a function whose range
  is cut by a nested function definition (24 of 255 loops of the corpus), variable-step headers and functions with
  other computed jumps.
* L4, `promote-loops` across hot calls with `clobbers=` lists: 32,312,320 cycles (a further -0.5%). On the 74-program
  corpus it reaches 12 of 152 numeric loops with hot calls: 72 are blocked by calls through `__builtin_exec`, 30 by
  a callee without a list, 19 by every register being excluded and 19 by liveness not proving a register free across
  `__builtin_table_set` (the categories overlap).

## What the data says

* One family dominates: the numeric `for` with a literal step, 244 of 353
  loops. `while` is 44, `pairs` 38, the rest 27.
* 75% of numeric loops have a `CALL` on the hot path (a table access that is
  not inlined, a user function). The counter then stays in a frame slot, and the
  loop spends a median of 11 instructions per iteration on control.
* 105 loops have constant bounds; 3 of them have no `CALL` in the body (9 if
  cold slow paths are ignored).
* `loop-invert` already works on Lua: 239 of 255 numeric loops are inverted
  at `-O3`. The ISSUES claim that Lua gains from `-O1`/`-O2` only is wrong.

## Decision

**No Lua counterpart of `loop_parse_counted()` for unrolling.** Full unrolling
would reach 3 to 9 loops of 353, partial unrolling the same handful, and
`loop-index` / `loop-strength` have nothing to fold: Lua indexes tables
through runtime routines, not `base + i`. A recognizer that tolerates the
shape's noise (cold `_slow_N` blocks in the loop text, threaded exits, inline
closures) is a large, risky piece of code for that return.

**Remove the per-iteration control overhead instead.** It is the same few
instructions in nearly every loop, and each removal is a local, checkable
rewrite. Four passes, in order of value over risk:

| # | pass | reaches | saves per iteration |
|---|---|---|---|
| L1 | float immediates in `peephole-copies` | 191 loops (step), 63 (limit), and straight-line code | 1 to 2 |
| L2 | cleanup the first pass exposes: jump to the next instruction across labels; dead copies in the loop header | 12 + 10 loops proven, more after L1 | 1 to 2 |
| L3 | `lua-loop-var`: the loop variable read from the counter slot, the per-iteration copy deleted | 190 loops | 2 |
| L4 | `promote-loops` across calls whose `;@fn clobbers=` leaves a register alone | 59 loops | 3 |

Variable-step loops (11) are not matched: 7 of them exist only because v32lua
compiles the literal `-1` as a call to `__builtin_unm`. That is fixed in the
compiler (a patch for v32lua), after which they are ordinary constant-step
loops.

## L1. Float immediates (extend `propagate_imm` in `peephole/dead_regs.c`)

    MOV R2, 1.0 / FADD R5, R2      ->  FADD R5, 1.0
    MOV R2, 10.0 / FGT R1, R2      ->  FGT R1, 10.0

Today the immediate propagation refuses floats. Allow a float immediate (a
float literal, or the hexadecimal word v32lua writes for one) when every use is
the source operand of FADD, FSUB, FMUL, FDIV, FMIN, FMAX, FEQ, FNE, FGT, FGE,
FLT, FLE or a plain `MOV Rd, Rt`. All other conditions stay as they are
(straight-line window, at most two uses, the register dead after the last
one). Before enabling it, confirm with v32as that each of those mnemonics
assembles with an immediate source and that the encoded word equals the one
`MOV` would have put in the register (assemble both forms, compare the VBIN
words). An integer immediate used by a float operation, or a float immediate
by an integer one, stays refused.

This also undoes the regression the catalog found in `peephole-lua-checks`,
which re-materialises `MOV R2, 1.0` in 32 loops.

## L2. Cleanup

* `peephole-jumps`: a `JMP L` (and `JT`/`JF Rn, L`, which then only need their
  test removed if Rn is dead: leave those) is deleted when only labels and
  comments lie between it and `L:`. Today one label is seen through, two are
  not (`JMP __f_end_if_2` / `__f_else_2:` / `__f_end_if_2:`).
* The dead header copies (`MOV R3, R5`, `MOV R2, R6` in `sum.lua`) are plain
  dead register writes that `peephole-dead-regs` should already remove. Find out
  why it does not before writing anything: the likely cause is that R2 and R3
  are live at every `RET` in `-L lua` (v32lua returns values 2 and 3 there), and
  since the liveness fix a function reached through `__builtin_exec` has every
  register live at its `RET`. If that is the cause, the remedy is knowledge of
  how many values a function returns, not a loop pass: report what v32lua's
  return protocol offers (a count register, a `;@fn` attribute that could be
  added) and stop there for a decision.

## L3. `lua-loop-var` (new pass, `-L lua`, `-O2`)

    start:  MOV R1, [BP-c] / MOV R3, R1 / <limit> / FGT R1, R2 / JT R1, end
            MOV [BP-v], R3              ; the copy: deleted, with MOV R3, R1 if R3 is then dead
            ... MOV Rn, [BP-v] ...      ; becomes MOV Rn, [BP-c]

Legal when, for a loop with a `;@loop kind=for counter=c ...` line:

1. the line carries `var=v` (a v32lua addition: the slot of the visible
   variable, promised to be written only by this copy, not captured by a
   closure, and unused after the loop). Without `var=` the pass does nothing;
2. between the start label and the back edge, `[BP-v]` is written exactly once,
   by `MOV [BP-v], Rk` that directly follows the exit `JT` (comments between),
   and `[BP-c]` is written exactly once, by the step, which is the last store
   before the back `JMP`;
3. no path from the copy to a read of `[BP-v]` passes the step: the step's
   store is followed only by the back `JMP` (so `[BP-c]` equals the copied value
   wherever the body reads `[BP-v]`);
4. no label inside the loop is referenced from outside it, the function's frame
   address does not escape (`promote_bp_value_escapes`), and no instruction of
   the loop has a volatile hint;
5. `[BP-v]` is not read between the end label and the function's end before
   being written (walk the text; give up at the first thing not understood).

The `;@var` record of the variable is re-pointed at the counter slot for the
loop's range. One trigger slot per loop.

## L4. `promote-loops` across calls that keep a register

`promote-loops` ends its CALL-free region at every hot `CALL`. Let a loop
with calls qualify when every `CALL` in it is direct, its callee has a
`;@fn F clobbers=` line, and some register in R1..R10 is in none of those
clobber sets, is not mentioned in the loop, and is dead at the preheader. The
callee's clobber set must be closed over what it calls (confirm what v32lua's
`clobbers=` means before relying on it: the routine itself only, or
everything it can reach). Calls through `__builtin_exec` (user functions) have
no hint and keep blocking the loop.

As implemented: v32lua's `clobbers=` is closed over everything the routine can reach
(`tools/check_clobbers.py`: a CALL adds its callee's set, an indirect call or jump or an
un-analysable callee leaves the routine without a list) and leaves out registers the routine
saves and restores. The loop's own registers, the union of its callees' lists and, in a
function with its own list, the registers that list promises to keep are excluded; the rest
of the choice is `pick_register` (`reg_dead_from` at the preheader). `V32OPT_PROMOTE_CALLS_CHECK=1`
walks each callee for a write of the chosen register outside a save (none on the corpus).
What limits it most is liveness, not the hints: through `__builtin_table_set`'s metamethod path
(`__table_call_handler` saves R1-R13 around `CALL __builtin_exec`, which liveness cannot take for
a save before an unknown callee, and a recursion past the call-depth limit) R6-R8 come out live
in `fill.lua`; 19 of the 50 numeric loops whose CALLs all have lists stop there (19 more have no
register left at all, 12 are promoted).

## Order and acceptance

L1, then L2, then L3 once v32lua emits `var=`, then L4. Each pass is validated
on its own before the next starts: `testing/lua-audit/run.sh` (every global of
every audit program equals the unoptimized run), 40 `genlua.py` seeds in each
mode at `-O1`/`-O2`/`-O3`, `testing/lua-checks`, and the catalog's own counter
(`/home/claude/work-lua/loopcat.py`) showing the control instructions per
iteration before and after.
