# Compiler hints

`v32opt` only sees assembly, so it has to *guess* things the compiler knew:
which locals have their address taken, whether a function may be inlined,
which memory accesses are `volatile`, which calling convention the code
follows. A compiler can say so in `;@` comment lines. This document is the
format; `v32cc` is the first compiler to write it.

Design rules:

* **Comments only.** A hint is an ordinary assembler comment, so a file that
  carries hints assembles to exactly the same cartridge. Nothing in the
  toolchain but `v32opt` reads them.
* **Hints only ever lift a conservative assumption.** Without a hint
  `v32opt` does what it always did. A missing, unknown, or malformed hint
  is therefore always safe; a *wrong* hint is a compiler bug.
* **Forward compatible.** Unknown attributes and unknown `;@` keywords are
  ignored. The file declares a format version (`;@hints 1`); a version
  `v32opt` does not know makes it ignore the whole set (with a warning).
* **Cheap to produce.** One header line, one line per function, and a suffix
  on the few instructions that need one.

## The four forms

```
;@hints 1 abi=caller-save                  once, anywhere (the top by convention)
;@fn __function_f noinline escapes=-5..-3  once per function, anywhere in the file
;@loop hot unroll=4                        before a loop's start label
        mov R0, [global_flag]   ;@v        a suffix on one instruction
```

`;@hints`, `;@fn` and `;@loop` start the line (leading blanks allowed). `;@v` is the
first `;` of the instruction's line.

The first three kinds of fact differ from the last two in what they are. `abi=`,
`escapes=`, `leaf`, `noreturn` and `;@v` are things the compiler *knows* about the code;
a wrong one is a compiler bug. `noinline` and the *requests* below (`inline`,
`alwaysinline`, `hot`, `cold`, and everything on `;@loop`) are things the programmer
*asked for* in the source; `v32opt` takes them as a push on its heuristics (a size
limit, how much a loop may grow), never as a reason to skip a check of whether a
transformation is correct, so they cannot make a program wrong.

### `;@hints <version> [abi=caller-save|callee-save]`

| Part | Meaning |
| --- | --- |
| `1` | the format version (the only one so far) |
| `abi=caller-save` | **The code follows v32cc's convention:** no register survives a `CALL`; a caller saves what it still needs; a callee may overwrite `R0`..`R10` without saving; the result is in `R0`. `v32opt` then promotes stack slots into free registers **without** adding a `PUSH`/`POP` to the function (the stock compiler's callee-save rule needs them, v32cc's does not). |
| `abi=callee-save` | the stock compiler's convention (also what `v32opt` assumes when there is no hint) |

`--abi=auto|stock|caller-save` on the command line overrides the header (`-L lua`
is always caller-save). *The compiler promises that every function in the
file, including hand-written assembly it links in, follows the convention.*

### `;@fn <label> [attribute ...]`

`<label>` is the function's label exactly as written in the assembly
(`__function_f`, without the colon). Attributes, in any order:

A `;@fn` line also settles one naming question: `__function_X_return:` is taken as the internal
return label of function `X` (and not a function of its own) unless a `;@fn __function_X_return`
line names it, or no `__function_X:` precedes it. A real function called `X_return` therefore
needs no line unless a function `X` also exists.

| Attribute | Meaning for `v32opt` | The compiler promises |
| --- | --- | --- |
| `noinline` | never inlines this function (`-finline` skips it) | the source said `noinline`, or the function must stay a real call (its address is compared, it is a `used` entry point, it must show up in a profile ...) |
| `escapes=none` | no word of the frame has its address taken | nothing in the function forms a pointer into its frame (no `LEA R, [BP±n]`, no `MOV R, BP`) |
| `escapes=<list>` | only the listed words can be reached through a pointer; every other local is promoted as if the function had no pointers at all | see below |
| `escapes=all` | any word may be reached through a pointer (the default behaviour; useful to say "I checked") | |
| `inline` | `-finline` allows a body of up to twice `-finline-max` instructions (at most `MAX_BODY_INS`, 32) | the source said `inline` (a soft request) |
| `alwaysinline` | `-finline` takes the body whatever its length (up to 32), even with `-finline-max=0`, even if the function is also `cold` | the source said `always_inline` |
| `hot` | like `inline` for the inliner; every loop in the function counts as hot (below) | the source said `hot` |
| `cold` | never inlined (unless `alwaysinline`); every loop in the function counts as cold (below) | the source said `cold` |
| `noreturn`, `leaf`, `naked` | recorded, not used yet | |

`noinline` beats `inline`, `alwaysinline` and `hot`. A function that is both `hot` and
`cold` is `cold` (v32cc writes only `cold`). The inliner still declines a body it
cannot copy (a frame, a jump, a call), whatever is asked.

Without an `escapes=` attribute `v32opt` keeps every local of a function that
contains *any* `LEA R, [BP-n]` in memory.

**`escapes=<list>`** is a comma-separated list of BP-relative word offsets,
each `N` or `A..B` (inclusive), written as in the assembly: `-3`, `-5..-3,-1`,
`+2` (a parameter whose address is taken; `v32opt` never promotes parameters, so
this is informational). The rule for the compiler: **a frame word is listed if
anything the function does could read or write it through a pointer** — it is the
operand of an `LEA`, a member of an object (array, struct, union) of which any
part is, or the whole object is passed or copied by address. An object of
several words lists all of its words. Words not listed are only ever named by
`[BP±n]` operands.

