# v32opt changes

## 20261124-dev

Two code-generation fixes, found by running v32lua 20261124-dev's `testing/` suite on v32sim's oracle (both bugs were
older than the `--gc` work; v32lua output from before it failed the same way):

* **Fix (liveness): a register a callee saves around a call and reads afterwards is an input.** In a callee, a save
  `PUSH Rn ... POP Rn` made the walk stop at the POP, so `PUSH R1 / CALL f / POP R1 / PUSH R1` (R1 passed on after
  the save) was not seen as reading the caller's R1, and the caller's `MOV R1, ...` was deleted as dead
  (peephole-dead-regs, and everything else that asks `reg_dead_from()`). v32lua's `__table_index_miss` has this
  shape: a metatable's `__index` was looked up with a garbage key (`special/13_metatables`,
  `tables/19_index_computed_key`, `tables/20_index_known_number_key` failed at `-O1` and up, `gc/03_arena_check_clean`
  too). The walk now goes on after the POP, with the register holding the caller's value again; it reaches the RET
  untouched only if it really does.
* **Fix (promote-loops / promote): a POP into a register is a write of it unless it gives back what its PUSH saved.**
  The region scan skipped every PUSH and POP as save/restore bookkeeping, but peephole-copies turns
  `MOV R2, R0 / PUSH R2 ... POP R2` into `PUSH R0 ... POP R2`, and that POP overwrites R2. A loop counter was kept in
  R2 across such a body (`special/26_pico8_stat` at `-O2` and up; it passed with `-fno-peephole-lua-checks` only
  because that left another use of R2 in the loop). The scan now follows the region's own stack: a POP that does not
  match its PUSH's register, or pops a word pushed before the region, or comes after an SP change it can't follow,
  marks the register used. On the whole suite this refuses exactly that one promotion (17 -> 16 at `-O3`).
* Not a v32opt bug: `MOV R1, __const_str_err_for_initial / JMP __for_number` losing its MOV. `__runtime_error_for`
  (v32lua `src/runtime/exec.s`) reads R1 as the message after a `CALL __builtin_print`, which overwrites R1 (the X
  coordinate) and does not restore it, so the MOV is dead and the panic screen shows the wrong text with or without
  v32opt (on the oracle, unoptimized: R1 = 0x20002AFB after the MOV, 0x7FC0008C at the print). The fix is in v32lua:
  keep the message in the frame, as the value already is in `[BP-1]`. The three tests that passed with
  `-fno-peephole-dead-regs` were the liveness bug above.
* With both fixes the whole suite gives the same result at `-O1`, `-O2`, `-O3` and `-Os` as without v32opt, test for
  test, in the default heap mode and with `--gc arena --gc-check` (146 of 162 files pass; the other 16 fail the same
  way unoptimized).
* New `;@fn` attribute **`nonzero`**: R0 is never 0 when the call returns (v32lua's 20261124-dev hint update puts it on
  `__malloc` and `__malloc_owned`; the first 20261124-dev build does not have it yet). peephole-lua-checks then drops any zero test of the result, whatever it jumps to. A file without
  the hint keeps the rule below (tests that go to `__oom_handler`, after `__malloc` / `__malloc_owned`, in a file that
  defines `__malloc_owned`).
* New test `testing/lua-regs` (both fixes; 6 cases, all failing on 20261119-dev), part of `make tests`.
  `testing/lua-alloc` gains the `nonzero` cases (18 now) and its fixtures are v32lua 20261124-dev output.

What v32lua's `--gc` work (20261120-dev onwards) changed in its output, and what v32opt does about it:

* `peephole-lua-checks` (`-L lua`, `-O1` and up) deletes the dead out-of-memory test after an allocation. `__malloc`
  never returns 0 any more (it jumps to `__oom_handler` and halts), so `CALL __malloc` / `CALL __malloc_owned` ...
  `MOV Rn, R0 / IEQ Rn, 0 / JT Rn, __oom_handler` becomes `... MOV Rn, 0` with the jump gone (the copy and the `MOV`
  are then dead and peephole-dead-regs takes them), and the `JF R0, __oom_handler` that peephole-zero-test makes of
  the same test is deleted. The result is followed through stack adjusts, PUSH/POP of other registers, copies and
  writes to other registers, up to the first label, branch, CALL or directive. Applied only to a file that defines
  `__malloc_owned` (added in the same v32lua release): an older v32lua's `__malloc` returned 0. `__malloc`'s own
  heap/stack test stays. All 11 tests in the runtime go: 17 instructions per program, 1-3 per allocation at run time.
* New layout guard (`src/layout.c`). Every v32lua cart now checks `__v32lua_code_end` (after all code) and
  `__v32lua_rom_end` (after the string pool, last in the file) at boot. No pass moved code past them or appended
  anything, and now that is checked: the labels, instructions and directives from `__v32lua_code_end` to the end of
  the file are recorded before the passes and compared before the output is written. A difference is an internal
  error: v32opt says what changed, writes no output, and exits with status **4**. Not checked: files without the
  label, and input that already has instructions after it (v32lua never writes that). `V32OPT_LAYOUT_SELFTEST=1`
  appends a `RET` to the end of the program before the comparison, so the test can show the check works.
* Checked, no change needed: the `MOV R0, owner` before `CALL __malloc_owned` in arena mode stays, because liveness
  reads `__malloc_owned`'s body and sees R0 used. The `PUSH Rv / CALL __arena_check_global|store / IADD SP,1` sequences
  stay, with `clobbers=none` honoured. The `;#if` blocks never reach v32opt. `__arena_release` had no `clobbers=` hint
  (v32lua's `tools/check_clobbers.py` only found call targets written as a literal `CALL __name` in the compiler's C,
  and it is called through `emit_runtime_call_intrinsic`), so it was treated as clobbering everything; v32lua's
  20261124-dev hint update gives it `clobbers=R0-R2`, which v32opt reads with no change.
* The man pages now list exit status 3 (`--check-src` / `--check-vars` found a problem; it was missing) and 4.
* New test `testing/lua-alloc` (v32lua output in both GC modes plus a hand-written unit; with teeth: 8 of its first
  16 cases fail on 20261119-dev), part of `make tests`.

## 20261119-dev

* BUG FIX (`peephole-copies`, immediate propagation): a LABEL was pasted into integer ALU operands --
  `MOV R2, __tic80_palette / IADD R1, R2` became `IADD R1, __tic80_palette`, which v32as and the stock assembler
  reject ("IADD second operand must be a register or an integer"; nostalgick's blastic). A label is an operand of
  `MOV` and of a memory address (`[label]`) only; a `%define` with an integer value is still propagated anywhere.
  New test `testing/peephole-copies-label` (with teeth; assembles the output when `assemble` is in PATH).

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
