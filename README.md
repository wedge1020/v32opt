# Vircon32 Assembly Optimizer

An  assembly-level optimization  tool designed  for the  Vircon32 fantasy
console.  This   tool  processes  Vircon32  assembly   code  (`.asm`)  to
analyze control  flow, reduce instruction count,  eliminate redundancies,
and  promote  stack variables  to  fast  general-purpose registers  while
guaranteeing strict memory and execution safety.

---

## Table of Contents

1. [Command-Line Usage](#1-command-line-usage)
2. [Optimization Levels](#2-optimization-levels)
3. [Fine-Grained Pass Control](#3-fine-grained-pass-control)
4. [Guide to Optimization Passes](#4-guide-to-optimization-passes)
* [Register Promotion (Scalar Replacement)](#register-promotion-scalar-replacement)
* [Strength Reduction](#strength-reduction)
* [Core & Global Optimizations](#core--global-optimizations)


5. [Safety & Correctness Guardrails](#5-safety--correctness-guardrails)
6. [Diagnostic & Debug Options](#6-diagnostic--debug-options)

---

## 1. Command-Line Usage

The  optimizer accepts  an input  assembly file  and allows  customizable
control  over   general  optimization  levels,  individual   passes,  and
diagnostic outputs.

```text
Usage: optimizer <input.asm> [-o output.asm] [options]

```

### General Options

| Option | Description |
| --- | --- |
| **`-o <file>`** | Specify the output assembly file name. If omitted, defaults to `<input>Opt.asm`. |
| **`-v`** | Enable verbose output to display detailed pass statistics, transformation counts, and execution metrics. |
| **`--dot <cfg.dot>`** | Export the Control Flow Graph (CFG) of the program to a Graphviz DOT file for visual analysis. |

---

## 2. Optimization Levels

For  quick  configuration,  the  optimizer groups  standard  passes  into
optimization  levels  ranging  from  `-O0`  (no  optimization)  to  `-O3`
(aggressive optimization).

| Level | Enabled Passes | Target Goal |
| --- | --- | --- |
| **`-O0`** | None (Default) | Fast compilation and clean debugging. |
| **`-O1`** | `peephole`, `algebraic`, `forwarding`, `jump_next`, `redundant_movs`, `combine_immediates`, `strength_reduction` | Local, basic block-level code cleanup and speed improvements without structural changes. |
| **`-O2`** | All `-O1` passes plus: `dce` (Dead Code Elimination), `constant_folding` | Global data-flow optimizations across entire functions. |
| **`-O3`** | All `-O2` passes plus: `inline` | Aggressive interprocedural optimizations for maximum execution speed. |

> **Important  Note   on  Register  Promotion:**   The  stack-to-register
> promotion passes (`promote_regs`,  `promote_leaf`, and `promote_loops`)
> are currently **unassigned** to optimization levels `-O1` through `-O3`
> to  allow  for  standalone  verification  and  testing.  They  must  be
> explicitly enabled  using their respective  `-fopt_<name>` command-line
> flags.

---

## 3. Fine-Grained Pass Control

You can independently enable or  disable any individual optimization pass
using  specific command-line  flags.  This allows  you  to customize  the
optimization pipeline or isolate problematic transformations.

* **Enable a Pass:** `-fopt_<pass_name>` (e.g., `-fopt_promote_leaf`, `-fopt_strength_reduction`)
* **Disable a Pass:** `-fno_opt_<pass_name>` (e.g., `-fno_opt_inline`, `-fno_opt_dce`)

### Available Pass Names

* **Register Promotion:** `promote_regs`, `promote_leaf`, `promote_loops`
* **Arithmetic & Instructions:** `strength_reduction`, `combine_immediates`, `algebraic`, `peephole`
* **Control Flow & Data Flow:** `forwarding`, `jump_next`, `redundant_movs`, `dce`, `constant_folding`
* **Interprocedural:** `inline`

---

## 4. Guide to Optimization Passes

### Register Promotion (Scalar Replacement)

The  optimizer   features  memory-to-register  promotion   that  converts
frequently  accessed  local  stack variables  (`[BP-offset]`)  into  fast
general-purpose registers  (`R1`–`R13`), significantly  reducing memory
bandwidth usage.

#### Leaf Function Stack Promotion (`-fopt_promote_leaf` / `-fopt_promote_regs`)

In   leaf  functions   (functions  that   do  not   execute  any   `CALL`
instructions), local stack variables accessed 3 or more times are hoisted
into  unused registers  for  the  entire duration  of  the function.  The
optimizer automatically  inserts a pre-header load  immediately following
the stack prologue.

```assembly
; BEFORE PROMOTION
__function_calculate:
    PUSH BP
    MOV BP, SP
    MOV R0, [BP-4]     ; Repeated memory reads/writes to local variable
    IADD R0, 10
    MOV [BP-4], R0
    MOV R0, [BP-4]
    MOV SP, BP
    POP BP
    RET

; AFTER PROMOTION (-fopt_promote_leaf)
__function_calculate:
    PUSH BP
    MOV BP, SP
    MOV R1, [BP-4]     ; Hoisted pre-header load into free register R1
    MOV R0, R1         ; Memory accesses rewritten to fast register moves
    IADD R0, 10
    MOV R1, R0
    MOV R0, R1
    MOV SP, BP
    POP BP
    RET

```

#### Loop-Invariant Register Promotion (`-fopt_promote_loops`)

For loops that do not contain `CALL`, `RET`, or `HLT` instructions, stack
variables accessed  at least twice within  the loop body are  promoted to
registers. A pre-header load is inserted  before the loop entry jump, and
an exit store is inserted after the loop exit label (only if the variable
was modified inside the loop).

```assembly
; BEFORE PROMOTION
__for_1_start:
    MOV R0, [BP-8]     ; Memory load inside loop body
    IEQ R0, 100
    JT __for_1_exit
    IADD R0, 1
    MOV [BP-8], R0     ; Memory store inside loop body
    JMP __for_1_start
__for_1_exit:

; AFTER PROMOTION (-fopt_promote_loops)
    MOV R2, [BP-8]     ; Pre-header load before loop entry
__for_1_start:
    MOV R0, R2         ; Fast register read
    IEQ R0, 100
    JT __for_1_exit
    IADD R0, 1
    MOV R2, R0         ; Fast register update
    JMP __for_1_start
__for_1_exit:
    MOV [BP-8], R2     ; Exit store preserves modified value after loop

```

---

### Strength Reduction

Enabled   via   `-fopt_strength_reduction`    (or   `-O1`),   this   pass
replaces   computationally    expensive   arithmetic    operations   with
cheaper,  cycle-optimized  equivalents.  It focuses  heavily  on  integer
multiplications (`IMUL`) involving immediate constants:

* **Multiplication by 0:** Replaced with a simple immediate move (`MOV dst, 0`).
* **Multiplication by 1:** Identified as an identity operation and completely eliminated.
* **Multiplication by 2:** Converted to an integer addition with itself (`IADD dst, dst`), avoiding multiplication hardware overhead.

```assembly
; BEFORE STRENGTH REDUCTION
    IMUL R0, 0
    IMUL R1, 1
    IMUL R2, 2

; AFTER STRENGTH REDUCTION (-fopt_strength_reduction)
    MOV R0, 0          ; Replaced IMUL by 0
                       ; IMUL R1, 1 completely removed
    IADD R2, R2        ; Replaced IMUL by 2 with self-addition

```

---

### Core & Global Optimizations

* **`peephole` & `algebraic`:** Simulates local instruction pairs to eliminate redundant operations (e.g., subtracting zero, double negations).
* **`redundant_movs` & `forwarding`:** Tracks register contents within basic blocks to eliminate unnecessary `MOV` instructions and forward values directly to their destination.
* **`combine_immediates` & `constant_folding`:** Evaluates expressions with known constant operands at compile time rather than runtime.
* **`jump_next` & `dce` (Dead Code Elimination):** Removes jumps that point directly to the sequential next line and deletes unreachable code blocks or unused assignments.
* **`inline`:** Replaces `CALL` instructions to small or frequently used functions with the actual body of the callee, eliminating call-frame setup and teardown overhead.

---

## 5. Safety & Correctness Guardrails

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

## 6. Diagnostic & Debug Options

When working with complex codebases or isolating runtime issues caused by
aggressive transformations, use the following diagnostic flags to control
optimizer behavior:

* **`-finline-max=N`**
Caps the total number of `CALL` sites inlined across the entire file to `N` (evaluated in file order). By adjusting this number, you can perform binary-search bisection on inlined calls to isolate runtime-only bugs.
*(Default: `-1`, meaning no limit).*
* **`-finline-exclude=NAME`**
Excludes specific functions from being inlined. Supports a comma-separated list of target label names.
*(Example: `-finline-exclude=__function_play_audio,__function_update_physics`).*