`v32opt` checks what it can: a `LEA` of a word that is not listed, a copy of
`BP` or `SP` into another register, or anything else that can form a frame
address makes it ignore the attribute for that function and behave as if it
were absent.

### `;@loop [attribute ...]`

Before a loop's **start label** (the label the back edge jumps to). It applies to the
next label line, comments and blank lines between do not matter, and is dropped if
something else follows. The loop passes find the loops by their shape; this line only
says what the programmer wants done with the one that has it.

| Attribute | Meaning |
|---|---|
| `hot` | the loop counts as hot, as if a profile had said so: `loop-unroll` and `loop-unroll-partial` allow twice the growth, partial unrolling uses 8 copies unless told otherwise |
| `cold` | the loop counts as cold: `loop-unroll`, `loop-unroll-partial` and `loop-invert` leave it alone |
| `unroll=N` | (N >= 2) `loop-unroll` unrolls a constant-count loop **completely** when it runs at most N times, within `LOOP_UNROLL_HINT_MAX_SIZE` (1024) instructions, whatever `-floop-unroll-max-trips` and `-floop-unroll-max-size` say; `loop-unroll-partial` makes N copies per round (N at most 16, halved while the main loop would exceed 256 instructions) and accepts a body four times the usual size |
| `unroll=full` | complete unrolling up to 256 iterations (1024 instructions); a loop whose count is not constant gets 8 copies |
| `nounroll` | neither pass touches the loop (`unroll=1` means the same) |

A measured profile (`--profile`) that covers the loop outranks `hot` and `cold` on the
loop and on its function; where the profile has nothing to say, the hints decide. An
explicit `unroll` beats `cold` (of the loop or of the function). The soundness
conditions of the passes (no call in the body, the counter written only by the step,
labels used only inside, ...) are still checked: a loop that cannot be unrolled stays as
it is.

### `;@v` on an instruction

Marks an instruction as a **volatile access**. Put it on the `MOV` (or the
read-modify-write instruction) that performs the load or the store of a
`volatile` object, and on nothing else. `v32opt` then

* never deletes it, merges it with another access, or forwards a value across
  it (it is a barrier for every windowed pass);
* never keeps a volatile **stack** slot in a register (a `volatile` local
  stays in memory even in a loop);
* copies it with the instruction when a pass copies code (loop unrolling,
  inlining), and rewrites it in place only to change addresses and registers,
  never to turn it into something that is not an access.

A volatile access survives into the output carrying its `;@v` again, and
before writing the output `v32opt` warns on stderr if a marked instruction
stopped being a memory access (that would be a `v32opt` bug).

## What `v32opt` does with them

| Hint | Effect |
| --- | --- |
| `abi=caller-save` | `promote-leaf/-regs/-loops` no longer add a `PUSH`/`POP` pair to the function for each register they use, and the registers `R2`..`R10` all count as free when liveness shows they are. Saves 2 instructions per call per register, and lets small slots be promoted that were not worth the pair. |
| `noinline` | the inliner leaves the function and its call sites alone. |
| `escapes=` | the promote passes work per slot instead of per function. |
| `;@v` | see above. |
| `inline`, `alwaysinline`, `hot`, `cold` on `;@fn` | the inliner's size limit for that function (`inline.c`); `hot` / `cold` also reach the function's loops through `loop_heat()` |
| `;@loop` | `loop-unroll`, `loop-unroll-partial` (`unroll=`, `nounroll`, `hot`, `cold`), `loop-invert` (`cold`) |

`-v` prints `Compiler hints found (calling convention: ...)` when a file has hints.

## Reserved for later

These are *not* read yet; compilers may write them and old `v32opt`s ignore them.

* `clobbers=R0-R3` on `;@fn`: the registers the function (and everything it
  calls) may write. It would let a caller keep values in the other registers
  across the call, and let `v32opt` drop the register saves a caller emits around
  calls.
* More attributes on `;@loop` that are facts rather than requests: the counter slot, the
  step, the bound or trip count, whether the body contains a call and how many exits it
  has, so the loop passes need not rediscover them from the shape of the code.
* `noreturn` / `pure` to let calls to such functions be treated as barriers or
  as removable.

## Testing

* `testing/hints/run.sh` — volatile accesses survive every optimization level.
* `testing/hints/run_requests.sh` — the requests: which functions `-finline` inlines under
  `inline` / `alwaysinline` / `hot` / `cold` (and with `-finline-max=0`), how many copies of
  a loop body each `;@loop` leaves (`requests_inline.c` and `requests_loops.c` made the `.asm`
  files with v32cc), that the hints are inert without `-finline`, and that a profile outranks
  them. No assembler or simulator needed.
* v32cc's `make optexec` runs a corpus of self-checking programs compiled with hints through
  `v32opt` at several levels, assembled with the stock assembler and executed in `vrun`
  (`tests/c/t_hints_opt.c` is made for it).
* `testing/differential/` with `HINTS=1` — the same random programs as always,
  but `testing/differential/hinter.py` first adds the hints v32cc would write
  (the calling convention, and a conservative `escapes=` computed from the
  assembly), so the hint-driven passes are compared with the unoptimized program
  in the simulator.
