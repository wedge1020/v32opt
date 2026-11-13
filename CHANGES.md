# v32opt changes

## 20261113-dev

* `;@fn F returns=N` (written by v32lua, `-L lua`): when every function a `RET` can belong to has `returns=0` or
  `returns=1`, R2 and R3 are no longer live at that `RET` (v32lua's callers read them only after checking `[RET_COUNT]`),
  so the dead copies of a loop's control value into R3 and the like can go. A function whose class has an entry
  without the hint (a runtime routine, a function that forwards `f()` or `...`) keeps R2/R3 live as before. New test
  `testing/lua-returns` (unit shape with teeth, and a v32lua program with one-, two- and three-value functions,
  forwarding and `...`).
* The `;@hints` header's `lang=lua` (v32lua output) now selects `-L lua` when `-L` is not given. The `Make.sh`/`Make.bat` v32lua
  writes run plain `v32opt -O3`, which was C mode on Lua output: no `peephole-lua-checks`, `lua-loop-var` or the R2/R3
  live-out rule, and more instructions left (nostalgick: 51,033 against 50,528). An explicit `-L c` still wins.
  Test: `testing/lua-returns`.
* Three unit fixtures that had gone stale after the liveness and frame-cleanup work now test what they meant to: `omit-frame-pointers`
  (the `ISUB SP,16` of `sp_modified` is used, so it is not an orphan frame), `peephole-copies` (`pointer_base` has a caller, so R2 is
  dead after it) and `peephole-dead-regs` (the register a callee saves and restores is read after the call). The optimizer's output
  was right in all three.
* `peephole-zero-test` keeps the trailing source comment of the jump it rewrites (test `testing/zero-test-comment`).
* `make -C testing` skips with one line when the stock `assemble` and `packrom` are not in PATH, instead of failing in
  every sub-directory.
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
* Function boundaries on v32lua output (`src/promote.c`): a function now ends at the next entry (the scope
  initializer included), a `__literal_` label, its own `__<stem>_skip` label or a `CALL` target, and a range cut short
  while its frame goes on is left alone by promotion, `peephole-lua-checks`, frame cleanup and `loop-strength`. Two
  numeric `for` loops of `main()` and the top-level code used the same `[BP-n]` slots and were taken for one function:
  `peephole-lua-checks` deleted the top-level loop's step and counter stores and the loop never ended. New test
  `testing/lua-function-bounds`.
* `promote-*` no longer count the save and reload of a promoted register around a cold `CALL` as a use of the slot:
  every pass found something new to do and `v32opt` never terminated on some Lua loops at `-O2 -L lua`
  (`testing/lua-function-bounds`).
* Frame changes are refused for a function whose inner labels are reached from elsewhere (another function jumping
  into its epilogue): `omit-frame-pointers` frame cleanup no longer removes the orphan save of a shared tail, no
  fresh `promote-*` save is added (its `POP` would pop a word the jumper never pushed), and a `_return` label jumped
  to from another function keeps the frame. Tests `testing/frame-cleanup` (`shared_tail.asm`, `frame_holes.asm`).
* `peephole-copies`: the immediate propagation takes float literals into `FADD FSUB FMUL FDIV FMIN FMAX FEQ FNE FGT
  FGE FLT FLE` (`MOV R1, 1.5 / FADD R0, R1` becomes `FADD R0, 1.5`). A hex word is respelled as the decimal float that
  reads back to the same bits; an integer literal that has no such spelling (`MOV R1, 5`), a NaN, an infinity, a
  denormal and a symbol are refused ("L1" of `doc/lua-loops.md`). Test `testing/peephole-copies-imm`.
* `peephole-jumps`: `JMP L` is also deleted when only labels, comments and blank lines lie between it and `L` (any
  number of labels; a directive ends the run); `JT`/`JF` keep the one-label form ("L2"). Its unreachable-code removal
  no longer deletes the compiler's `;@scope }`, `;@var`, `;@home`, `;@loop`, `;@fn` and `;@cold` lines in the range
  (`--check-vars` reported an unclosed scope). Test `testing/peephole-jumps-labels`.
* Liveness: the tables of a session are reused by the next pass when the program is unchanged (an edit counter,
  guarded by a fingerprint of the program), and the registers outside the compiler's ABI are free at the `RET` of a
  function that is entered from an unknown place when a compiler vouches for the convention (`-L lua`, or a
  `;@hints` header) and every unknown way in is a `__function_` entry. In v32lua output every function is reached
  through `__builtin_exec`, so before this no register was ever free for the promote passes. A `;@hints` header on
  hand-written code that does not follow the convention makes the optimizer wrong. Test `testing/liveness-trust`.
* `dce` keeps a function that is reached by a jump to an inner label (a shared epilogue) or only fallen into from the
  function above it. Test `testing/dce-reach`.
* `MOVS`, `SETS` and `CMPS` read and write `CR`/`SR`/`DR` (`R11`-`R13`), and `CR`/`SR`/`DR` are those registers:
  `peephole-movs` no longer deletes the repeated `MOV R12, 300` after a `MOVS`, `peephole-dead-stores` keeps the
  `MOV R12, 7` before a `SETS`, and constant folding forgets `R11`-`R13` at a string instruction. Test
  `testing/string-ops`.
* `;@v` on an instruction without a memory operand is reported once at parse time (the hint marks loads and stores
  only) and no longer produces a false "a volatile access was optimized away" report. Test `testing/hints`.
* New pass `lua-loop-var` (`-O2`, `-L lua`, `-flua-loop-var`): with `var=` on the `;@loop kind=for` line (written by
  v32lua) the reads of the loop variable's word read the counter instead and the per-iteration copy is deleted
  ("L3"). Refused unless the header shape, the step, every use of the word, the ways into the loop and the ways out all
  check ([doc/hints.md](doc/hints.md)). Test `testing/lua-loop-var`.
* `promote-loops` keeps a register across the hot `CALL`s of a loop when every callee has a `;@fn F clobbers=` list
  ("L4"; v32lua writes them); `-fno-promote-loops-calls` turns the extension off (on wherever `promote-loops` is).
  Calls through `__builtin_exec`, through a register, or to a routine without a list still end the loop.
  Test `testing/promote-loops-calls`.
* Measured on 40 random Lua programs at `-O2`: 33,247,006 cycles with the original optimizer, 32,668,581 with the
  float immediates, the jump over labels and the compiler-ABI rule, 32,473,596 with `lua-loop-var` as well and
  32,312,320 with `promote-loops` across calls (patched v32lua).
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
