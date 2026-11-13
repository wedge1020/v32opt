; hand-written, copied from /home/claude/work-t3/dce/alias.asm: MOVS/SETS/CMPS and the aliases CR/SR/DR (= R11/R12/R13)
  call __function_main
  hlt
__function_main:
  mov R11, 2
  mov CR, 5
  mov R11, 2
  mov R0, R11
  mov [400], R0
  mov R12, 7
  mov R1, 0
  mov R13, 300
  sets
  mov R13, 300
  mov R0, R13
  mov [401], R0
  ret
