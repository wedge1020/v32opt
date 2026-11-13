; peephole-compiler-myopia: 'mov [R1], R12 / ... / mov R12, [R1]' drops the reload when R12 is not changed in between; 'mov SR, 41' is a write of R12 under its alias (SR = R12), so the reload must stay (R12 is 300 again afterwards).
; program start section
  call __function_main
  hlt
__function_main:
  mov R1, 200
  mov R12, 300
  mov [R1], R12
  mov SR, 41
  mov R12, [R1]
  mov R0, R12
  mov [400], R0
  ret
