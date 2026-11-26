**v32opt** is a multi-pass assembly optimizer for [Vircon32](https://www.vircon32.com). It reads the assembly that your compiler writes (or that you write by hand), makes it smaller and faster, and writes assembly back out for the official assembler to pack into a cartridge.

## Highlights

- **Works on what you already build.** C with the Vircon32 DevTools, C++ through `v32c++`, Lua through `v32lua`, and hand-written assembly. `-L lua` makes the value-tracking passes aware of v32lua's NaN-boxed values.
- **Three tiers, so you choose the risk.** `-O1` runs 14 local peephole passes. `-O2` adds common-subexpression elimination, dead-function elimination, global constant folding and frame-pointer elimination. `-O3` adds function inlining. Every pass can also be switched on or off with `-f<name>` / `-fno-<name>`.
- **Experimental register promotion.** Stack slots and loop values can be promoted into registers (`-fpromote-leaf`, `-fpromote-loops`); these are not part of any `-O` level.
- **Made for bisecting.** `-d` marks every transformation in the output as a `; [DEBUG <pass>]` comment, `--trigger-max=N` stops after the Nth transformation, and `-finline-call-limit` / `-finline-exclude` narrow down inlining, so a miscompile can be traced to the one change that caused it.
- **Careful with your data.** Directives and data lines are never deleted or altered, input lines can be any length, folded constants that would not fit a 32-bit literal are left alone, and float literals are never mistaken for integers.
- **Checked against the simulator.** The optimizer's passes are exercised by a differential harness (unoptimized against optimized output at `-O1` to `-Os`), and this release fixes a family of liveness bugs found on optimized v32lua output (a pushed register whose stack word is read as data is no longer treated as a save).
- **See the control flow.** `--dot` exports the control flow graph for Graphviz.
- **Documented.** A manual page, and the README, in English, Spanish and French.

## Downloads

Pick the zip for your system: `v32opt-@VERSION@-<os>-<arch>.zip`.

| System | Architectures |
| --- | --- |
| Windows | x86_64, arm64 |
| macOS | x86_64 (Intel), arm64 (Apple silicon) |
| Linux | x86_64, arm64, armv7 and armv8 (Raspberry Pi) |
| OpenBSD, Haiku | included when their (optional) builds succeeded |

Each zip unpacks straight into your `v32tools` folder (`/usr/local/Vircon32/v32tools`, or `C:\Program Files\Vircon32\v32tools` on Windows), beside v32lua and v32c++: `v32opt` at the top, `LICENSE`, and under `docs/v32opt/` the README in each language, `USAGE.md`, the technical notes (`doc/`) and the manual pages (`v32opt.1`, `v32opt.es.1`, `v32opt.fr.1`). Keep that folder on your `PATH`. Check a download against `SHA256SUMS.txt`.

## Quick start

```sh
v32opt game.asm -O1 -o gameOpt.asm     # local peephole passes only: the safest tier
v32opt game.asm -O2 -o gameOpt.asm -v  # add the global passes, and report what changed
```

Then assemble `gameOpt.asm` as usual and compare it with the original; the optimized assembly should be smaller. `-O3` is aggressive, so test the cartridge it produces before you ship it.

See the [README](https://github.com/wedge1020/v32opt#readme) for every pass and option, and the `ISSUES` file for the known limitations. Bug reports and feature requests are welcome in the issues.
