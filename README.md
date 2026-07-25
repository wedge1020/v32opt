# v32opt — Vircon32 Assembly Optimizer

`v32opt`  is  a modular,  multi-pass  assembly  optimizer written  in  C,
specifically  targeting  the  **Vircon32**   fantasy  console.  It  takes
raw   assembly  output   from  a   Vircon32-targeting  compiler   (C  and
lua),  hand-written   assembly,  or  disassembled  Vircon32   CARTs,  and
applies  iterative local,  structural, data-flow,  and register-promotion
optimizations to reduce  code size, see the individual  build chain steps
in action, and improve execution efficiency.

> **Note  on   Vircon32  Architecture:**  On  the   Vircon32  CPU,  **all
> instructions  execute  in  exactly  1   cycle**,  and  there  are  **no
> instruction-reactive  CPU  flags**  (like Carry,  Zero,  or  Overflow).
> Destructive  comparison  instructions  (e.g.,  `IEQ`,  `INE`)  directly
> overwrite  the destination  register with  `1` (true)  or `0`  (false),
> which  conditional jumps  (`JT`,  `JF`) evaluate  directly. While  many
> algebraic  simplifications are  cycle-neutral  due to  the flat  timing
> model, `v32opt` aggressively applies them  to reduce total binary size,
> decrease register  pressure, eliminate memory bottlenecks,  and enforce
> clean, idiomatic assembly.

This project and  this documentation - was built with  the help of AI:

  * Google Gemini,  a mix of  its Thinking (3.6)  mid-tier model and  Pro (3.1)
  * Anthropic Claude Sonnet 5 (medium)
  * Mistral Vibe (Thinking)

---

## Table of Contents

