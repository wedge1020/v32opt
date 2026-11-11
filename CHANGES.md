# v32opt changes

## 20261109-dev

* Post-unroll cleanup: `omit-frame-pointers` now first shrinks the frame of a function whose locals are in registers
  (or gone after full unrolling): a `PUSH Rn` / `POP Rn` save that nothing uses any more is removed (not when `Rn`
  is mentioned, when the function has a `CALL`, or when an `[SP+k]` or `[BP-k]` operand reaches the saved word), and
  so is an `ISUB SP,N` with no `[BP-k]` left (not while a call's arguments are staged into it with `MOV [SP+k], x`);
  after that the frame itself goes when nothing else uses `BP`. It also sees through labels nothing references (the copies'
  labels of an unrolled loop) and keeps the frame of a body that jumps out of the function. One `--trigger-max` slot
  per removed save and per removed `ISUB`. New test `testing/frame-cleanup`.
* New pass `peephole-repeat-imm` (`-O3`, `-fpeephole-repeat-imm`): a `MOV Rd, imm` is deleted when `Rd` provably
  holds `imm` already, across labels nothing references. Facts die at a write, a `CALL`, `MOVS`/`SETS`/`CMPS`, a
  referenced label, a directive and a volatile instruction. New test `testing/peephole-repeat-imm`.
* `peephole-copies` propagates an immediate: `MOV Rt, imm` followed by one or two uses of `Rt` (`OP Rd, Rt`,
  `MOV Rd, [Rt]`, `MOV [Rt], Rs`) becomes `OP Rd, imm` / `MOV Rd, [imm]` / `MOV [imm], Rs` when `Rt` dies there.
  Integer literals and plain symbols only, not across a label, jump or `CALL`. New test `testing/peephole-copies-imm`.
* `constant-folding`: a block entered by a `CALL`, through an address that is taken (`MOV R0, L`, `pointer L`, any
  mention other than as a jump target), as the first block of the program, or with no predecessor starts with every
  register unknown. A constant set before `f1:` used to be folded into `f1` although `f1` is also called with other
  register values. New test `testing/constant-folding-entries`.
* `promote-regs`, `promote-leaf` and `promote-loops` no longer add a register save where an `[SP+k]` operand can reach
  the saved word. v32cc's single `ISUB SP,N` for locals and outgoing arguments, read as callee-save, made the fresh
  `PUSH` take the slot of an argument stored with `MOV [SP+k], x`, and the `POP` handed the argument to the caller.
  The stock compiler's separate `ISUB` for the argument area is not affected. New test `testing/promote-save`.
* New pass `loop-strength` (`-O3`, `-floop-strength`): pointer strength reduction. The address `base + K*i` recomputed
  in every iteration of a counted loop (`MOV Rt, base / IADD Rt, Ri`, the scaled form with `IMUL`/`SHL`, `LEA Rt,
  [BP+off]`, `arr[i + c]`) becomes a pointer register set before the loop and stepped with the counter. It runs after
  `loop-index` and before `loop-unroll-partial` and `loop-invert`, which still see the loop. Registers are chosen as
  in `promote-loops`. Design in `doc/pointer-strength-reduction.md`. New test `testing/loop-strength`.
* `loop-index` honours `--trigger-max` (one slot per loop) and marks each rewrite with a `-d` comment.
* Liveness soundness (`src/liveness.c`, audit in `doc/liveness-audit.md`): `reg_dead_from()` answered "dead" for
  registers that were live in functions reached by a tail jump or a shared epilogue, called only indirectly, that
  read a caller's register between a `PUSH` and its `POP`, or that return a result in a register other than the ABI's.
  Tests: `testing/peephole-dead-regs`, `testing/differential/asm/live_*.asm`, the `GEN_LIVE`/`GENLUA_LIVE`/
  `GENREG_LIVE` generators and `testing/differential/fuzz.sh`.
* Not done: shrinking an `ISUB SP,N` by part; removing the counter that a strength-reduced pointer makes redundant;
  a pointer register across a `CALL`. See `ISSUES`.

* Dead variables: a `;@var loc=Rn` whose register is never written in the final region of the function becomes
  `optimized-out` (so v32sim shows `<optimized out>` instead of a stale register). New test `testing/vars/dead`.

## 20261105-dev

* Variable locations after register promotion (V32DBG): `;@var loc=BP+n` is rewritten to the register in
  the range it is promoted for (leaf, register and loop promotion), and the move is recorded with the new
  `;@home FROM TO` marker (understood by `v32as` 20261105-dev).
* Inlining no longer copies the callee's `;@var` / `;@scope` lines into the caller.
* `--check-vars`: verify the records afterwards (exit status 3).
* `testing/vars/` with `run.sh`; `testing/srcmarks/run.sh` skips it.
* Not done: marking variables with only dead stores `optimized-out`.
* `make version VERSION=20261105-dev` stamped; `install` and `sysinstall` unchanged (both present).
