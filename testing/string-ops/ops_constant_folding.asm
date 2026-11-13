; constant-folding: R11-R13 are not known constants after MOVS (they were set before it)
; MOVS copies CR (R11) words from SR (R12) to DR (R13) and leaves CR=0, SR+=count, DR+=count:
; here R12 is 300 before and 302 after, R13 310 and 312, R11 2 and 0.
; program start section
  call __function_main
  hlt
__function_main:
  mov R11, 2
  mov R12, 300
  mov R13, 310
  movs
  mov R0, R12
  mov [400], R0
  mov R1, R13
  mov [401], R1
  mov R2, R11
  mov [402], R2
  ret
