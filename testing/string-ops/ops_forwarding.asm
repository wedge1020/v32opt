; peephole-forwarding: R1 is a copy of R12 from before MOVS; after it R1 is 300 and R12 302, so 'mov R0, R1' must not become 'mov R0, R12'
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
  movs
  mov R0, R1
  mov [400], R0
  mov R0, R12
  mov [401], R0
  ret
