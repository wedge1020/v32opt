# v32opt — Vircon32 Compiler-Generated Assembly Language Output Optimizer

`v32opt` is a multi-pass optimizing  pipeline for Vircon32 (V32) assembly
code.  Written in  standard C,  `v32opt` takes  assembly (`.asm`)  source
files,  builds a  Control  Flow Graph  (CFG),  performs global  data-flow
analysis, and  runs a series  of local and  global passes to  emit highly
optimized assembly.

Whether you are writing assembly by hand or optimizing compiler-generated
output,  `v32opt` reduces  binary size  and cycle  counts by  eliminating
redundant  operations,  performing  algebraic  simplifications,  inlining
trivial functions, and stripping dead code.

This project and  this documentation - was built with  the help of Gemini
AI (Google),  a mix of  its Thinking (3.6)  mid-tier model and  Pro (3.1)
high-tier model.

---

## 🛠️ Compilation & Build Instructions

`v32opt` is written in standard C99 with minimal external dependencies.

### Standard Build (GCC / Clang)

To compile `v32opt` using `gcc` or `clang`:

```bash
# Using GCC
$ gcc -O2 -Wall -std=c99 src/*.c -o v32opt

# Using Clang
$ clang -O2 -Wall -std=c99 src/*.c -o v32opt
```

### Using Makefile

If a `Makefile` is provided in your repository:

```bash
$ make
```

To clean build artifacts:

```bash
$ make clean
```

---

## 🚀 Usage

`v32opt`  runs  as a  CLI  utility  taking  an  input assembly  file  and
generating an optimized output file.

```bash
./v32opt <input.asm> [output.asm] [options]
```

### Arguments

| Argument | Description |
| --- | --- |
| `<input.asm>` | **(Required)** Path to the source assembly file. |
| `[output.asm]` | *(Optional)* Output file path. Defaults to `<input>Opt.asm` if omitted. |

### Optimization Levels & Options

`v32opt` supports  standard compiler-style optimization flags  to control
pass aggressiveness.

| Option | Description |
| --- | --- |
| `-O0` | **Disable all optimizations**. Parses and rewrites the assembly unmodified. |
| `-O1` | **Enable local optimizations** (Peephole, Algebraic Simplifications, Store-to-Load Forwarding). |
| `-O2` | **(Default)** Enables `-O1` plus global optimizations (Dead Function Elimination, Constant Folding). |
| `-O3` | Enables `-O2` plus aggressive optimizations (Trivial Function Inlining). |

### Individual Pass Toggles

You can enable specific optimization passes explicitly if you want custom
behavior outside the standard `-O` levels:

* `-Opeephole`: Enable local peephole optimizations.
* `-Oalgebraic`: Enable algebraic simplifications.
* `-Oforwarding`: Enable store-to-load forwarding.
* `-Oinline`: Enable trivial function inlining.
* `-Odce`: Enable dead function elimination (reachability analysis).
* `-Oconstant_folding`: Enable global constant propagation and folding.

### Debugging & Analysis

*  `-v`: **Verbose  output**.  Prints detailed  pass  statistics and  the
number of optimizations applied per phase.

* `--dot  <cfg.dot>`: **Export CFG**.  Exports the internal  Control Flow
Graph to a Graphviz DOT file for visual inspection.

### Examples

```bash
# Default optimization (-O2) on game.asm, outputs to gameOpt.asm
$ ./v32opt game.asm

# Aggressive optimization (-O3) with verbose statistics
$ ./v32opt game.asm build/game_optimized.asm -O3 -v

# Export the Control Flow Graph to a visualizable DOT file
$ ./v32opt math.asm --dot math_cfg.dot
```

---

## ⚡ Supported Optimizations

### 1. Algebraic Simplifications

Replaces identity  operations with  `NOP`s (or  omits them  entirely) and
applies strength reduction to arithmetic.

* **Self-Assignments**: `MOV R, R` -> Removed.
* **Zero Arithmetic**: `IADD R, 0` or `ISUB R, 0` -> Removed.
* **Strength Reduction**: `IMUL R, 2` -> Converted to a cheaper `IADD R, R`.

```nasm
; --- Before ---
MOV R0, R0
IADD R1, 0
IMUL R2, 2

; --- After ---
; (MOV R0, R0 removed)
; (IADD R1, 0 removed)
IADD R2, R2
```

### 2. Peephole Optimizations

Scans adjacent instructions to eliminate redundant operations that cancel
each other out.

* **Stack Folding**: `PUSH R` immediately followed by `POP R` of the same
register.

* **Boolean/Bitwise  Negation**: Consecutive  `BNOT R` operations  on the
same register cancel out.

* **Redundant Cast-to-Bool**: `IEQ` followed  by `CIB` (Cast Int to Bool)
on the  same register. `IEQ` natively  guarantees a 0 or  1, making `CIB`
redundant.

```nasm
; --- Before ---
PUSH R0
POP R0
BNOT R1
BNOT R1
IEQ R2, R3
CIB R2

; --- After ---
; (PUSH/POP removed)
; (BNOT/BNOT removed)
IEQ R2, R3
```

### 3. Store-to-Load Forwarding

Eliminates redundant memory reads when a value was just written to memory
from  a register.  If an  indirect store  is immediately  followed by  an
indirect load from the exact same address, `v32opt` forwards the register
directly.

```nasm
; --- Before ---
MOV [BP-4], R1
MOV R2, [BP-4]

; --- After ---
MOV [BP-4], R1
MOV R2, R1       ; Memory read bypassed entirely
```

### 4. Dead Function Elimination (DFE)

Performs  a  global  reachability  analysis starting  from  entry  points
(`main`, `_start`, Interrupt Service  Routines, etc.). Any function label
and its  body that  cannot be  reached through the  call graph  is safely
discarded, drastically reducing final ROM size.

```nasm
; --- Before ---
main:
    CALL used_function
    RET

dead_function:       ; Unreachable from main
    MOV R0, 42
    RET

used_function:
    MOV R0, 1
    RET

; --- After ---
main:
    CALL used_function
    RET

used_function:
    MOV R0, 1
    RET
```

### 5. Trivial Function Inlining

*(Requires `-O3` or `-Oinline`)*

Identifies  small "leaf"  functions  (functions with  no inner  branches,
calls, or complex  control flow) and inlines  their instructions directly
into  the caller.  This eliminates  the `CALL`/`RET`  overhead and  stack
frame setup for small helpers.

```nasm
; --- Before ---
main:
    CALL get_true
    MOV R1, R0
    RET

get_true:
    MOV R0, 1
    RET

; --- After ---
main:
    MOV R0, 1        ; Inlined body of get_true
    MOV R1, R0
    RET
```

### 6. Global Constant Propagation & Folding

Builds a  global Control  Flow Graph  (CFG) to  track constant  values in
registers  across basic  blocks. If  a register's  value is  definitively
known at  compile-time when  an instruction  executes, `v32opt`  folds it
into an immediate mode instruction.

```nasm
; --- Before ---
MOV R1, 42       ; R1 is now known to be 42
IADD R2, 5       ; Unrelated instruction
MOV R3, R1       ; R1 is STILL 42

; --- After ---
MOV R1, 42
IADD R2, 5
MOV R3, 42       ; Folded to an immediate
```
