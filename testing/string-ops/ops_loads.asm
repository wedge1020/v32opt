; peephole-loads: 'mov R12, [R1]' then a load of the same cell: the second becomes a copy of R12 unless R12 changed; 'mov SR, 41' writes R12 under its alias, so the second load must stay (R3 is 300, not 41).
; program start section
  call __function_main
  hlt
__function_main:
  mov R1, 200
  mov R0, 300
  mov [R1], R0
  mov R12, [R1]
  mov SR, 41
  mov R3, [R1]
  mov [400], R3
  ret
