; FIXED in 1aacb50 (was a known wrong result; run by testing/string-ops/run.sh): peephole-movs deleted the second set of
; R11/R12/R13 because MOVS looked like it writes none of them (modifies_register() ignored the
; implicit R11-R13). The unoptimized program stores 300 and 310; the broken optimizer, with
; -fpeephole-movs (and so -O1 and up), stored 302 and 312. Present in the baseline optimizer too.
; Check:  cd testing/differential && STRICT=1 python3 run.py --flags "-fpeephole-movs" ../string-ops/known_peephole_movs.asm
; program start section
  call __function_main
  hlt
__function_main:
  mov R11, 2
  mov R12, 300
  mov R13, 310
  movs
  mov R11, 2
  mov R12, 300
  mov R13, 310
  mov R0, R12
  mov [400], R0
  mov R0, R13
  mov [401], R0
  ret
