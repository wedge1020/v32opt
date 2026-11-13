; cse: 'mov R0, R12 / iadd R0, 1' computed again after MOVS is a different value
; MOVS copies CR (R11) words from SR (R12) to DR (R13) and leaves CR=0, SR+=count, DR+=count:
; here R12 is 300 before and 302 after, R13 310 and 312, R11 2 and 0.
; program start section
  call __function_main
  hlt
__function_main:
  mov R11, 2
  mov R12, 300
  mov R13, 310
  mov R1, R12
  iadd R1, 1
  mov [400], R1
  movs
  mov R2, R12
  iadd R2, 1
  mov [401], R2
  ret
