; program start section
  call __function_main
  hlt

; location of global variables
  %define global___fails 0
  %define global___checks 1
  %define global___lines 2
  %define global_g 18

__function___check:
  push BP
  mov BP, SP
  mov R0, [global___checks]
  mov R1, R0
  iadd R1, 1
  mov [global___checks], R1
  mov R0, [BP+2]
  jt R0, __if_1_end
  mov R0, [global___fails]
  ilt R0, 16
  jf R0, __if_2_end
  mov R0, [BP+3]
  mov R1, global___lines
  mov R2, [global___fails]
  iadd R1, R2
  mov [R1], R0
__if_2_end:
  mov R0, [global___fails]
  mov R1, R0
  iadd R1, 1
  mov [global___fails], R1
__if_1_end:
__function___check_return:
  mov SP, BP
  pop BP
  ret

__function_f:
  push BP
  mov BP, SP
  mov R0, [BP+2]
  mov R1, [BP+3]
  iadd R0, R1
__function_f_return:
  mov SP, BP
  pop BP
  ret

__function_h:
  push BP
  mov BP, SP
  isub SP, 5
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
__for_3_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_3_end
  mov R0, [BP-1]
  mov R1, [BP-2]
  iadd R0, R1
  mov [BP-1], R0
  mov R0, [BP-1]
  mov R1, [BP-2]
  iadd R0, R1
  mov [BP-1], R0
__for_3_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_3_start
__for_3_end:
  mov R0, [BP-1]
  mov [BP-3], R0
  mov R1, [BP-1]
  mov [SP], R1
  mov R1, 3
  mov [SP+1], R1
  call __function_f
  mov R1, R0
  mov R0, [BP-3]
  iadd R0, R1
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
__for_4_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_4_end
  mov R0, [BP-1]
  mov R1, [BP-2]
  iadd R0, R1
  mov [BP-1], R0
  mov R0, [BP-1]
  mov R1, [BP-2]
  iadd R0, R1
  mov [BP-1], R0
__for_4_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_4_start
__for_4_end:
  mov R0, [BP-1]
__function_h_return:
  mov SP, BP
  pop BP
  ret

__function_main:
  push BP
  mov BP, SP
  isub SP, 3
  mov R5, 1234          ; hand-written caller: keeps R5 across the call (callee-save ABI)
  mov R0, 5
  mov [SP], R0
  call __function_h
  mov [2000], R5
  ieq R0, 63
  jf R0, __cond_else_0
  mov R0, 1
  jmp __cond_end_1
__cond_else_0:
  mov R0, 0
__cond_end_1:
  mov [BP-1], R0
  mov R0, 5
  mov [SP+1], R0
  mov R0, [BP-1]
  mov [SP], R0
  call __function___check
  mov R0, 0
__function_main_return:
  mov SP, BP
  pop BP
  ret

