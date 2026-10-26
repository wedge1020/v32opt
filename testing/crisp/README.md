# crisp-game-lib as a v32opt test

`fuzz.patch` (apply in crisp's `src/`, with `patch -p0` style paths adjusted) adds a `-DFUZZ` mode that
needs no gamepad: it feeds pseudo-random buttons, starts each game in turn for `FUZZ_PER_GAME` frames,
keeps a checksum of the game state (`global_fuzz_sum`, one entry per game in `global_fuzz_log`) and
the CPU cycles used per frame (`global_fuzz_cycles`). The random seed is fixed.

    v32cc --standard --keep-data --data-base 0 -w -DFUZZ -DFUZZ_HOLD=4 -DFUZZ_PER_GAME=60 \
          -DFUZZ_STOP=9999 -DFUZZ_FIRST=0 [--emit-hints] src/main.c -o f.asm
    v32opt f.asm -O3 -fpromote-regs -o fo.asm        # then v32as --standard, wrap as .v32
    v32sim -r -x -n -B bios.v32 -E 0x20000000 --frame-us=0 --max-frames=16100 --seed=1 \
           --fixed-clock --dump=303@0xDE603 fo.v32   # (cycles, checksum, per-game log)

Compare the checksum/log of the optimized build with the `-O0` one. A first-divergent-game index plus
`FUZZ_FIRST/FUZZ_STOP` isolates a game; `v32opt --trigger-max=N` bisects the pass. This found the
31-character operand-name bug (testing/long-names).

Caveat: crisp is timing-sensitive. Adding a pure delay loop to the *unoptimized* build changes the
checksum of the last games, so a late divergence is not necessarily an optimizer bug.
