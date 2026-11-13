; constant-folding and peephole-movs: CMPS (compare CR words at SR and DR) changes R11-R13 like the
; other string instructions (and writes its result into R0), so the values known before it are
; gone after it: the reads of R11, R12, R13 must stay reads of the registers.
; program start section
  call __function_main
  hlt
__function_main:
  mov R11, 2
  mov R12, 300
  mov R13, 310
  cmps R0
  mov R1, R0
  mov [403], R1
  mov R0, R11
  mov [400], R0
  mov R0, R12
  mov [401], R0
  mov R0, R13
  mov [402], R0
  mov R11, 2
  mov R12, 300
  mov R13, 310
  mov R2, R12
  cmps R0
  mov R0, R2
  mov [404], R0
  ret
