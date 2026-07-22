# v32opt — Vircon32 Compiler-Generated Assembly Language Output Optimizer

`v32opt`  is  a  lightweight,  multi-pass peephole  and  static  analysis
optimizer for  V32 assembly code.  Written in C, `v32opt`  takes standard
assembly  (`.asm`)  source  files,   scans  the  instruction  stream  for
inefficient code  patterns, and emits clean,  optimized assembly suitable
for direct assembly and linking.

Whether you are writing assembly by hand or optimizing compiler-generated
assembly  output,  `v32opt`  reduces  binary  size  and  cycle  count  by
eliminating  redundant  operations,  performing strength  reduction,  and
stripping dead code.

---

## Compilation

`v32opt` is written in standard C99 with minimal external dependencies.

### Standard Build (GCC / Clang)

To compile `v32opt` using `gcc` or `clang`:

```bash
# Using GCC
$ gcc -O2 -Wall -std=c99 src/*.c -o v32opt

# Using Clang
$ clang -O2 -Wall -std=c99 src/*.c -o v32opt
```

### Using Makefile (if provided)

```bash
make
```

To clean build artifacts:

```bash
make clean
```

---

## 🚀 Usage

`v32opt`  runs  as a  CLI  utility  taking  an  input assembly  file  and
generating an optimized output file.

```bash
./v32opt <input.asm> [output.asm]
```

### Options & Arguments

| Argument | Description |
| --- | --- |
| `<input.asm>` | **(Required)** Path to the source assembly file. |
| `[output.asm]` | *(Optional)* Output file path. Defaults to `<input>Opt.asm` if omitted. |

### Example

```bash
# Optimizes game.asm and writes output to gameOpt.asm
./v32opt game.asm

# Explicit output file destination
./v32opt game.asm build/game_optimized.asm
```

---

## ⚡ Supported Optimizations

`v32opt` operates  as a  **peephole optimizer**,  sliding a  small window
over the  instruction stream to replace  inefficient instruction patterns
with faster, equivalent sequences.

### 1. Redundant Move & Self-Assignment Elimination

Removes  redundant register-to-register  moves or  self-assignments where
the source and destination registers are identical.

* **C Analogy:** `x = x;`

```nasm
; --- Before ---
MOV R0, R0
MOV [global_ship], R1
MOV [global_ship], R1

; --- After ---
MOV [global_ship], R1
```

---

### 2. Dead Code Elimination (Unreachable Code)

Strips  instructions   following  unconditional   control-flow  transfers
(`JMP`,  `RET`, `BRA`)  until  a valid  jump label  or  branch target  is
encountered.

* **C Analogy:** Code placed  directly after an unconditional `return` or
`break`.

```nasm
; --- Before ---
    RET
    MOV R0, 10      ; Unreachable
    ADD R1, R2      ; Unreachable
.label_next:
    MOV R0, 1

; --- After ---
    RET
.label_next:
    MOV R0, 1
```

---

### 3. Algebraic Simplification

Replaces identity  operations (like  adding zero  or multiplying  by one)
with  `NOP`s  or omits  them  entirely.  Replaces register  zeroing  with
cheaper idioms.

* **C Analogy:** `x = x + 0;` or `x = 0;`

```nasm
; --- Before ---
ADD R0, 0
MUL R1, 1
MOV R2, 0

; --- After ---
; (ADD R0, 0 removed)
; (MUL R1, 1 removed)
XOR R2, R2          ; Cheaper zeroing operation
```

---

### 4. Strength Reduction

Replaces arithmetic operations  that consume high CPU  clock cycles (such
as  integer multiplication  or division  by power-of-two  constants) with
cheaper bitwise shifts.

* **C Analogy:** `x * 8` $\rightarrow$ `x << 3`

```nasm
; --- Before ---
MUL R0, 8
DIV R1, 4

; --- After ---
SHL R0, 3
SHR R1, 2
```

---

### 5. Stack Push/Pop Folding

Folds redundant push/pop operations across adjacent stack operations into
direct register moves or eliminates push/pop pairs that cancel each other
out.

* **C Analogy:** Saving a variable  to a temporary stack variable only to
immediately load it back into a different register.

```nasm
; --- Before ---
PUSH R0
POP  R1

; --- After ---
MOV R1, R0
```
