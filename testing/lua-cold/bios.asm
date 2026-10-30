; Minimal BIOS for the differential tests (tests/run_gaps.sh).
;
; Word 0 is where a hardware error lands (the CPU reports it in R0-R3), the program
; starts at word 4 and hands over to the cartridge. The error handler stores the whole
; register file where the test can read it:
;
;   RAM[0x100 + n]   Rn at the moment of the error (R0..R13)
;   RAM[0x10E]       TIM_FrameCounter, RAM[0x10F] TIM_CycleCounter (both a few cycles in)
;   RAM[0x110]       number of times the handler has run
;
; and then halts, which ends both emulators' runs.
  jmp __error       ; words 0-1
  hlt               ; words 2-3 keep the entry point at word 4
  hlt
  jmp 0x20000000    ; word 4: the cartridge

__error:
  mov [0x100], R0
  mov [0x101], R1
  mov [0x102], R2
  mov [0x103], R3
  mov [0x104], R4
  mov [0x105], R5
  mov [0x106], R6
  mov [0x107], R7
  mov [0x108], R8
  mov [0x109], R9
  mov [0x10A], R10
  mov [0x10B], R11
  mov [0x10C], R12
  mov [0x10D], R13
  in  R0, TIM_FrameCounter
  mov [0x10E], R0
  in  R0, TIM_CycleCounter
  mov [0x10F], R0
  mov R0, [0x110]
  iadd R0, 1
  mov [0x110], R0
  hlt