1. [Build Instructions](#build-instructions)
2. [Usage & Optimization Levels](#usage--optimization-levels)
3. [Phase 1: Local Peephole Optimizations (-O1)](#phase-1-local-peephole-optimizations--o1)
4. [Phase 2: Global Data-Flow & Dead Code (-O2)](#phase-2-global-data-flow--dead-code--o2)
5. [Phase 3: Interprocedural Inlining (-O3)](#phase-3-interprocedural-inlining--o3)
6. [Experimental Passes: Memory-to-Register Promotion](#experimental-passes-memory-to-register-promotion)
7. [CFG Visualization](#cfg-visualization)
8. [Safety and Correctness Guardrails](#safety and correctness guardrails)
9. [Diagnostic and Debug Options](#diagnostic and debug options)

---

## Build Instructions

`v32opt` is written in  standard C and is designed to  be easily built on
any platform using a modern C compiler (`gcc`, `clang`, or MSVC).

### Using Make (Linux / macOS / MSYS2)

If your repository includes the standard Makefile, simply run:

```bash
# Build the binary
$ make

# Optionally install to your system PATH (defaults to /usr/local/bin)
$ sudo make install

# Clean build artifacts
$ make clean
```

### Direct Compilation

You can also compile the modular codebase directly using GCC or Clang:

```bash
# Compile all source files with optimization enabled
$ gcc -O3 -Wall -Wextra src/*.c -o v32opt

# Verify  the  build  (running  with  no  arguments  will  display  usage
# information)

$ ./v32opt
```

---

## Integrate into CART build process

The typical sequence  to build a Vircon32 CART consists  of the following
steps (if you're writing in assembly you can start at step 2):

### write/edit source code

Currently Vircon32  provides a C  compiler (considered stable and  is the
primary  language of  development  on the  platform)  via its  `DevTools`
suite.  There  is  also  (in  development) a  third  party  lua  compiler
(`v32lua`).

### compile your source code

Either compiler translates it high-level language code/syntax
into  Vircon32 assembly.  It is  this assembly  you need  before you  can
proceed with optimization with `v32opt`.

```bash
# compile a C program with the Vircon32 C compiler
$ compile -o game.asm game.c

# compile a lua program with the v32lua compiler
$ v32lua -o game.asm game.lua
```

Should you  be writing a Vircon32  program IN assembly language,  you can
proceed straight to the next step (optimize).

If you want to try and  optimize an existing, packed binary Vircon32 CART
(perhaps you  do not  have access  to the  source code  to build  it from
scratch), you can use the `unpack` and `disassemble` commands provided by
the Vircon32 `DevTools` to obtain the disassembled assembly of any CART.

Verify you have that resulting  `game.asm` file (and if you're interested
in noting any  space-savings possible via optimization, take  note of the
overall file size of the assembly file).

### optimize: pass your assembly file through

With an assembly file in hand, pass it through `v32opt` applying whatever
combination of optimizations you desire.

It is recommended  you try multiple runs,  using different optimizations,
to find  the desired "sweet  spot" of optimization (the  best performance
gain/space saving without  it breaking anything). Sometimes  you will get
lucky  and be  able to  apply all  optimizations and  it will  just work.
Other  times you  will  have  to backtrack  and  isolate the  problematic
optimization(s) and exclude them.

To start, try  your assembly file against `-O1`, creating  a new assembly
file `gameOpt.asm` (do NOT overwrite the original):

```bash
# run game.as through v32opt with -O1 optimizations:
$ v32opt game.asm -o gameOpt.asm -O1
```

NOTE: currently argument ordering is  rather inflexible; you MUST specify
the  source  assembly  file  first,   and  follow  it  with  any  desired
command-line arguments.

You can also run `v32opt` with the  `-v` argument, and it will give you a
high-level status report of optimization actions it was able to perform:

```bash
# run game.as through v32opt with -O1 optimizations:
$ v32opt game.asm -o gameOpt.asm -O1 -v
```

Now you  should have `gameOpt.asm`  (or whatever  you chose to  call it).
This is the "optimized" form of `game.asm` as a result of having `v32opt`
work on it.

Compare  file sizes,  space savings  is a  common optimization  gain with
relatively little effort (optimized assembly  file should, in most cases,
be smaller by some amount).

### proceed with assembly

At this point, you can continue on with your Vircon32 CART build process,
by passing  the optimized  assembly code  (in `gameOpt.asm`)  through the
Vircon32  assembler to  get  the resulting  object  file (`game.vbin`  or
`gameOpt.vbin`):

```bash
# assemble gameOpt.asm
$ assemble -o gameOpt.vbin gameOpt.asm
```

If  all   goes  without  issue,   the  optimized  assembly   file  should
assemble  just as  well as  the original  `game.asm` (the  assumption is:
successfully).

### generate CART assets

Generate any textures and sounds to be included in your CART, create your
`game.xml`  CART definition  file  (note that  `v32lua`  by default  will
automatically generate an XML file for you):

```bash
$ png2vircon -o background.vtex background.png
$ png2vircon -o sprites.vtex sprites.png
...
```

NOTE that your XML file likely expects  the `.vbin` to be named after the
original assembly file (`game.asm` ->  `game.vbin`), so either rename any
`gameOpt.vbin` file,  or edit `game.xml` to  instead use `gameOpt.vbin`).
Or if you're interested in benchmarking,  make a copy of the XML, calling
it `gameOpt.xml`, and edit it to  work with your optimized file. That way
you can build both (unoptimized, optimized) and compare results.

### pack the CART

Finally, once you have all the  individual pieces in place (`.vbin` file,
any `.vtex`/`.vsnd` files, and the `.xml` file), you can build the CART:

```bash
# build the CART
$ packrom game.xml
```

At this point,  assuming no errors, you should have  a `game.v32` you can
play in the Vircon32 emulator.

---

## Usage & Optimization Levels

```bash
v32opt <input.asm> [-o output.asm] [options]
```

| Flag | Description | Included Passes |
| --- | --- | --- |
| `-O0` | **No Optimization** | Disables all optimization passes (default). |
| `-O1` | **Local Peephole** | Enables all 12 local 1–3 instruction window peephole optimizations. |
| `-O2` | **Global Analysis** | Enables all `-O1` passes + **Dead Code Elimination (DCE)** & **Global Constant Folding**. |
| `-O3` | **Aggressive** | Enables all `-O2` passes + **Function Inlining**. |
| `-v` | **Verbose Mode** | Displays detailed pass statistics and optimization counts per iteration. |
| `--dot <file>` | **CFG Export** | Exports the Control Flow Graph to a Graphviz `.dot` file for visualization. |

### Individual Pass Control

You can enable or disable specific passes granularly using `-fopt_<name>`
and `-fno_opt_<name>`:

```bash
# Example: Run O2 but disable jump chaining and enable loop register promotion
$ v32opt game.asm -O2 -fno_opt_peephole_jmp_chain -fopt_promote_loops
```

---

## Phase 1: Local Peephole Optimizations (`-O1`)

Phase  1  operates on  a  sliding  window  of instructions,  cleaning  up
redundant compiler  output and  simplifying local  instruction sequences.
The optimizations found in this  category are considered the least likely
to horribly  break the resulting  code, yet  still offer some  modicum of
space or performance improvements.

If  you're just  starting out  with optimization,  give `-O1`  a try  and
continue with your build, comparing both  resulting ASM file size and any
performance behaviours. If  you desire more optimization  (more may bring
risks of broken  assembly or errant runtime behaviour),  you can progress
to the higher level optimizations.

### 1. Adjacent Instruction Pair Elimination (`peephole_pairs`)

Scans  consecutive   pairs  to  remove  redundant   operations,  such  as
Convert Integer  to Boolean (`CIB`) instructions  immediately following a
destructive comparison (`IEQ`/`INE`,  which already leave a  clean `0` or
`1` in the destination register),  double bitwise negations (`BNOT`), and
zero-net-effect stack operations (`PUSH`/`POP`).

```vircon32
; BEFORE                             ; AFTER
IEQ R1, R2                           IEQ R1, R2
CIB R1                               ; (CIB removed: IEQ already outputs 0 or 1)

BNOT R3
BNOT R3                              ; (Double negation cancelled out)

PUSH R4
POP R4                               ; (PUSH/POP pair removed)
```

For  those  that understand  assembly,  you  should  see that  these  are
generally useless  progressions, eating  up CPU  cycles and  not actually
contributing  anything to  your  game. Removing  them  means less  cycles
consumed per frame (and that *could* improve performance).

### 2. Algebraic Simplification (`peephole_algebra`)

Eliminates self-moves (`MOV r, r`) and identity arithmetic (`IADD`/`ISUB`
with  `0`). It  also converts  multiplications by  2 into  self-additions
(`IADD r, r`) for idiomatic clarity.

```vircon32
; BEFORE                             ; AFTER
MOV R1, R1                           ; (Self-move removed)

IADD R2, 0                           ; (Identity addition removed)

IMUL R3, 2                           IADD R3, R3
```

Similar to the pair elimination,  look for obvious math transactions that
don't  result in  any modifications.  They  can be  safely stripped  out,
leading to reduce cycles per frame.

### 3. Store-to-Load Forwarding (`peephole_forwarding`)

When a value is stored from a register to memory and immediately loaded back from that exact memory address into another register, the memory read is replaced with a direct register-to-register move.

```vircon32
; BEFORE                             ; AFTER
MOV [R1+4], R2                       MOV [R1+4], R2
MOV R3, [R1+4]                       MOV R3, R2
```

While this may  not offer any distinct performance boost,  it should save
you 1 word  of space, as the resulting double  registered `MOV` will only
need  1 word  to  store  the instruction,  where  any indirect  reference
requires a second, follow-on word for the immediate value/address.

### 4. Redundant Jump Elimination (`peephole_jumps`)

Removes  unconditional jumps  (`JMP`) that  point directly  to the  label
immediately following the instruction.

```vircon32
; BEFORE                             ; AFTER
JMP loop_continue                    ; (Fall-through jump removed)
loop_continue:                       loop_continue:
```

### 5. Redundant & Mirror Move Elimination (`peephole_movs`)

Removes consecutive duplicate moves (`MOV R1, X; MOV R1, X`) and "mirror"
moves where  two registers swap  values twice without  modification (`MOV
R1, R2; MOV R2, R1`).

```vircon32
; BEFORE                             ; AFTER
MOV R1, 100                          MOV R1, 100
MOV R1, 100                          ; (Duplicate move removed)

MOV R2, R3                           MOV R2, R3
MOV R3, R2                           ; (Mirror move removed)
```

### 6. Immediate Math Combining (`peephole_immediates`)

Combines sequential additions  or subtractions on the  same register with
immediate operands  into a single  combined operation. If  the operations
cancel out to zero, both are eliminated.

```vircon32
; BEFORE                             ; AFTER
IADD R1, 5                           IADD R1, 2
ISUB R1, 3
IADD R2, 10                          ; (Both removed: +10 -10 = 0)
ISUB R2, 10
```

### 7. Strength Reduction (`peephole_reduce`)

Simplifies arithmetic operations  with special constants. Multiplications
by  `0`  become  `MOV  0`,   multiplications  or  divisions  by  `1`  are
eliminated, and multiplications by `2` are converted to addition.

```vircon32
; BEFORE                             ; AFTER
IMUL R1, 0                           MOV R1, 0
IMUL R2, 1                           ; (Identity multiplication removed)
IDIV R3, 1                           ; (Identity division removed)
IMUL R4, 2                           IADD R4, R4
```

On many other  CPUs, the multiplication and division  instructions may be
more "costly"  in terms of cycles  needed to perform the  instruction. On
Vircon32, that  is not an  issue. Still, simplifying operations  can help
with overall readability.

### 8. Shift Optimizations (`peephole_shifts`)

Removes no-op shifts by `0` and  converts left-shifts by `1` (`SHL r, 1`)
into self-additions (`IADD r, r`).

```vircon32
; BEFORE                             ; AFTER
SHL R1, 0                            ; (Shift by 0 removed)

SHL R2, 1                            IADD R2, R2
```

### 9. Dead Store Elimination (`peephole_dead_stores`)

Removes memory  stores that are  immediately overwritten by  a subsequent
store to  the exact same  indirect memory address without  an intervening
read.

```vircon32
; BEFORE                             ; AFTER
MOV [R1+0], R2                       ; (Overwritten store removed)
MOV [R1+0], R3                       MOV [R1+0], R3
```

### 10. Redundant Load Elimination (`peephole_loads`)

When  two  registers sequentially  load  from  the same  indirect  memory
address, the second load is replaced with a direct register move from the
first destination register.

```vircon32
; BEFORE                             ; AFTER
MOV R1, [R2+8]                       MOV R1, [R2+8]
MOV R3, [R2+8]                       MOV R3, R1
```

Again, the `MOV R3,  R1` will end up saving a word as  it doesn't need to
reference any immediate data.

### 11. Immediate Propagation (`peephole_immediate_prop`)

Propagates  constant  immediate values  loaded  via  `MOV` directly  into
immediately following arithmetic instructions  or moves that consume that
register.

```vircon32
; BEFORE                             ; AFTER
MOV R1, 42                           MOV R1, 42
IADD R2, R1                          IADD R2, 42
```

In isolation,  this "optimization" would  seem to make things  worse: the
`IADD`  would  end  up consuming  one  MORE  word  of  space due  to  the
presence  of immediate  data. However,  in combination  with some  of the
other optimizations,  doing this  could help enable  further optimization
possibilities.

Optimization can be as much an art as it is a science.

### 12. Jump Chain Elimination (`peephole_jmp_chain`)

Short-circuits jump  indirection. If a jump  lands on a label  whose only
immediate instruction  is another unconditional  jump, the first  jump is
updated to point directly to the final target.

```vircon32
; BEFORE                             ; AFTER
JMP label_step1                      JMP label_final
...                                  ...
label_step1:                         label_step1:
JMP label_final                      JMP label_final
```

---

## Phase 2: Global Data-Flow & Dead Code (`-O2`)

Phase 2 constructs a **Control Flow  Graph (CFG)** across basic blocks to
perform program-wide data-flow analysis.

### 13. Dead Function Elimination (`dce`)

Performs  a  reachability  analysis  starting from  known  program  roots
(`__boot_vector`,  `main`,  `_start`,  interrupt  service  routines,  and
`pointer` directives).  It traces all  function call branches  and sweeps
away entire functions that can never be reached during execution.

```vircon32
; BEFORE                             ; AFTER
__function_main:                     __function_main:
    CALL __function_init                 CALL __function_init
    RET                                  RET

__function_unused:                   ; (Unreachable function eliminated)
    MOV R1, 0
    RET
```

More  for  the  lua  compiler,  as recent  versions  of  the  Vircon32  C
compiler actually perform a form  of dead function elimination during the
compilation step.

### 14. Global Constant Propagation & Folding (`constant_folding`)

Uses  a lattice-based  worklist algorithm  over  the CFG  to track  known
register constants across block  boundaries. It folds constant arithmetic
and replaces register  references with immediate values  across jumps and
branches, safely  invalidating state during function  `CALL`s or indirect
writes.

```vircon32
; BEFORE                             ; AFTER
block_1:                             block_1:
    MOV R1, 10                           MOV R1, 0xA
    JMP block_2                          JMP block_2
block_2:                             block_2:
    MOV R2, R1                           MOV R2, 0xA
```

---

## Phase 3: Interprocedural Inlining (`-O3`)

### 15. Function Inlining (`inline`)

Identifies trivial functions (short  execution lengths and simple control
flow) and replaces their `CALL`  sites directly with the function's body.
This eliminates call/return jump overhead  and exposes new local peephole
opportunities at the call site.

> **Diagnostic Flags:**  Cap inlining behavior using  `-finline-max=N` or
> exclude specific functions using `-finline-exclude=<name>`.

```vircon32
; BEFORE                             ; AFTER
CALL __function_add_one              IADD R1, 1
...                                  ...
__function_add_one:                  __function_add_one:
    IADD R1, 1                           IADD R1, 1
    RET                                  RET
```

This optimization  has proved  to be  the heavy-hitter  when it  comes to
appreciable optimization gains (performance and space-wise, due to how it
can  eliminate a  bunch of  unnecessary instructions  related to  the set
up  and  tear down  of  a  function). But  it  also  is considered  quite
*aggressive* in the level in which it will modify your code.

---

## Experimental Passes: Memory-to-Register Promotion

> **Note:**  These  passes  are   currently  disconnected  from  standard
> `-O1`/`-O2`/`-O3` optimization levels  while undergoing testing. Enable
> them explicitly using individual `-fopt_` toggles.

### 16. Stack Slot Promotion (`promote_regs` / `promote_leaf`)

Performs  scalar  replacement  of  aggregates on  the  stack.  In  **leaf
functions** (functions that make no `CALL`s and never take the address of
`BP`),  frequently accessed  local  stack  variables (`[BP-offset]`)  are
promoted to unused general-purpose  registers (`R1–R13`). The optimizer
injects pre-header  loads from  the stack  and post-header  stores before
`RET`.

```vircon32
; BEFORE                 ; AFTER
__function_compute:      __function_compute:
    MOV BP, SP               MOV BP, SP
    MOV [BP-1], 10           MOV R1, [BP-1]     ; (Pre-header load injected)
    IADD [BP-1], 5           MOV R1, 10         ; (Stack accesses promoted to R1)
    MOV R2, [BP-1]           IADD R1, 5
    RET                      MOV R2, R1
                             MOV [BP-1], R1     ; (Post-header store injected)
                             RET
```

The benefit  here is that, by  factoring out regular stack  access during
some core  section of  code, we  potentially eliminate  the corresponding
`MOV`s needed to copy data  to/from memory, instead dealing directly with
registers.

As you can  see in this (short) example, it  actually makes the footprint
of code  larger, but if the  core action is  more than just a  few lines,
this can start to offer real benefit.

### 17. Loop-Invariant Register Promotion (`promote_loops`)

Targets call-free loops (`__for_start`, `__while_start`) to promote stack
variables  referenced  inside the  loop  body  into CPU  registers.  This
eliminates repetitive memory read/write cycles during loop iterations.

```vircon32
; BEFORE                             ; AFTER
__for_1_start:                       __for_1_start:
    IADD [BP-2], 1                       IADD R2, 1         ; (Promoted to register inside loop)
    JMP __for_1_start                    JMP __for_1_start
```

One of  the biggest abusers of  frequent stack access during  runtime are
loops. If  we can mitigate  that in any  way, that should  translate into
fewer instructions per loop iteration, which could add up to considerable
savings.

Think of those examples  where you do a "just in  time" declaration of an
`index` variable,  whose *sole* purpose is  to drive the loop.  You don't
need or do anything with `index` once the loop has completed. But without
this optimization,  the compiler would  allocate memory (via  the stack),
and there would  be constant MOVs to  read and write the  `index` data to
the stack during the loop.

---

## CFG Visualization

### Exporting Control Flow Graphs

You can  generate visual diagrams  of your assembly's control  flow graph
using the `--dot` parameter:

```bash
v32opt game.asm -O2 --dot cfg.dot
dot -Tpng cfg.dot -o cfg.png
```

This  exports  blocks, labels,  instruction  lists,  and directed  branch
edges (handling fall-throughs,  unconditional jumps, conditional branches
(`JT`/`JF`), and calls) into standard Graphviz format.

---

## Safety & Correctness Guardrails

To  prevent optimizations  from  altering program  semantics or  breaking
edge-case  behaviors, the  optimizer  enforces  strict structural  safety
checks:

*  **Float  Literal   Preservation:**  Floating-point  immediates  (e.g.,
`0.500000`)  are   explicitly  distinguished  from   integer  immediates.
Constant tracking  passes treat float literals  as unknown (`VAL_BOTTOM`)
rather than parsing them as  integer `0`s. This prevents constant folding
from silently corrupting floating-point  arguments (such as audio channel
volumes or physics calculations).

* **Self-Referential Load Protection:** Redundant move elimination deeply
inspects  indirect memory  loads. Textually  identical instructions  like
`MOV  R1, [R1]`  followed  by  a second  `MOV  R1,  [R1]` are  recognized
as   pointer-dereference  chaining   rather  than   duplicates,  ensuring
pointer-to-pointer lookups evaluate correctly.

* **Address-Taking Guardrails:** Register promotion passes scan functions
and  loops for  stack-address calculations  (e.g., `IADD  R1, BP`).  If a
local  variable's  memory  address  is  dynamically  computed  or  taken,
promotion is  automatically aborted  for that  block to  guarantee memory
safety and prevent aliasing bugs.

*  **Inlining  Stack  Rewriting:**   When  leaf  functions  are  inlined,
parameter  reads accessing  `[BP+N]`  (where `N  >=  2`) are  dynamically
rewritten to `[SP+(N-2)]` at the call site. This allows seamless splicing
of callee  bodies without corrupting  caller stack frames or  requiring a
dedicated frame pointer.

---

## Diagnostic and Debug Options

When working with complex codebases or isolating runtime issues caused by
aggressive transformations, use the following diagnostic flags to control
optimizer behavior:

* **`-finline-max=N`**

Caps the total  number of `CALL` sites inlined across  the entire file to
`N` (evaluated in file order). By  adjusting this number, you can perform
binary-search bisection on inlined calls to isolate runtime-only bugs.

*(Default: `-1`, meaning no limit).*

* **`-finline-exclude=NAME`**

Excludes   specific   functions   from    being   inlined.   Supports   a
comma-separated list of target label names.

*(Example: `-finline-exclude=__function_play_audio,__function_update_physics`).*

* **`-fmax_passes=N`**

Limits the iterative local optimization engine to a maximum of `N` passes
(default: `1000`).
