; program start section
  call __function_main
  hlt

; location of global variables
  %define global___fails 0
  %define global___checks 1
  %define global___lines 2
  %define global_arr 18

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

__function_clear16:
  push BP
  mov BP, SP
  isub SP, 1
  mov R0, 0
  mov [BP-1], R0
__for_3_start:
  mov R0, [BP-1]
  ilt R0, 16
  jf R0, __for_3_end
  mov R0, 0
  mov R1, global_arr
  mov R2, [BP-1]
  iadd R1, R2
  mov [R1], R0
__for_3_continue:
  mov R0, [BP-1]
  mov R1, R0
  iadd R1, 1
  mov [BP-1], R1
  jmp __for_3_start
__for_3_end:
__function_clear16_return:
  mov SP, BP
  pop BP
  ret

__function_set4:
  push BP
  mov BP, SP
  isub SP, 1
  mov R0, 0
  mov [BP-1], R0
__for_4_start:
  mov R0, [BP-1]
  ilt R0, 4
  jf R0, __for_4_end
  mov R0, [BP+2]
  mov R1, global_arr
  mov R2, [BP-1]
  iadd R2, 20
  iadd R1, R2
  mov [R1], R0
__for_4_continue:
  mov R0, [BP-1]
  mov R1, R0
  iadd R1, 1
  mov [BP-1], R1
  jmp __for_4_start
__for_4_end:
__function_set4_return:
  mov SP, BP
  pop BP
  ret

__function_main:
  push BP
  mov BP, SP
  isub SP, 2
  call __function_clear16
  mov R0, 7
  mov [SP], R0
  call __function_set4
  mov R0, [21]
  ieq R0, 0
  jf R0, __cond_else_0
  mov R0, 1
  jmp __cond_end_1
__cond_else_0:
  mov R0, 0
__cond_end_1:
  mov [SP], R0
  mov R0, 5
  mov [SP+1], R0
  call __function___check
  mov R0, [40]
  ieq R0, 7
  jf R0, __cond_else_2
  mov R0, 1
  jmp __cond_end_3
__cond_else_2:
  mov R0, 0
__cond_end_3:
  mov [SP], R0
  mov R0, 5
  mov [SP+1], R0
  call __function___check
  mov R0, 0
__function_main_return:
  mov SP, BP
  pop BP
  ret

