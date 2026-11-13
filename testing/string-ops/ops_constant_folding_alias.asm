; constant-folding: a write spelled with the alias (mov SR, 5) is a write of R12: the known value is forgotten, not kept as 300
; MOVS copies CR (R11) words from SR (R12) to DR (R13) and leaves CR=0, SR+=count, DR+=count:
; here R12 is 300 before and 302 after, R13 310 and 312, R11 2 and 0.
; program start section
  call __function_main
  hlt
__function_main:
  mov R12, 300
  mov SR, 41
  mov R0, R12
  mov [400], R0
  mov R13, 310
  mov DR, 51
  mov R1, R13
  mov [401], R1
  ret
