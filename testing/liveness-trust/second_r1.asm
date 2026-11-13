; copied from /home/claude/work-t3/trust/second_r1.asm (hand-written; no ;@hints header: the strict rule for open classes)
; hand-written: __function_f returns a second result in R1 to a caller that reaches it only
; through "call R2" (no direct CALL, so the class is open). No ';@hints' header: strict rule.
  call __function_main
  hlt

__function_f:
  push BP
  mov BP, SP
  mov R0, 7
  mov R1, 9
  mov SP, BP
  pop BP
  ret

__function_g:
  push BP
  mov BP, SP
  mov R0, [BP+2]
  iadd R0, 1
  mov R1, R0
  imul R1, 3
  mov SP, BP
  pop BP
  ret

__function_main:
  push BP
  mov BP, SP
  isub SP, 1
  mov R2, __function_f
  call R2
  mov [200], R0
  mov [201], R1
  mov R0, 5
  mov [SP], R0
  mov R3, __function_g
  call R3
  mov [202], R0
  mov [203], R1
  mov SP, BP
  pop BP
  ret
