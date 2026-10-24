# v32opt — Vircon32 Assembly Optimizer

[Spanish version / en español](README.es.md) | [French version / en français](README.fr.md)

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


## AI Collaboration

This tool was developed in heavy  collaboration with AI. I am hesitant to
call it  "vibe-coded", as I have  some knowledge and experience  with the
basics (of  assembly, of code  optimization). But, I certainly  would not
have achieved this level  of functionality in so short a  time had AI not
been actively  involved. I feel like  I took more of  a "project manager"
role,  dictating  direction,  troubleshooting problems,  debugging  where
needed.

While I may  not know every inch of  code as well as if  I had personally
wrote it  entirely myself, I  do appreciate  the leverage it  provides in
establishing  new tools  I  can  use in  the  classroom,  share with  the
community, and realize fruition of ideas more quickly.

I know  this may be or  cross a line of  dubious fuzziness/acceptableness
for some. But, as a niche tool in  a niche environment, may it be just as
easy to ignore.

---

## Table of Contents

1. [Build Instructions](#build-instructions)
2. [Integrate into CART build process](#integrate-into-cart-build-process)
3. [Source Languages: C, C++, Lua, and Assembly](#source-languages-c-c-lua-and-assembly)
4. [Usage & Optimization Levels](#usage--optimization-levels)
5. [Phase 1: Local Peephole Optimizations (-O1)](#phase-1-local-peephole-optimizations--o1)
6. [Phase 2: Global Data-Flow & Dead Code (-O2)](#phase-2-global-data-flow--dead-code--o2)
7. [Phase 3: Interprocedural Inlining (-O3)](#phase-3-interprocedural-inlining--o3)
8. [Memory-to-Register Promotion (-O2)](#memory-to-register-promotion--o2)
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
# Build the binary (written to bin/v32opt; bin/v32opt.exe on Windows)
$ make

# Install to your home bin directory (~/bin/bin.<arch> if it exists,
# otherwise ~/bin); "make uninstall" removes it again
$ make install

# Or install system-wide: v32opt to /usr/local/bin and the manual page to
# /usr/local/share/man/man1 (PREFIX=... changes the root; DESTDIR=... is
# honored for staging). "sudo make sysuninstall" removes both.
$ sudo make sysinstall

# ...with the Spanish (es) or French (fr) manual page instead of English
$ sudo make sysinstall DEST_LANG=es

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
| Windows (MSYS2 + MinGW) | `<Program Files>\Vircon32\v32tools\v32opt.exe` | `<Program Files>\Vircon32\v32tools\docs\v32opt\` (`README.md`, `v32opt.1`) |

On  Windows  the  optimizer  is   installed  with  the  default  Vircon32
installation directory tree, under a `v32tools/` directory therein, so be
sure to establish the proper `PATH` entry. As with the DevTools, build it
with MSYS2 + MinGW (Visual C++ lacks `getopt_long`); from a MinGW shell:

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
$ cmake -DDEST_LANG=fr ..                        # French (fr) or Spanish (es) docs
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

### Documentation languages

This README and the manual page are available in English, Spanish and
French: `README.md` / `README.es.md` / `README.fr.md`, and `man/v32opt.1`
/ `man/v32opt.es.1` / `man/v32opt.fr.1` (`man ./man/v32opt.es.1` to read
one in place). Both builds install the English manual page by default;
`DEST_LANG=es` or `DEST_LANG=fr` (`make sysinstall DEST_LANG=es`, `cmake
-DDEST_LANG=es ..`) installs that translation instead, under the plain name
`v32opt.1`, so `man v32opt` simply shows it. Where CMake also installs the
README (Windows), the translated one is installed as `README.md`.

### Prebuilt binaries

The `Generate Global Binaries` GitHub Actions workflow
(`.github/workflows/release-builds.yml`, run by hand from the Actions tab)
builds zipped binaries for Linux (x86_64, arm64, Raspberry Pi armv7/armv8),
macOS (x86_64, arm64) and Windows (x86_64, arm64), plus -- as optional
extras that never hold up a release -- OpenBSD (x86_64, arm64) and Haiku
(x86_64). Each zip holds the binary, the READMEs and the manual pages.
Before building, the workflow checks that the manual pages are stamped
with the current version (see [Releasing](#releasing-version-stamping));
the native builds are smoke-tested (`--version`, and
`testing/long-lines/run.sh`).

A `release` run collects the zips, plus a `SHA256SUMS.txt`, into a GitHub
release `v<VERSION>`, whose text is `.github/release-notes.md` (`@VERSION@`
is replaced by the version). The release is a **draft** until you press
"Publish release" on the Releases page; tick `publish_release` when
starting the run to publish at once. A `-dev` version is marked a
pre-release. Running it again for the same version updates that release.

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
  frame-pointer elimination and  the promote-* passes only  treat a label
  of the form `__function_<name>:` as a function. Code under other labels
  is simply never  considered for removal or inlining --  safe, but those
  passes will find little to do unless you follow the same convention.

* **Anything reached  only through a  computed address is  invisible.** A
  `__function_` routine whose address is only ever formed arithmetically,
  or jumped to via `JMP R0`/`CALL R0` with no `pointer`/operand reference
  to its name,  can be removed by DCE; code  after an unconditional `JMP`
  that  is entered  only by  a  computed jump  (no label)  is removed  by
  `peephole-jumps`.  Referencing the  label  by name  anywhere (`MOV  R0,
  __function_cb`, a `pointer` directive) keeps it alive.

* **Labels  ending  in  `_start`**  are  taken as  loop  headers  by  the
  `promote-loops` pass. The loop's safety is judged over the whole
  `__function_` it sits in (code before the first such label counts as one
  function): a single `MOV R1, BP` or `LEA R1, [BP-N]` anywhere in it keeps
  every loop of that function in memory.

* Assembler directives (`%define`, `%include`,  data directives, ...) are
  never  deleted by  any  pass,  and integer  `%define`  values are  seen
  through unless `-fno-resolve-defines` is given.

### Disassembled CARTs

The Vircon32 `disassemble`  tool names jump and  call targets (`_label1`,
`_label2`, ...),  but every  reference to  *data* in  the program  ROM --
string literals,  tables, initial  values --  stays a  hard-coded address
(`MOV R0,  0x2000002C`). `v32opt`  does not  relocate such  addresses, so
**any optimization that  changes the size of the code  moves the data out
from under them** and the rebuilt  CART reads the wrong memory. (Removing
a  single  4-word  frame  prologue/epilogue  is  enough  to  break  every
string  in  the  program.)  Treat  optimizing  disassembled  code  as  an
experiment:  compare  the original  and  optimized  CARTs carefully,  and
expect only  programs with  no ROM-resident  data references  to survive.
Since disassembled functions are  `_labelN`, not `__function_<name>`, DCE
and inlining also find little to do there.

---

## Usage & Optimization Levels

```bash
v32opt <input.asm> [-o output.asm] [options]
```

| Flag | Description | Included Passes |
| --- | --- | --- |
| `-O0` | **No Optimization** | Disables all optimization passes (default). |
| `-O1` | **Local Peephole** | Enables all 16 local peephole optimizations (14 window passes, plus the liveness-driven **dead register write** and **copy cleanup**). |
| `-O2` | **Global Analysis** | Enables all `-O1` passes + **CSE**, **Dead Code Elimination (DCE)**, **Global Constant Folding**, **Frame Pointer Elimination** & **Memory-to-Register Promotion** (`promote-regs`, `promote-leaf`, `promote-loops`). |
| `-O3` | **Aggressive** | Enables all `-O2` passes + **Function Inlining** + **Loop Inversion** + **Loop Unrolling**. |
| `-Os` | **Space Saving** | `-O3` without loop inversion and unrolling (which duplicate code); a placeholder for a dedicated size-focused tier. |
| `-o <file>` | **Output File** | Where to write the optimized assembly (default: `<input>Opt.asm`). |
| `-v`, `--verbose` | **Verbose Mode** | Displays detailed pass statistics and optimization counts per iteration. |
| `-t` | **Testing Mode** | Prints a machine-readable `pass:count` summary (plus a `total:N` line). |
| `-d` | **Debug Marking** | Marks every optimization inline in the output as `; [DEBUG <pass>] ...` comments. |
| `-L <c\|lua>`, `--langmode` | **Language Mode** | `lua` enables NaN-boxed-type awareness for `v32lua` output; `c` (default) for C, C++ and assembly (see [Source Languages](#source-languages-c-c-lua-and-assembly)). |
| `--dot <file>` | **CFG Export** | Exports the Control Flow Graph to a Graphviz `.dot` file for visualization. |
| `--trigger-max=<N>` | **Bisection Cap** | Global budget on total committed transformations (see below). |
| `-V`, `--version` / `-h`, `--help` | **Info** | Print the version, or a usage summary, and exit. |

`v32opt` is  silent on success  unless `-v` or  `-t` is given,  and exits
with status `1` (and a message on  stderr) for a bad option, a missing or
unreadable input, or an unwritable output. (Input lines can be of any
length.)

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
eliminate  unreachable  code,  fold  constants across  jumps,  strip
redundant overhead, and keep stack locals in registers (the `promote-*`
passes need whole-function knowledge -- which frame addresses escape,
which registers are provably free -- so they belong here, not with the
sliding-window peepholes).

* **`-O3` (Aggressive  Interprocedural Transformations):** Makes sweeping
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
# Example: Run O2 but disable jump chaining and loop register promotion
$ v32opt game.asm -O2 -fno-peephole-jmp-chain -fno-promote-loops
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

Vircon32's conditional jumps  already test a register  against zero (`JT`
jumps when  it is non-zero, `JF`  when it is zero),  so materialising the
comparison first  is redundant. This  pass folds the comparison  into the
jump, both when a copy is tested and when the value is tested in place.

```vircon32
; BEFORE                             ; AFTER
MOV R0, R4
IEQ R0, 0
JT  R0, __slow_path                  JF  R4, __slow_path

INE R5, 0
JF  R5, __is_zero                    JF  R5, __is_zero
```

The  original leaves  the  comparison  result (0  or  1)  in the  scratch
register, so  the rewrite is  only applied  when that register  is proven
never  to be  read again  on  **either** side  of the  branch. The  proof
follows jumps and looks into  called functions; anything it cannot follow
(a computed jump  or call, registers `R11`-`R15`, a callee  that may read
the register)  leaves the  code untouched.  Integer compares  only: `FEQ`
treats `-0.0` as zero, while `JF` tests the raw bits.

v32lua emits  this shape  in every  inline table  lookup, so  on Lua-mode
programs it removes roughly 3% of all instructions.

### Dead Register Writes (`peephole-dead-regs`)

Compiler  output  (and  the  output  of the  promote  passes)  is  full  of
instructions that  only write a  register nobody reads again:  a temporary
copied  into place  and  never  used, a  value overwritten  by  the next
instruction, a comparison result thrown away. This pass removes them.

```vircon32
; BEFORE                             ; AFTER
MOV  R1, R6                          ; (R1 is never read again)
ILT  R0, R6                          ILT  R0, R6
MOV  R0, R4                          ; (R0 is overwritten at the loop top)
JMP  __loop                          JMP  __loop
```

Only instructions whose  sole effect is the register write  are candidates
(`MOV`, `LEA`,  the integer and  float arithmetic, shifts, logic,  the compares
and the conversions): never `IN`/`POP`, the divisions and `MOD`s and the
transcendental functions (they  can raise errors), `MOVS`/`SETS`/`CMPS`,
`CALL`. A  write is removed  only when `liveness.c` -- the  control-flow-aware
query `peephole-zero-test` uses -- proves the register is never read on any
path (it follows jumps  and looks into callees; anything it cannot follow
counts as "read"). `R0` is live at every `RET` (the return value), and so
is any register a  direct caller reads after the call:  v32cc's unsigned-
divide helper returns the remainder in `R1`, and a write to it is not dead.

### Copy Cleanup (`peephole-copies`)

Cancels the register copies  that are left once a value has  been moved
into a register  to be worked on. Two  rewrites, both on straight-line code
(no label, jump or call in between):

```vircon32
; BEFORE                             ; AFTER
MOV  R1, R4                          IADD R4, 1
IADD R1, 1
MOV  R4, R1                          ; (copy in, operate, copy back: R1 dead)

MOV  R1, R6                          ; (R1 now dead: removed)
ILT  R0, R1                          ILT  R0, R6
```

*Coalescing:* `MOV Rt, Rs / ... / MOV Rs, Rt` around a window that never
mentions `Rs`, with `Rt` dead afterwards, is the window renamed onto `Rs`
with both copies gone. *Propagation:* after `MOV Rt, Rs` the reads of `Rt`
up to the next write of either register read `Rs`; the copy then goes if
`Rt` is dead. Together with `peephole-dead-regs` this is what makes
register promotion pay: promotion turns `[BP-2]` into `R4` but leaves a
copy for each use, and these two passes cancel them.

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

Despite its  name, this pass  folds constants *within* a  register rather
than propagating them into other  instructions. It does three things with
integer immediates on a register destination:

* drops identity arithmetic (`IADD`/`ISUB` by `0`, `IMUL`/`IDIV` by `1`);

* folds a  constant `MOV` into  a later `IADD`/`ISUB`/`IMUL` of  the same
  register (never across a read or  write of that register, a conditional
  branch, or a label/jump/call boundary);

* merges adjacent `IADD`/`ISUB` immediates on the same register, removing
  the pair outright when they cancel.

```vircon32
; BEFORE                             ; AFTER
MOV R1, 10                           MOV R1, 15
IADD R1, 5                           ; (Folded into the MOV)

IADD R2, 5                           IADD R2, 2
ISUB R2, 3                           ; (Merged)
```

A  fold whose  result  would not  fit a  32-bit  literal (the  arithmetic
overflowed)  is  left alone.  `MOV  R1,  42` /  `IADD  R2,  R1` is  *not*
rewritten to `IADD R2, 42` -- that would cost a word, not save one.

### Jump Chain Elimination (`peephole-jmp-chain`)

Short-circuits jump indirection. Every `JMP`, `JT` or `JF` -- wherever it
sits  in the  program --  whose target  label leads  straight to  another
unconditional  `JMP <label>`  is  retargeted  to the  end  of the  chain,
however many  hops long.  Once the  last user  of an  intermediate jump's
label has been retargeted, that intermediate jump is removed.

```vircon32
; BEFORE                             ; AFTER
JT  R0, label_step1                  JT  R0, label_final
...                                  ...
JMP label_step1                      JMP label_final
...                                  ...
label_step1:                         label_step1:
JMP label_step2                      ; (Intermediate jump removed)
label_step2:                         label_step2:
JMP label_final                      ; (Intermediate jump removed)
```

"Leads  straight to"  allows  further labels,  comments  and blank  lines
between the label  and its `JMP`; any directive ends  the search. A chain
that loops back on itself (`L: JMP L`) is left alone, and one ending in a
computed jump (`JMP R0`) is followed only as far as its last label.

NOTE: an  intermediate jump is only  removed when nothing else  can reach
it: no label directly above it  is referenced anywhere in the program (as
a  jump or  call target,  by `MOV  R0, label`  taking its  address, by  a
`pointer` directive,  ...), and no code  can fall into it  (the preceding
instruction must itself be an unconditional  transfer; a jump at the very
start of the program is the boot entry point and always stays). Otherwise
it is kept and still routes its remaining users correctly.

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

The re-initializing `MOV  R3, R5` is now  dead (immediately overwritten),
and at `-O2` `peephole-dead-stores` removes it  -- so in a full `-O2` run
every row above saves one more word.

---

#### **Supported Operations**

All arithmetic, logical, and floating-point ops:

`IADD`,  `ISUB`,  `IMUL`, `IDIV`,  `AND`,  `OR`,  `XOR`, `FADD`,  `FSUB`,
`FMUL`, etc. Works with  registers, immediates (including negatives), and
respects:

- ✅ Control flow boundaries (`JMP`, `JT`, `JF`, `CALL`, `RET`, labels)
- ✅ Register modifications between expressions
- ✅ Different operations (e.g., `IADD` ≠ `ISUB`)

### Dead Function Elimination (`dce`)

Performs a reachability analysis starting from known program roots -- the
code  before the  first  function (the  boot  path), `__function_main`  /
`main` /  `_start` / `start` /  `__start`, global-initialization routines
(`__init_globals`, `__function_init`, v32lua's global-scope initializer),
labels containing `ISR`  or `interrupt`, data labels,  and every function
named in  a `pointer` directive.  Any function  whose name appears  as an
operand of reachable code (a `CALL`,  or a `MOV R0, __function_cb` taking
its address)  becomes reachable  in turn; functions  (labels of  the form
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


### Loop Inversion (`loop-invert`)

Both compilers emit top-tested loops, so every iteration executes two
branches: the test's conditional jump and the back edge. Loop inversion
turns the back edge into a copy of the test plus a conditional jump
straight back to the body, saving one jump (one cycle) per iteration:

```vircon32
; BEFORE                          ; AFTER
__for_1_start:                    __for_1_start:
    MOV R0, R1                        MOV R0, R1
    ILT R0, R2                        ILT R0, R2
    JF  R0, __for_1_end               JF  R0, __for_1_end
    ...body...                    __v32opt_loop_1:
    JMP __for_1_start                 ...body...
__for_1_end:                          MOV R0, R1
                                      ILT R0, R2
                                      JT  R0, __v32opt_loop_1
                                  __for_1_end:
```

The top copy of the test stays, so the loop entry and every other jump to
the start (a `continue` in a `while` loop) still see the test first. A loop
that runs zero or one time costs nothing extra; every further iteration
saves a cycle, in exchange for the test's size once (at most 8 instructions).

It is plain tail duplication of the code at the jump target, so no register
is read or written that was not before. A back edge is only rewritten when
the test is straight-line code (no label, `CALL` or other branch inside), the
jump is a backward one, the code right after the jump begins with the test's
exit label (so the fall-through is the loop exit), the labels are defined only
once (the v32cc default `.local` labels repeat per function: use
`--standard`), and the target is not a function entry. It runs once, after the
promote passes (which find a loop by its back edge) and before the final
cleanup. It is part of `-O3`, not `-O2` or `-Os`, because it trades code size
for speed; `-floop-invert` enables it on its own.


### Full Loop Unrolling (`loop-unroll`)

A counted loop whose trip count is a compile-time constant is replaced by
that many copies of its body, with no test and no back edge:

```vircon32
; BEFORE                          ; AFTER (4 trips)
    MOV R4, 0
__for_1_start:                        ...body, R4 = 0...
    MOV R0, R4                        IADD R4, 1
    ILT R0, 4                         ...body, R4 = 1...
    JF  R0, __for_1_end               IADD R4, 1
    ...body...                        ...                (2 more copies)
    IADD R4, 1                    __for_1_end:
    JMP __for_1_start
__for_1_end:
```

Per iteration that removes the test (3 instructions) and the back edge (1).
Every copy keeps its `IADD Ri, step`, so the counter holds the same value
everywhere as before, and in each copy a read of the counter where an
immediate is accepted is replaced by its value (`global_arr + i` becomes an
address the cleanup passes can fold). The loop shape matched is the one the
compilers and `promote-*` leave: `MOV Ri, <imm>` as the last write of `Ri`
before the start label, a test `MOV Rc, Ri / I<cmp> Rc, <imm> / JF Rc, end`,
a body, `IADD/ISUB Ri, <imm>` and `JMP start`.

Soundness: the trip count is computed on the constant init and step (32-bit
wrap-around); no instruction of the body writes the counter and the body has
no `CALL`; the start label is referenced only by the back edge, and every
label of the body only from inside it (jumps out of the body, for `break`
and `return`, are kept; labels of each copy are renamed to stay unique);
`Rc`, the test register, is re-created (`MOV Rc, 1` at the top of each copy,
`MOV Rc, 0` at the normal exit) when liveness says someone reads it. At most
16 trips and 96 instructions in all; nested loops unroll inside-out. It
runs once, after constant folding and before `loop-invert`, is part of `-O3`
(code grows; `-Os` leaves it out) and `-floop-unroll` enables it on its own.

---

## Memory-to-Register Promotion (`-O2`)

The Vircon32 C compiler keeps every local variable in memory
(`[BP-1]`, `[BP-2]`, ...) and goes through a register for each use. Three
passes move frequently used locals into otherwise unused registers
(`R1`-`R10`) for the stretch of code in which that is safe. On its own
that costs the same number of cycles (`MOV R0, [BP-2]` and `MOV R0, R4` take
one each); what pays is that the register copies it leaves can then be
cancelled by the `-O1` passes `peephole-copies` and `peephole-dead-regs`, and
that a register can no longer alias a pointer the way a memory slot can.
All three are part of `-O2` (and so of `-O3` and `-Os`); `-fno-promote-regs`,
`-fno-promote-leaf` and `-fno-promote-loops` leave any of them out.

All three work the same way: pick a local referenced often enough in a
region, rewrite every reference in the region to a register, then add
the load (and, only where the memory copy can still be observed, the
store) at the region's edges.

### Stack Slot Promotion (`promote-leaf` / `promote-regs`)

* `promote-leaf` works on whole **leaf functions** (no `CALL`): the load
  goes in right after the prologue (`MOV BP, SP` / `ISUB SP, N` / saves).

* `promote-regs` applies the same logic to ordinary functions, separately
  within each `CALL`-free stretch (before the first `CALL`, between
  calls, after the last), and only where that stretch is
  single-entry/single-exit. A local written in a stretch is stored back
  just before the `CALL` that ends it if anything reads it afterwards.

```vircon32
; BEFORE                      ; AFTER (promote-regs, then the -O1 cleanup)
__function_f:                 __function_f:
    ...                           ...
    CALL __function_g             CALL __function_g
    MOV R0, [BP-1]                MOV R4, [BP-1]       ; (load once, after the CALL)
    IADD R0, R1                   IADD R4, R1          ; (the copies cancel out)
    MOV [BP-1], R0
    MOV R0, [BP-1]                MOV R0, R4
    IMUL R0, 2                    IMUL R0, 2
    MOV [BP-1], R0
```

Nothing is stored back before `RET`: the frame is dead the moment the
function returns, and no pointer into it can exist (see below).

### Loop-Invariant Register Promotion (`promote-loops`)

Targets `CALL`-free loops (`__for_N_start`, `__while_N_start`, ...) and
promotes the locals referenced inside the body: the load goes in front of
the loop's header label (a true preheader, run once), the store right
after its exit label. Either is left out when it cannot matter: no load
when every path through the loop writes the register before reading it,
no store when nothing outside the loop reads the slot (a `for` counter
that is dead afterwards) -- unless the loop sits inside another one, which
re-enters it and re-reads the slot in the preheader.

```vircon32
; BEFORE                                 ; AFTER
    MOV [BP-2], R0                           MOV [BP-2], R0
__for_1_start:                               MOV R4, [BP-2]       ; (preheader)
    MOV R0, [BP-2]                       __for_1_start:
    ...                                      MOV R0, R4
    MOV R0, [BP-2]                           ...
    IADD R0, 1                               IADD R4, 1           ; (after the cleanup)
    MOV [BP-2], R0                           JMP __for_1_start
    JMP __for_1_start                    __for_1_end:
__for_1_end:                                 ...
    ...
```

References are weighted by loop nesting, so when there are not enough free
registers for every candidate the innermost loops' variables win. A loop
that cannot be promoted (it contains a `CALL`, a `RET`, ...) is skipped
on its own; loops nested inside it are still considered.

### Calling convention: whose registers are they?

A promoted local needs a register, and the registers belong to somebody.
The **stock Vircon32 C compiler is callee-save**: a function pushes every
register it uses (`R1` and up) right after `ISUB SP, N` and pops them before
`MOV SP, BP`, because its callers keep live temporaries in `R1`, `R2`, ...
across a `CALL` -- an expression like `h(f(1), f(2)) + f(3) * (f(4) + ...)`
holds up to eight partial results in registers across calls. A promoted local
in a register the function never saved would silently destroy the caller's
value (an earlier version of these passes did exactly that to stock-compiled
programs). So outside `-L lua`:

* a register the function already saves can be used for free;
* otherwise the pass adds a `PUSH` after the existing ones and a `POP`
  before the existing pops of **every** exit -- if every exit is a
  recognizable `MOV SP, BP / POP BP / RET` epilogue and nothing jumps out
  of the function; and only when the references justify the 2 extra
  instructions per call (`R4`-`R10`; `R2`/`R3` stay out);
* if neither works the local stays in memory.

`-L lua` is **caller-save** (v32lua spills around calls and its callees
clobber freely; `R0`, `R2` and `R3` return values), so nothing needs saving.
v32cc, which also clobbers freely, is handled as the stock compiler: a
needless push/pop pair, but never a wrong result.

### What keeps a slot in memory

These checks look at the **whole function**, not just the region being
promoted: a pointer made before a loop is as capable of aliasing a slot
inside it as one made inside.

* The frame address escapes: `MOV R1, BP`, `LEA R1, [BP-N]` (taking the
  address of a local, a struct, an array), `IADD R1, BP`, ... Likewise
  anything that copies `SP` out (`MOV R1, SP`, `LEA R1, [SP+N]`).
* No `MOV BP, SP` prologue (hand-written or `naked` code has no frame of
  its own to promote slots of).
* A segment or loop that something else branches into or out of: a
  jump from outside to a label inside (a `goto` into a loop body, or to
  the loop's exit label), or from inside to somewhere that would skip the
  store.
* Parameters (`[BP+N]`) are never promoted: a copy of one costs a load
  and saves nothing.

### Choosing the register

A register is only used when it is **provably free**: it does not appear
in the region, and the control-flow-aware liveness query that
`peephole-zero-test` uses (`liveness.c`) shows it is never read
before being written again on any path from the load. `R11`-`R13` double as
the implicit operands of `MOVS`/`SETS`/`CMPS` and are never used.

### What to expect

Over about 190 compiler-generated test programs run in the simulator
(loop- and call-heavy, with address-taken locals; ~175 more through the
stock compiler), cycles / code size against `-O0`:

| | v32cc | stock `compile` |
| --- | --- | --- |
| `-O2` before the promote passes and the copy cleanup | -1.6% / -3.1% | -1.6% / -5.6% |
| `-O1` (now with the cleanup) | -3.4% / -3.8% | -3.0% / -6.1% |
| `-O2` | -6.9% / -6.0% | -4.9% / -14.5% |
| `-O3` | -7.9% / -6.0% | -7.0% / -14.4% |

Most of the `-O2` gain over `-O1` is promotion; individual programs gain
up to 30%. `-d` marks every load and store a pass adds
(`; [DEBUG promote-regs] ...`), and `--trigger-max` counts one slot per
promoted variable, so a suspected miscompile can be bisected like any other
pass.

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

* **Literal Range:**  Every constant fold is computed  wide and abandoned
if  the  result doesn't  fit  a  32-bit  literal the  Vircon32  assembler
accepts, so an overflowing `IMUL`/`IADD` chain is left as written instead
of producing an out-of-range immediate.

*      **Directives      Are      Never      Deleted:**      No      pass
removes     an     assembler    directive     (`%define`,     `%include`,
`integer`/`float`/`string`/`pointer`/`datafile` data), even  when it sits
inside a range of dead code being swept away. A line can be of any length:
the text of every line is kept whole (a long `string` literal or a `pointer`
table of hundreds of functions is one line), and DCE reads a `pointer` line
in full, so a function named only at its far end is still kept.

* **Self-Referential Load Protection:** Redundant move elimination deeply
inspects  indirect memory  loads. Textually  identical instructions  like
`MOV  R1, [R1]`  followed  by  a second  `MOV  R1,  [R1]` are  recognized
as   pointer-dereference  chaining   rather  than   duplicates,  ensuring
pointer-to-pointer lookups evaluate correctly.

* **Address-Taking Guardrails:** Register promotion passes scan the whole
function (not just the loop or segment being promoted) for stack-address
calculations (e.g., `IADD R1, BP`, `LEA R1, [BP-N]`, `MOV R1, SP`). If a
local  variable's  memory  address  is  dynamically  computed  or  taken
anywhere in it, promotion is automatically aborted for that function to
guarantee memory safety and prevent aliasing bugs. Register choice is
backed by a liveness proof (see
[Memory-to-Register Promotion](#memory-to-register-promotion--o2)).

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

The  version lives  in  exactly  one place:  the  `VERSION` `#define`  in
`inc/v32opt.h`  (the  same  `YYYYMMDD-status`   scheme  as  `v32lua`  and
`v32c++`,  e.g. `20261002-dev`,  `20261015-release`). `v32opt  --version`
prints  it, the  CMake  build reads  it at  configure  time (for  package
names), and `make version` stamps it into the manual pages:

```bash
# after editing the #define by hand:
$ make version

# or let make edit inc/v32opt.h too:
$ make version VERSION=20261015-release

# then rebuild, so the binary reports the new version
$ make
```

`make version` prints the version and the lines it maintains: the header's
`#define` and the `.TH` line of each manual page (English, Spanish and
French), which also gets the current month and year, in that page's
language.
