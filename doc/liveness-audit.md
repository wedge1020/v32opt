# liveness.c soundness audit (2026-11-09)

`reg_dead_from()` must only answer "dead" when that is a proof. The audit found five
ways it answers "dead" for a live register. Each was reproduced as a miscompile with
`-fpeephole-dead-regs` (optimized and unoptimized programs disagree in v32sim).

## Gaps (all confirmed)

| id | gap | minimal shape |
|----|-----|---------------|
| G1 | RET of a function that is only called indirectly (`CALL Rn`, address taken) uses the ABI default; a non-ABI result register written before the RET is deleted. ISSUES claims such functions are "called from anywhere (all registers live at RET)"; the code never did that. | `_pair: mov r0,1 / mov r1,2 / ret` called via `mov r5,_pair / call r5`, caller reads R1 |
| G2 | Function regions run from one direct-CALL target to the next, and a RET is charged to the region it textually sits in. A RET reached by a JMP from another function (tail jump, shared epilogue), or by falling through a label, misses that other function's callers. `written[]` has the same blind spot. | `_a: ... jmp _b` ; `_b: mov r1,5 / ret`; a caller of `_a` reads R1, callers of `_b` do not |
| G3 | Inside a callee, `PUSH Rn` that looks like a save returns PASSTHRU at once. The callee may still read the caller's Rn between the PUSH and the POP (a register argument). Affects callee summaries and `g_in` (summary mode runs at depth 1). | `_f: push r1 / mov r0,r1 / ... / pop r1 / ret`, caller `mov r1,41 / call _f` |
| G4 | compute_outputs() drops a register from the output candidates when the function contains any `PUSH Rn`, without checking that the PUSH is a save. A PUSH that passes Rn as a stack argument hides a real output. | `_g: mov r1,7 / push r1 / call _id / iadd sp,1 / mov r1,9 / ret`, caller reads R1 |
| G5 | `PUSH Rn / CALL f / POP Rn` with the pushed word on top of the stack at the CALL is taken as a save, but it is also how a stack argument is passed: the callee reads the word at `[SP+1]` / `[BP+2]`. | `_f: push r1 / call _id / pop r1 / ret`, `_id: mov r0,[sp+1] / ret` |

## Audited and found sound

* **Visit stamps with shared labels.** One `int` per label; a callee summary walk takes a
  fresh stamp and may overwrite stamps of labels the outer walk already visited. The
  outer walk then re-explores them (costing steps, bounded by LV_MAX_STEPS -> LIVE). A
  walk can never see its own stamp on a label it has not visited, so no path is skipped.
* **Summary cache.** Depth-limited and recursion-limited results are LIVE (conservative)
  and may be cached; budget-exhausted results are not cached. The `g_in` downgrade
  (LIVE -> PASSTHRU when the fixed point says the function never reads the register) is
  sound once G3 is fixed, because every "unknown" in flow() also sets the g_in bit.
* **Stale summaries.** `g_callee`, `g_extra`, `g_in` are computed once per session and not
  refreshed after edits. This is sound under two conditions that the passes holding a
  session must keep (to be checked, see task list): (a) a register write is removed
  from a callee only when reg_dead_from() proved it dead, with sound outputs (G1, G2, G4);
  (b) no pass introduces a read of an incoming register value in a callee.
  Nodes created by a pass have `lv_fn == 0`; after the fix a RET with `lv_fn == 0` is
  all-live, so new nodes are conservative.
* **Label index.** Dangling if a label node is freed during a session. loop-unroll frees
  its labels only after its last query. The other session holders must be checked.

## Remaining limits (document, do not fix)

* A save slot overwritten or read through `[SP+k]` / `[BP-k]` aliasing is not detected.
* CALL/JMP to a numeric address is "unknown" at the call site (LIVE) but cannot make a
  function "address taken".

## Fix design

1. **Entries.** A label is a function entry if it is a direct CALL target, or a
   `__function_*` label not ending in `_return`, or *escaped*: its name appears as an
   operand of any instruction other than CALL/JMP/JT/JF, or in a data directive.
2. **Classes.** Every node belongs to the region of the nearest preceding entry label
   (region 0 before the first). Regions are merged (union-find) when control can pass
   between them without a CALL: a JMP/JT/JF whose label target lies in another region,
   or fall-through into an entry label (the previous instruction is not JMP/RET/HLT).
   `lv_fn` holds 1 + the class representative; `written`, `pushed`, `g_extra` are per
   class; a call site's target is the class of its label.
3. **Open classes.** A class is *open* when it contains region 0, or one of its entries
   is escaped, or none of its entries has a direct CALL site. `ret_live_out()` answers
   true for R0..R10 when `lv_fn == 0` or the class is open.
   *Compiler-ABI exemption* (one static predicate, decided after measuring): an open
   class whose entries are all `__function_*` labels may keep "ABI + g_extra" instead of
   all-live, since compiler-emitted functions follow the compiler's convention for
   indirect callers too.
4. **G3.** At depth > 0, when `PUSH Rn` is a register save: walk on from the next node
   with the same depth and stamp; result = `result | (sub & LV_LIVE) | LV_PASSTHRU`.
5. **G4.** Set `pushed` only for a PUSH for which is_register_save() holds.
6. **G5.** Per class, `stack_args`: some operand is `[BP+k]` with k >= 2, or reads through
   `[SP...]`. In is_register_save(), a CALL met while the saved word is on top of the
   stack (depth == mine) makes it "not a save" if the CALL is indirect, its target is
   unknown, or the target class has `stack_args`.

## Acceptance

* `testing/differential/asm/live_*.asm` agree with their unoptimized run under
  `-fpeephole-dead-regs`, `-O2`, `-O3` (STRICT=1).
* The existing suites still pass, and the cycle/word totals of the differential
  programs do not get worse by more than noise (report the table).

## Since the audit

* **More gaps, found while implementing the fix** (G1-G7 are in commit 1e7f1a3; the rules are in the header comment
  of `src/liveness.c`). The rules that came out of G6, G7 and a deeper form of G5 are: a `PUSH Rn ... POP Rn` is a save only when the pushed word
  cannot be a stack argument, which asks for more than the callee's class (a CALL while the word is within the reach of
  the callee's stack arguments, or to an unknown callee; the reach of a target is that of all the code it runs on its
  frame, jumps followed, `[BP+k]` reading word k-2 from the top and a read through `SP` or a computed `JMP` reaching
  any); writes between a save's `PUSH` and `POP` are outputs only if a jump leaves the range or an entry label lies
  inside it; and which registers a function returns is found per class (a callee that returns a second value in `R1`,
  like v32cc's unsigned divide) and asked at each direct call site.
* **Trust rule** (commit 2f9327c): the compiler-ABI exemption of "Open classes" above applies only where a compiler
  vouches for the convention: `-L lua`, or a `;@hints` header. Hand-written assembly without a header keeps the strict
  rule. Measured on primes (`-O2 -L lua`): promote-loops promotions 76 before the fix, 0 with the strict rule, 70 with
  this one. A header that is false for a function (a second result in `R1` to an indirect caller) makes the optimizer
  delete that write; `testing/liveness-trust` has the program that stays correct without the header.
* **Session reuse** (commits 47034c9, 2f9327c): the begin-time tables are a function of the program alone, so a pass
  that finds the program unchanged since the previous one reuses them; the answers are those of a fresh session. The
  fast test is an edit counter, and a reuse also needs the program's fingerprint to match, so an edit made without
  the counted helpers still gets a rebuild.
