# v32opt — Vircon32 Assembly Optimizer

`v32opt`  is  a modular,  multi-pass  assembly  optimizer written  in  C,
specifically  targeting the  **Vircon32** fantasy  console. It  takes raw
assembly  output from  a Vircon32-targeting  compiler (C,  C++ by  way of
`v32c++`, and  Lua via `v32lua`),  hand-written assembly, or  (with care,
see  below) disassembled  Vircon32  CARTs, and  applies iterative  local,
structural,  data-flow, and  register-promotion  optimizations to  reduce
code size,  see the individual build  chain steps in action,  and improve
execution efficiency.

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

  * Google Gemini,  a mix of  its Thinking (3.6)  and  Pro (3.1) models
  * Mistral Vibe (Thinking)
  * Anthropic Claude Sonnet 5 (medium)
  * OpenCode Go (GLM 5.2)

---

## Table of Contents

1. [Build Instructions](#build-instructions)
2. [Integrate into CART build process](#integrate-into-cart-build-process)
3. [Source Languages: C, C++, Lua, and Assembly](#source-languages-c-c-lua-and-assembly)
4. [Usage & Optimization Levels](#usage--optimization-levels)
5. [Phase 1: Local Peephole Optimizations (-O1)](#phase-1-local-peephole-optimizations--o1)
6. [Phase 2: Global Data-Flow & Dead Code (-O2)](#phase-2-global-data-flow--dead-code--o2)
7. [Phase 3: Interprocedural Inlining (-O3)](#phase-3-interprocedural-inlining--o3)
8. [Experimental Passes: Memory-to-Register Promotion](#experimental-passes-memory-to-register-promotion)
9. [CFG Visualization](#cfg-visualization)
10. [Safety & Correctness Guardrails](#safety--correctness-guardrails)
11. [Diagnostic and Debug Options](#diagnostic-and-debug-options)
12. [Releasing (version stamping)](#releasing-version-stamping)

---

## Build Instructions

`v32opt` is written in  standard C and is designed to  be easily built on
any  platform using  a  modern  C compiler  (`gcc`  or `clang`).  Project
headers live  in `inc/`  (split by concern:  `v32opt.h` is  the umbrella,
with  `asm.h`,  `peephole.h`,  `dataflow.h`, `inline.h`,  `stack.h`,  and
`promote.h` behind it); sources live in `src/` and `src/peephole/`.

There are  two independent  ways to  build it:  the base  `Makefile` (the
everyday developer build), and CMake  (an out-of-tree build with a system
install  and packaging  harness, modeled  on the  Vircon32 DevTools'  own
CMake setup). They  never share build products, so both  can be used side
by side.

### Using Make (Linux / macOS / MSYS2)

```bash
# Build the binary (written to ./v32opt)
$ make

# Install to your home bin directory (~/bin/bin.<arch> if it exists,
# otherwise ~/bin); "make uninstall" removes it again
$ make install

# Or install system-wide: v32opt to /usr/local/bin and the manual page to
# /usr/local/share/man/man1 (PREFIX=... changes the root; DESTDIR=... is
# honored for staging). "sudo make sysuninstall" removes both.
$ sudo make sysinstall

# Run the unit-test suite (needs the Vircon32 DevTools -- assemble, packrom
# -- on your PATH; the primes test additionally needs v32lua)
$ make tests

# Clean build artifacts ("make distclean" also removes a CMake build/ dir)
$ make clean
```

### Using CMake (Linux / macOS / Windows)

CMake builds out of tree, in a `build/` directory (an in-source `cmake .`
is refused, since it would overwrite the base `Makefile`):

```bash
$ mkdir build && cd build
$ cmake ..                    # Release build by default
$ cmake --build .
$ ctest                       # quick smoke tests (no Vircon32 tools needed)
$ sudo cmake --install .      # see install locations below
```

| Platform | `v32opt` executable | Manual page / docs |
| --- | --- | --- |
| Linux, macOS (and other Unix) | `/usr/local/bin/v32opt` | `/usr/local/share/man/man1/v32opt.1` |
| Windows (MSYS2 + MinGW) | `<Program Files>\Vircon32\DevTools\v32opt.exe` | `<Program Files>\Vircon32\DevTools\docs\v32opt\` (`README.md`, `v32opt.1`) |

On Windows the optimizer is installed  next to the Vircon32 DevTools (the
`compile.exe`/`assemble.exe`  folder  the  DevTools'  own  CMake  install
creates),  so it  is found  through the  same `PATH`  entry. As  with the
DevTools, build it  with MSYS2 + MinGW (Visual  C++ lacks `getopt_long`);
from a MinGW shell:

```bash
$ mkdir build && cd build
$ cmake -G "MSYS Makefiles" ..
$ make
$ cmake --install .           # from an elevated shell, for Program Files
```

Other useful knobs:

```bash
$ cmake -DCMAKE_INSTALL_PREFIX=$HOME/.local ..   # install somewhere else
$ cmake --install . --prefix /opt/v32opt         # ...or choose at install time
$ cmake --build . --target uninstall             # undo the last install
$ cpack                                          # .tar.gz (+ .deb/.rpm on Linux, .zip on Windows)
```

The       `V32OPT_INSTALL_BINDIR`,      `V32OPT_INSTALL_MANDIR`       and
`V32OPT_INSTALL_DOCDIR`   cache   variables   override   the   individual
destinations (relative to the prefix).

### Direct Compilation

You can  also compile the  modular codebase  directly using GCC  or Clang
(the  headers live  in  `inc/`,  and the  peephole  passes  in their  own
subdirectory):

```bash
# Compile all source files with optimization enabled
$ gcc -O2 -Wall -Wextra -Iinc src/*.c src/peephole/*.c -o v32opt

# Verify  the  build  (running  with  no  arguments  will  display  usage
# information)

$ ./v32opt
```

A  full command-line  reference is  available as  a unix  manual page  in
`man/v32opt.1` (view it with `man ./man/v32opt.1`).

---

## Integrate into CART build process

The typical sequence  to build a Vircon32 CART consists  of the following
steps (if you're writing in assembly you can start at step 2):

### write/edit source code

Currently Vircon32  provides a C  compiler (considered stable and  is the
primary  language of  development  on the  platform)  via its  `DevTools`
suite.  There are  also (in  development) third  party compilers  for Lua
(`v32lua`)  and  C++ (`v32c++`,  which  translates  C++  into C  for  the
Vircon32 C compiler).

### compile your source code

Each  compiler  translates  its   high-level  language  code/syntax  into
Vircon32 assembly.  It is this assembly  you need before you  can proceed
with optimization with `v32opt`.

```bash
# compile a C program with the Vircon32 C compiler
$ compile -o game.asm game.c

# compile a lua program with the v32lua compiler
$ v32lua -o game.asm game.lua

# compile a C++ program: v32c++ emits C, which the C compiler then compiles
$ v32c++ -o game.c game.cpp
$ compile -o game.asm game.c
```

Which    `-L`    mode    to    use    (and    what    to    watch    for)
depends   on   where   the   assembly    came   from   --   see   [Source
Languages](#source-languages-c-c-lua-and-assembly).

Should you  be writing a Vircon32  program IN assembly language,  you can
proceed straight to the next step (optimize).

If  you want  to try  and optimize  an existing,  packed binary  Vircon32
CART  (perhaps  you do  not  have  access to  the  source  code to  build
it  from  scratch),  you  can   use  the  `unpackrom`  and  `disassemble`
commands provided by  the Vircon32 `DevTools` to  obtain the disassembled
assembly  of  any   CART  --  but  read  the   disassembly  caveat  under
[Source  Languages](#source-languages-c-c-lua-and-assembly)  first:  most
optimizations break disassembled programs.

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
# run game.asm through v32opt with -O1 optimizations:
$ v32opt game.asm -o gameOpt.asm -O1
```

NOTE: the input  file is the only non-option argument,  and it may appear
before, after, or among the options (`v32opt -O1 -o gameOpt.asm game.asm`
works the same, on every platform).  Use `-o` to name the output; without
it, `game.asm`  is written  to `gameOpt.asm`.  A second  file name  is an
error rather than being silently ignored.

You can also run `v32opt` with the  `-v` argument, and it will give you a
high-level status report of optimization actions it was able to perform:

```bash
# run game.asm through v32opt with -O1 optimizations, reporting statistics:
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

## Source Languages: C, C++, Lua, and Assembly

`v32opt`  only ever  sees assembly,  but how  that assembly  was produced
matters. The passes  recognize the code shapes and  label conventions the
compilers  emit  (`__function_<name>:`  function labels,  their  internal
`..._return:`  labels, the  standard  `PUSH  BP` /  `MOV  BP, SP`  frame,
`pointer` directives for function  addresses, `..._start:` loop headers),
and the `-L` mode tells the value-tracking passes how to read immediates.

| Source | Toolchain | `-L` mode | Notes |
| --- | --- | --- | --- |
| C | `compile` (Vircon32 DevTools) | `c` (default) | The primary target; every pass is designed around this output. |
| C++ | `v32c++` → C → `compile` | `c` (default) | The assembly *is* C-compiler output, so everything said for C applies unchanged (vtables become `pointer` tables, which DCE already follows). |
| Lua | `v32lua` | `lua` | Lets the passes recognize v32lua's NaN-boxed values (below). Running v32lua output in C mode can fold, forward or CSE away boxing/unboxing arithmetic. |
| Hand-written assembly | — | `c` (default) | Works, with the caveats below. |
| Disassembled CART | `unpackrom` + `disassemble` | `c` | Only safe without any size-changing pass -- see below. |

### Lua mode (`-L lua`)

`-L lua` (long form `--langmode  lua`; the mode name is case-insensitive)
makes the  value-tracking passes aware  of `v32lua`'s boxed  type system:
`BOXED_*` tagging/untagging  idioms (OR/AND/IADD with a  boxed immediate)
are never  folded, forwarded, or  CSE'd away as ordinary  arithmetic, the
NaN-boxing  tags  (`BOXED_*`,  `NAN_VALUE`,  anything with  the  NaN  bit
pattern) are never resolved through `%define`, and the algebra pass drops
the provably-dead  "is it Nil?"  half of the compiler's  fixed truthiness
test after a boxed-boolean producer. `-L c` (the default) keeps the plain
C-mode behavior. v32lua's internal  `__global_*` helper labels inside the
global-scope initializer are understood in either mode.

### Hand-written assembly

Use the default C mode. Things to know:

* **Function-level passes key off the compiler's naming.** DCE, inlining,
  frame-pointer elimination and the promote-* passes only treat a label
  of the form `__function_<name>:` as a function. Code under other labels
  is simply never considered for removal or inlining -- safe, but those
  passes will find little to do unless you follow the same convention.
* **Anything reached only through a computed address is invisible.** A
  `__function_` routine whose address is only ever formed arithmetically,
  or jumped to via `JMP R0`/`CALL R0` with no `pointer`/operand reference to
  its name, can be removed by DCE; code after an unconditional `JMP` that
  is entered only by a computed jump (no label) is removed by
  `peephole-jumps`. Referencing the label by name anywhere (`MOV R0,
  __function_cb`, a `pointer` directive) keeps it alive.
* **Labels ending in `_start`** are taken as loop headers by the
  experimental `promote-loops` pass.
* Assembler directives (`%define`, `%include`, data directives, ...) are
  never deleted by any pass, and integer `%define` values are seen through
  unless `-fno-resolve-defines` is given.

### Disassembled CARTs

The Vircon32 `disassemble` tool names jump and call targets (`_label1`,
`_label2`, ...), but every reference to *data* in the program ROM -- string
literals, tables, initial values -- stays a hard-coded address
(`MOV R0, 0x2000002C`). `v32opt` does not relocate such addresses, so **any
optimization that changes the size of the code moves the data out from under
them** and the rebuilt CART reads the wrong memory. (Removing a single 4-word
frame prologue/epilogue is enough to break every string in the program.)
Treat optimizing disassembled code as an experiment: compare the original and
optimized CARTs carefully, and expect only programs with no ROM-resident data
references to survive. Since disassembled functions are `_labelN`, not
`__function_<name>`, DCE and inlining also find little to do there.

---

## Usage & Optimization Levels

```bash
v32opt <input.asm> [-o output.asm] [options]
```

| Flag | Description | Included Passes |
| --- | --- | --- |
| `-O0` | **No Optimization** | Disables all optimization passes (default). |
| `-O1` | **Local Peephole** | Enables all 14 local window peephole optimizations. |
| `-O2` | **Global Analysis** | Enables all `-O1` passes + **CSE**, **Dead Code Elimination (DCE)**, **Global Constant Folding** & **Frame Pointer Elimination**. |
| `-O3` | **Aggressive** | Enables all `-O2` passes + **Function Inlining**. |
| `-Os` | **Space Saving** | Currently identical to `-O3` (a placeholder for a dedicated size-focused tier). |
| `-o <file>` | **Output File** | Where to write the optimized assembly (default: `<input>Opt.asm`). |
| `-v`, `--verbose` | **Verbose Mode** | Displays detailed pass statistics and optimization counts per iteration. |
| `-t` | **Testing Mode** | Prints a machine-readable `pass:count` summary (plus a `total:N` line). |
| `-d` | **Debug Marking** | Marks every optimization inline in the output as `; [DEBUG <pass>] ...` comments. |
| `-L <c\|lua>`, `--langmode` | **Language Mode** | `lua` enables NaN-boxed-type awareness for `v32lua` output; `c` (default) for C, C++ and assembly (see [Source Languages](#source-languages-c-c-lua-and-assembly)). |
| `--dot <file>` | **CFG Export** | Exports the Control Flow Graph to a Graphviz `.dot` file for visualization. |
| `--trigger-max=<N>` | **Bisection Cap** | Global budget on total committed transformations (see below). |
| `-V`, `--version` / `-h`, `--help` | **Info** | Print the version, or a usage summary, and exit. |

`v32opt` is silent on success unless `-v` or `-t` is given, and exits
with status `1` (and a message on stderr) for a bad option, a missing or
unreadable input, an unwritable output, or an input line too long to
process safely.

### Optimization Tier Philosophy & Debugging Considerations

In `v32opt`, optimization  tiers are separated not just by  how much they
shrink  the binary,  but  by their  computational complexity,  structural
impact, and debugging ergonomics:

*  **`-O1`  (Local Peepholes):**  Operates  on small  sliding windows  of
instructions (a few passes scan further ahead, bounded by a scan-distance
cap).  These are  stateless,  linear-time passes  that  clean up  obvious
compiler  artifacts with  no  structural  impact on  the  program and  no
effect  on debugging  ergonomics —  the  least risky  tier. That  said,
"least risky"  is not "risk-free"  (see `ISSUES` for the  remaining known
limitations),  so it  is  still worth  play-testing  the optimized  build
against the original.

* **`-O2`  (Global Analysis  & Structural Cleanup):**  Introduces Control
Flow  Graphs  (CFG), program-wide  data-flow  tracking,  and stack  frame
modifications.  These  passes  analyze  entire blocks  and  functions  to
eliminate  unreachable  code,  fold  constants across  jumps,  and  strip
redundant overhead.

* **`-O3` (Aggressive Interprocedural  Transformations):** Makes sweeping
architectural  modifications—such  as  function  inlining—that  trade
binary  size  for  execution   velocity  and  completely  erase  function
boundaries.

#### Why Frame Pointer Elimination lives in `-O2`

From  a  pure  compiler  design perspective,  Frame  Pointer  Elimination
(`omit_frame_pointers`)  looks like  a  textbook  `-O1` optimization:  it
requires only a  fast linear scan, it *always*  reduces instruction count
and binary size,  and it offers immediate execution speed  gains for leaf
functions.

However,  we   deliberately  promote  it  to   `-O2`.  Standard  function
prologues (`PUSH  BP` followed by  `MOV BP,  SP`) create a  stable linked
list  of  stack frames  in  memory.  When  frame pointers  are  stripped,
emulators  and  debuggers  can  no  longer walk  backward  from  `BP`  to
generate clean  call-stack backtraces during runtime  crashes. By placing
`omit_frame_pointers` in `-O2`, `-O1` remains a high-performance yet 100%
"debug-safe" tier!

### Individual Optimization Control

You can enable or disable specific passes granularly using `-f<name>` and
`-fno-<name>`:

```bash
# Example: Run O2 but disable jump chaining and enable loop register promotion
$ v32opt game.asm -O2 -fno-peephole-jmp-chain -fpromote-loops
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

### Adjacent Instruction Pair Elimination (`peephole-pairs`)

Scans  consecutive   pairs  to  remove  redundant   operations,  such  as
Convert Integer  to Boolean (`CIB`) instructions  immediately following a
destructive comparison (`IEQ`/`INE`,  which already leave a  clean `0` or
`1` in the destination register),  double bitwise negations (`BNOT`), and
zero-net-effect stack operations (`PUSH`/`POP`).

```vircon32
; BEFORE                 ; AFTER
IEQ R1, R2               IEQ R1, R2
CIB R1                   ; (CIB removed: IEQ already outputs 0 or 1)

BNOT R3
BNOT R3                  ; (Double negation cancelled out)

PUSH R4
POP R4                   ; (PUSH/POP pair removed)
```

For  those  that understand  assembly,  you  should  see that  these  are
generally useless  progressions, eating  up CPU  cycles and  not actually
contributing  anything to  your  game. Removing  them  means less  cycles
consumed per frame (and that *could* improve performance).

### Algebraic Simplification (`peephole-algebra`)

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

### Store-to-Load Forwarding (`peephole-forwarding`)

When a value  is stored from a register to  memory and immediately loaded
back from  that exact  memory address into  another register,  the memory
read is replaced with a direct register-to-register move.

```vircon32
; BEFORE                             ; AFTER
MOV [R1+4], R2                       MOV [R1+4], R2
MOV R3, [R1+4]                       MOV R3, R2
```

While this may  not offer any distinct performance boost,  it should save
you 1 word  of space, as the resulting double  registered `MOV` will only
need  1 word  to  store  the instruction,  where  any indirect  reference
requires a second, follow-on word for the immediate value/address.

### Redundant Reload Elimination (`peephole-compiler-myopia`)

Removes a reload of  a value that was just stored  from the same register
to the same  memory location, when nothing in between  changed the memory
or the registers involved.

```vircon32
; BEFORE                             ; AFTER
MOV [R1+4], R2                       MOV [R1+4], R2
MOV R2, [R1+4]                       ; (Reload removed: R2 still holds it)
```

A common compiler artifact: the code generator writes a temporary back to
its stack slot and then immediately  reads it again before anything could
have changed.

### Redundant Jump Elimination (`peephole-jumps`)

Three  local control-flow  cleanups: removes  `JMP`/`JT`/`JF` that  point
directly  to the  label  immediately following  the instruction;  inverts
branch-over-jump pairs (`JF R, L1; JMP L2; L1:` becomes `JT R, L2; L1:`);
and eliminates code made unreachable by an unconditional `JMP` (up to the
next label, which is kept).

```vircon32
; BEFORE                             ; AFTER
JMP loop_continue                    ; (Fall-through jump removed)
loop_continue:                       loop_continue:
```

### Redundant & Mirror Move Elimination (`peephole-movs`)

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

### Immediate Math Combining (`peephole-immediates`)

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

### Strength Reduction (`peephole-reduce`)

Simplifies arithmetic operations  with special constants. Multiplications
by  `0`  become  `MOV  0`,   multiplications  or  divisions  by  `1`  are
eliminated, and  multiplications by  `2` are  converted to  addition. For
floating-point forms only  the exact identities (`FMUL r,  1.0`, `FDIV r,
1.0`) are  applied — `FMUL  r, 0.0`  is deliberately left  alone, since
`Inf/NaN * 0.0` is `NaN`, not `0.0`.

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

### Shift Optimizations (`peephole-shifts`)

Removes no-op shifts by `0` and  converts left-shifts by `1` (`SHL r, 1`)
into self-additions (`IADD r, r`).

```vircon32
; BEFORE                             ; AFTER
SHL R1, 0                            ; (Shift by 0 removed)

SHL R2, 1                            IADD R2, R2
```

### Zero-Test Folding (`peephole-zero-test`)

Vircon32's conditional jumps already test a register against zero (`JT`
jumps when it is non-zero, `JF` when it is zero), so materialising the
comparison first is redundant. This pass folds the comparison into the
jump, both when a copy is tested and when the value is tested in place.

```vircon32
; BEFORE                             ; AFTER
MOV R0, R4
IEQ R0, 0
JT  R0, __slow_path                  JF  R4, __slow_path

INE R5, 0
JF  R5, __is_zero                    JF  R5, __is_zero
```

The original leaves the comparison result (0 or 1) in the scratch
register, so the rewrite is only applied when that register is proven
never to be read again on **either** side of the branch. The proof
follows jumps and looks into called functions; anything it cannot
follow (a computed jump or call, registers `R11`-`R15`, a callee that
may read the register) leaves the code untouched. Integer compares only:
`FEQ` treats `-0.0` as zero, while `JF` tests the raw bits.

v32lua emits this shape in every inline table lookup, so on Lua-mode
programs it removes roughly 3% of all instructions.

### Dead Store Elimination (`peephole-dead-stores`)

Removes memory  stores that are  immediately overwritten by  a subsequent
store to  the exact same  indirect memory address without  an intervening
read.

```vircon32
; BEFORE                             ; AFTER
MOV [R1+0], R2                       ; (Overwritten store removed)
MOV [R1+0], R3                       MOV [R1+0], R3
```

### Redundant Load Elimination (`peephole-loads`)

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

### Immediate Folding (`peephole-immediate-prop`)

Despite its name, this pass folds  constants *within* a register rather than
propagating them into other instructions. It does three things with integer
immediates on a register destination:

* drops identity arithmetic (`IADD`/`ISUB` by `0`, `IMUL`/`IDIV` by `1`);
* folds a constant `MOV` into a later `IADD`/`ISUB`/`IMUL` of the same register
  (never across a read or write of that register, a conditional branch, or
  a label/jump/call boundary);
* merges adjacent `IADD`/`ISUB` immediates on the same register, removing the
  pair outright when they cancel.

```vircon32
; BEFORE                             ; AFTER
MOV R1, 10                           MOV R1, 15
IADD R1, 5                           ; (Folded into the MOV)

IADD R2, 5                           IADD R2, 2
ISUB R2, 3                           ; (Merged)
```

A fold whose result would not fit a 32-bit literal (the arithmetic
overflowed) is left alone. `MOV R1, 42` / `IADD R2, R1` is *not* rewritten
to `IADD R2, 42` -- that would cost a word, not save one.

### Jump Chain Elimination (`peephole-jmp-chain`)

Short-circuits jump indirection: a `JMP` that sits directly before its own
target label, whose first instruction is another unconditional `JMP`, is
retargeted to the final destination, and the intermediate jump is removed.

```vircon32
; BEFORE                             ; AFTER
JMP label_step1                      JMP label_final
label_step1:                         label_step1:
JMP label_final                      ; (Intermediate jump removed)
```

Only this adjacent shape is handled at present: a jump elsewhere in the
code that targets `label_step1` is not retargeted (it still reaches
`label_final` through the intermediate jump, which is then kept).

NOTE: the intermediate  jump is only removed when nothing  else can reach
its label: no other jump or branch targets it, and no code can fall into
it (the preceding instruction must itself be an unconditional transfer).
Otherwise it is kept and still routes every remaining user correctly.
Multi-hop chains collapse one hop per iteration, under the same adjacency
condition.

---

## Phase 2: Global Data-Flow & Dead Code (`-O2`)

Phase 2 constructs a **Control Flow  Graph (CFG)** across basic blocks to
perform program-wide data-flow analysis.

### Common Subexpression Elimination (CSE)

**What it  does:** Detects  and eliminates redundant  computations within
basic  blocks. When  the same  operation is  applied to  the same  source
operands (after  identical `MOV` initializations), the  second occurrence
is replaced  with a `MOV`  from the  first result register.  On Vircon32,
where  all  instructions are  1  cycle,  this  **reduces code  size**  by
eliminating duplicate word usage.

---

#### **Pattern**
```
MOV Rx, A    ; Compute Rx = A
OP Rx, B     ; Compute Rx = Rx OP B
...
MOV Ry, A    ; Re-initialize Ry = A (same as Rx was)
OP Ry, B     ; -- CSE replaces this with: MOV Ry, Rx
```

---

#### **Examples**

| Before | After | Savings |
|--------|-------|---------|
| `MOV R1, R5`<br>`IADD R1, R2`<br>`MOV R3, R5`<br>`IADD R3, R2` | `MOV R1, R5`<br>`IADD R1, R2`<br>`MOV R3, R5`<br>`MOV R3, R1` | 0 words by itself (1-word op → 1-word `MOV`); see below |
| `MOV R1, R5`<br>`IMUL R1, 42`<br>`MOV R3, R5`<br>`IMUL R3, 42` | `MOV R1, R5`<br>`IMUL R1, 42`<br>`MOV R3, R5`<br>`MOV R3, R1` | **1 word** (`IMUL` + immediate = 2 words, `MOV` = 1) |
| `MOV R1, R5`<br>`FADD R1, R2`<br>`MOV R3, R5`<br>`FADD R3, R2` | `MOV R1, R5`<br>`FADD R1, R2`<br>`MOV R3, R5`<br>`MOV R3, R1` | 0 words by itself; see below |

The re-initializing `MOV R3, R5` is now dead (immediately overwritten), and
at `-O2` `peephole-dead-stores` removes it -- so in a full `-O2` run every
row above saves one more word.

---

#### **Supported Operations**
All arithmetic, logical, and floating-point ops:
`IADD`, `ISUB`, `IMUL`, `IDIV`, `AND`, `OR`, `XOR`, `FADD`, `FSUB`, `FMUL`, etc.
Works with registers, immediates (including negatives), and respects:
- ✅ Control flow boundaries (`JMP`, `JT`, `JF`, `CALL`, `RET`, labels)
- ✅ Register modifications between expressions
- ✅ Different operations (e.g., `IADD` ≠ `ISUB`)

### Dead Function Elimination (`dce`)

Performs  a  reachability  analysis  starting from  known  program  roots
-- the code before the first function (the boot path), `__function_main`
/ `main` / `_start` / `start` / `__start`, global-initialization routines
(`__init_globals`, `__function_init`, v32lua's global-scope initializer),
labels containing `ISR` or `interrupt`, data labels, and every function
named in a `pointer` directive. Any function whose name appears as an
operand of reachable code (a `CALL`, or a `MOV R0, __function_cb` taking
its address) becomes reachable in turn; functions (labels of the form
`__function_<name>:`) that are never reached are swept away.

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
compilation step (C++ goes through that same C compiler).

### Global Constant Propagation & Folding (`constant-folding`)

Uses  a lattice-based  worklist algorithm  over  the CFG  to track  known
register constants across block  boundaries. It folds constant arithmetic
and replaces register  references with immediate values  across jumps and
branches, safely  invalidating state during function  `CALL`s or indirect
writes.

```vircon32
; BEFORE                             ; AFTER
block_1:                             block_1:
    MOV R1, 10                           MOV R1, 10
    JMP block_2                          JMP block_2
block_2:                             block_2:
    MOV R2, R1                           MOV R2, 10
```

Note this trades words for immediates (`MOV R2, 10` takes two words where
`MOV R2, R1` took one), so on its own it can *grow* the assembled binary;
its payoff is in the immediate-based peepholes it enables afterwards.

### Frame Pointer Elimination (`omit-frame-pointers`)

Scans  entire function  bodies to  verify  if the  Base Pointer  register
(`BP`)  is ever  referenced or  dereferenced by  instructions within  the
function. In  functions where `BP`  is never  touched (such as  leaf math
helpers, getters,  or functions utilizing only  general-purpose registers
`R0–R15`), it then completely eliminates the standard function prologue
(`PUSH BP`,  `MOV BP,  SP`) and  epilogue (`MOV SP,  BP`, `POP  BP`). Any
BP-relative  reference  counts  as  "touching"  BP  —  parameter  slots
(`[BP+N]`) AND local-variable slots  (`[BP-N]`) alike, since both address
memory through the frame pointer.

This  strips  4  redundant  stack  and  memory  instructions  from  every
invocation, noticeably reducing cycle overhead and shrinking total binary
footprint without altering register logic.

```vircon32
; BEFORE                 ; AFTER
__function_add:          __function_add:
    PUSH BP                  ; (Prologue frame setup removed)
    MOV BP, SP               ;
    IADD R1, R2              IADD R1, R2
    MOV R0, R1               MOV R0, R1
    MOV SP, BP               ; (Epilogue frame teardown removed)
    POP BP                   ;
    RET                      RET
```

---

## Phase 3: Interprocedural Inlining (`-O3`)

### Function Inlining (`inline`)

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
> them explicitly using individual `-f` toggles.

### Stack Slot Promotion (`promote-leaf` / `promote-regs`)

Performs  scalar  replacement  of  aggregates on  the  stack: frequently
accessed  local  stack  variables (`[BP-offset]`)  are promoted to unused
general-purpose  registers (`R1–R13`), with a load from the stack slot at
the start of the region and a store back at its end.

* `promote-leaf` works on whole **leaf functions** (functions that make no
  `CALL`s and never take or overwrite `BP`'s value); the store goes in
  before the epilogue that precedes `RET`.
* `promote-regs` applies the same logic to ordinary functions, separately
  within each `CALL`-free stretch (before the first `CALL`, between calls,
  after the last), and only where that stretch is single-entry/single-exit.

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

### Loop-Invariant Register Promotion (`promote-loops`)

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

* **Literal Range:** Every constant fold is computed wide and abandoned if
the result doesn't fit a 32-bit literal the Vircon32 assembler accepts, so an
overflowing `IMUL`/`IADD` chain is left as written instead of producing an
out-of-range immediate.

* **Directives Are Never Deleted:** No pass removes an assembler directive
(`%define`, `%include`, `integer`/`float`/`string`/`pointer`/`datafile` data),
even when it sits inside a range of dead code being swept away. An
instruction or data line too long for the optimizer to hold is reported as
an error rather than silently truncated.

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

*  **Inlining  Stack  Rewriting:**  When   leaf  functions  are  inlined,
parameter  reads accessing  `[BP+N]`  (where `N  >=  2`) are  dynamically
rewritten to `[SP+(N-2)]` at the call site. This allows seamless splicing
of callee  bodies without corrupting  caller stack frames or  requiring a
dedicated frame pointer.

*  **These  guardrails  are  not  exhaustive.**  The  window  passes  now
model read-modify-write memory instructions  (`IADD [R1+0], 5`), aliasing
stores  through   different  base   registers,  and   the  dynamic-memory
`MOVS`/`SETS`/`CMPS` operations,  and `CALL`/`RET` are treated  as moving
SP (the  hardware return-address  push/pop). Remaining  known limitations
are  tracked  in  the   `ISSUES`  file  (most  notably:  unreachable-code
elimination does  not attempt to  reason about computed jump  targets —
`JMP R0`  / `JMP  0x1000` — and  DCE's `_return:`-suffix  heuristic can
confuse a real  function whose name happens to end  in `_return`). Always
verify  optimized  output  by  building  and  running  it  alongside  the
original.

---

## Diagnostic and Debug Options

When working with complex codebases or isolating runtime issues caused by
aggressive transformations, use the following diagnostic flags to control
optimizer behavior:

* **`-finline-max=N`**

Caps  the  size  of  a  function  still  considered  inlinable,  in  body
instructions: any  function whose straight-line  body is longer  than `N`
instructions  is never  inlined. The  value  is clamped  to `0..32`  (the
candidate-table capacity).

*(Default: `8`).*

* **`-finline-call-limit=N`**

Caps the  total number of `CALL`  sites inlined across the  entire run to
`N` (evaluated in file order). By  adjusting this number, you can perform
binary-search bisection on inlined calls to isolate runtime-only bugs.

*(Default: `-1`, meaning no limit).*

* **`-finline-exclude=NAME`**

Excludes   specific   functions   from    being   inlined.   Supports   a
comma-separated list of target label names.

*(Example: `-finline-exclude=__function_play_audio,__function_update_physics`).*

* **`-fmax-passes=N`**

Limits the iterative local optimization engine to a maximum of `N` passes
(default: `1000`).

* **`--trigger-max=N`**

A global  transformation budget  shared by EVERY  enabled pass  and every
fixed-point  iteration:  once a  total  of  `N` transformations  (deleted
nodes,  in-place rewrites,  spliced  loads/stores)  have been  committed,
every later candidate is left untouched  as if it had never matched. Omit
(or pass a negative `N`) for  unlimited. The primary tool for bisecting a
miscompile: re-run  with increasing `N`  until the output breaks  — the
`N`th transform applied,  in program order across all passes,  is the one
to inspect.  Combine with  `-d` to  have the culprit  named right  in the
output file,  and with  `-v` for a  final report of  how many  slots were
used.

---

## Releasing (version stamping)

The version lives in exactly one place: the `VERSION` `#define` in
`inc/v32opt.h` (the same `YYYYMMDD-status` scheme as `v32lua` and `v32c++`,
e.g. `20261002-dev`, `20261015-release`). `v32opt --version` prints it, the
CMake build reads it at configure time (for package names), and
`make version` stamps it into the manual page:

```bash
# after editing the #define by hand:
$ make version

# or let make edit inc/v32opt.h too:
$ make version VERSION=20261015-release

# then rebuild, so the binary reports the new version
$ make
```

`make version` prints the version and the two lines it maintains (the
header's `#define` and the man page's `.TH` line, which also gets the
current month and year).
