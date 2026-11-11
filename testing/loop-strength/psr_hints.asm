;@hints 1 abi=caller-save
; program start section
  call __function_main
  hlt

; location of global variables
  %define global___fails 0
  %define global___checks 1
  %define global___lines 2
  %define global_arr 18
  %define global_brr 82
  %define global_sts 146

;@fn __function___check leaf escapes=none
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

;@fn __function_g1 leaf escapes=none
__function_g1:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, [BP+2]
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
__for_3_start:
  mov R0, [BP-2]
  ilt R0, 3
  jf R0, __for_3_end
  mov R0, [BP-1]
  mov R1, [BP-2]
  mov R2, [BP+2]
  imul R1, R2
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
  mov R1, 3
  mov R2, [BP+2]
  imul R1, R2
  isub R0, R1
  iadd R0, 1
  mov R1, [BP+2]
  imul R1, 0
  isub R0, R1
__function_g1_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_f_unit leaf escapes=none
__function_f_unit:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
__for_4_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_4_end
  mov R0, [BP-1]
  mov R1, global_arr
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
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
__function_f_unit_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_f_value leaf escapes=none
__function_f_value:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
__for_5_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_5_end
  mov R0, [BP-1]
  mov R1, global_arr
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  mov R2, [BP-2]
  imul R1, R2
  iadd R0, R1
  mov [BP-1], R0
__for_5_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_5_start
__for_5_end:
  mov R0, [BP-1]
__function_f_value_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_f_scaled leaf escapes=none
__function_f_scaled:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
__for_6_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_6_end
  mov R0, [BP-1]
  mov R1, global_sts
  mov R2, [BP-2]
  imul R2, 3
  iadd R1, R2
  mov R1, [R1+1]
  iadd R0, R1
  mov [BP-1], R0
__for_6_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_6_start
__for_6_end:
  mov R0, [BP-1]
__function_f_scaled_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_f_frame leaf escapes=-16..-1
__function_f_frame:
  push BP
  mov BP, SP
  isub SP, 18
  mov R0, 0
  mov [BP-17], R0
__for_7_start:
  mov R0, [BP-17]
  ilt R0, 16
  jf R0, __for_7_end
  mov R0, [BP-17]
  imul R0, 2
  lea R1, [BP-16]
  mov R2, [BP-17]
  iadd R1, R2
  mov [R1], R0
__for_7_continue:
  mov R0, [BP-17]
  mov R1, R0
  iadd R1, 1
  mov [BP-17], R1
  jmp __for_7_start
__for_7_end:
  mov R0, 0
  mov [BP-17], R0
  mov R0, 0
  mov [BP-18], R0
__for_8_start:
  mov R0, [BP-18]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_8_end
  mov R0, [BP-17]
  lea R1, [BP-16]
  mov R2, [BP-18]
  iadd R1, R2
  mov R1, [R1]
  iadd R0, R1
  mov [BP-17], R0
__for_8_continue:
  mov R0, [BP-18]
  mov R1, R0
  iadd R1, 1
  mov [BP-18], R1
  jmp __for_8_start
__for_8_end:
  mov R0, [BP-17]
__function_f_frame_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_f_two leaf escapes=none
__function_f_two:
  push BP
  mov BP, SP
  isub SP, 1
  mov R0, 0
  mov [BP-1], R0
__for_9_start:
  mov R0, [BP-1]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_9_end
  mov R0, global_arr
  mov R1, [BP-1]
  iadd R0, R1
  mov R0, [R0]
  iadd R0, 1
  mov R1, global_brr
  mov R2, [BP-1]
  iadd R1, R2
  mov [R1], R0
__for_9_continue:
  mov R0, [BP-1]
  mov R1, R0
  iadd R1, 1
  mov [BP-1], R1
  jmp __for_9_start
__for_9_end:
__function_f_two_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_f_down leaf escapes=none
__function_f_down:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov R0, [BP+2]
  isub R0, 1
  mov [BP-2], R0
__for_10_start:
  mov R0, [BP-2]
  ige R0, 0
  jf R0, __for_10_end
  mov R0, [BP-1]
  mov R1, global_arr
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  mov R2, [BP-2]
  imul R1, R2
  iadd R0, R1
  mov [BP-1], R0
__for_10_continue:
  mov R0, [BP-2]
  mov R1, R0
  isub R1, 1
  mov [BP-2], R1
  jmp __for_10_start
__for_10_end:
  mov R0, [BP-1]
__function_f_down_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_f_step2 leaf escapes=none
__function_f_step2:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
__for_11_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_11_end
  mov R0, [BP-1]
  mov R1, global_arr
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  iadd R0, R1
  mov [BP-1], R0
__for_11_continue:
  mov R0, [BP-2]
  iadd R0, 2
  mov [BP-2], R0
  jmp __for_11_start
__for_11_end:
  mov R0, [BP-1]
__function_f_step2_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_f_offset leaf escapes=none
__function_f_offset:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
__for_12_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_12_end
  mov R0, [BP-1]
  mov R1, global_arr
  mov R2, [BP-2]
  iadd R2, 3
  iadd R1, R2
  mov R1, [R1]
  iadd R0, R1
  mov [BP-1], R0
__for_12_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_12_start
__for_12_end:
  mov R0, [BP-1]
__function_f_offset_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_r1_shape leaf escapes=none
__function_r1_shape:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
__for_13_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_13_end
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  mov R0, [BP-1]
  mov R1, global_arr
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  iadd R0, R1
  mov [BP-1], R0
__for_13_continue:
  jmp __for_13_start
__for_13_end:
  mov R0, [BP-1]
__function_r1_shape_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_r2_call escapes=none
__function_r2_call:
  push BP
  mov BP, SP
  isub SP, 5
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
__for_14_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_14_end
  mov R0, [BP-1]
  mov R1, global_arr
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  mov [BP-3], R0
  mov [BP-4], R1
  mov R2, [BP-2]
  mov [SP], R2
  call __function_g1
  mov R2, R0
  mov R0, [BP-3]
  mov R1, [BP-4]
  iadd R1, R2
  iadd R0, R1
  mov [BP-1], R0
__for_14_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_14_start
__for_14_end:
  mov R0, [BP-1]
__function_r2_call_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_r3_counter leaf escapes=none
__function_r3_counter:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
__for_15_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_15_end
  mov R0, [BP-1]
  mov R1, global_arr
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  iadd R0, R1
  mov [BP-1], R0
  mov R0, global_arr
  mov R1, [BP-2]
  iadd R0, R1
  mov R0, [R0]
  ieq R0, 5
  jf R0, __if_16_end
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
__if_16_end:
__for_15_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_15_start
__for_15_end:
  mov R0, [BP-1]
__function_r3_counter_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_r4_goto leaf escapes=none
__function_r4_goto:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 1
  mov [BP-2], R0
  mov R0, [BP+2]
  igt R0, 3
  jf R0, __if_17_end
  jmp __user_label_19
__if_17_end:
  mov R0, 0
  mov [BP-2], R0
__for_18_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_18_end
__user_label_19:
  mov R0, [BP-1]
  mov R1, global_arr
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  iadd R0, R1
  mov [BP-1], R0
__for_18_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_18_start
__for_18_end:
  mov R0, [BP-1]
__function_r4_goto_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_r8_base leaf escapes=none
__function_r8_base:
  push BP
  mov BP, SP
  isub SP, 3
  mov R0, global_arr
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
  mov R0, 0
  mov [BP-3], R0
__for_20_start:
  mov R0, [BP-3]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_20_end
  mov R0, [BP-2]
  mov R1, [BP-1]
  mov R2, [BP-3]
  iadd R1, R2
  mov R1, [R1]
  iadd R0, R1
  mov [BP-2], R0
  mov R0, [BP-3]
  ieq R0, 2
  jf R0, __if_21_end
  mov R0, global_brr
  mov [BP-1], R0
__if_21_end:
__for_20_continue:
  mov R0, [BP-3]
  mov R1, R0
  iadd R1, 1
  mov [BP-3], R1
  jmp __for_20_start
__for_20_end:
  mov R0, [BP-2]
__function_r8_base_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_main escapes=none
__function_main:
  push BP
  mov BP, SP
  isub SP, 4
  mov R0, 0
  mov [BP-1], R0
__for_22_start:
  mov R0, [BP-1]
  ilt R0, 64
  jf R0, __for_22_end
  mov R0, [BP-1]
  mov R1, global_arr
  mov R2, [BP-1]
  iadd R1, R2
  mov [R1], R0
  mov R0, 100
  mov R1, [BP-1]
  iadd R0, R1
  mov R1, global_brr
  mov R2, [BP-1]
  iadd R1, R2
  mov [R1], R0
__for_22_continue:
  mov R0, [BP-1]
  mov R1, R0
  iadd R1, 1
  mov [BP-1], R1
  jmp __for_22_start
__for_22_end:
  mov R0, 0
  mov [BP-1], R0
__for_23_start:
  mov R0, [BP-1]
  ilt R0, 16
  jf R0, __for_23_end
  mov R0, 1
  mov R1, global_sts
  mov R2, [BP-1]
  imul R2, 3
  iadd R1, R2
  mov [R1], R0
  mov R0, [BP-1]
  imul R0, 10
  mov R1, global_sts
  mov R2, [BP-1]
  imul R2, 3
  iadd R1, R2
  mov [R1+1], R0
  mov R0, 2
  mov R1, global_sts
  mov R2, [BP-1]
  imul R2, 3
  iadd R1, R2
  mov [R1+2], R0
__for_23_continue:
  mov R0, [BP-1]
  mov R1, R0
  iadd R1, 1
  mov [BP-1], R1
  jmp __for_23_start
__for_23_end:
  mov R0, 10
  mov [SP], R0
  call __function_f_unit
  ieq R0, 45
  jf R0, __cond_else_0
  mov R0, 1
  jmp __cond_end_1
__cond_else_0:
  mov R0, 0
__cond_end_1:
  mov [BP-2], R0
  mov R0, 27
  mov [SP+1], R0
  mov R0, [BP-2]
  mov [SP], R0
  call __function___check
  mov R0, 4
  mov [SP], R0
  call __function_f_value
  ieq R0, 14
  jf R0, __cond_else_2
  mov R0, 1
  jmp __cond_end_3
__cond_else_2:
  mov R0, 0
__cond_end_3:
  mov [BP-2], R0
  mov R0, 28
  mov [SP+1], R0
  mov R0, [BP-2]
  mov [SP], R0
  call __function___check
  mov R0, 4
  mov [SP], R0
  call __function_f_scaled
  ieq R0, 60
  jf R0, __cond_else_4
  mov R0, 1
  jmp __cond_end_5
__cond_else_4:
  mov R0, 0
__cond_end_5:
  mov [BP-2], R0
  mov R0, 29
  mov [SP+1], R0
  mov R0, [BP-2]
  mov [SP], R0
  call __function___check
  mov R0, 5
  mov [SP], R0
  call __function_f_frame
  ieq R0, 20
  jf R0, __cond_else_6
  mov R0, 1
  jmp __cond_end_7
__cond_else_6:
  mov R0, 0
__cond_end_7:
  mov [BP-2], R0
  mov R0, 30
  mov [SP+1], R0
  mov R0, [BP-2]
  mov [SP], R0
  call __function___check
  mov R0, 8
  mov [SP], R0
  call __function_f_two
  mov R0, [89]
  ieq R0, 8
  jf R0, __cond_else_8
  mov R0, 1
  jmp __cond_end_9
__cond_else_8:
  mov R0, 0
__cond_end_9:
  mov [SP], R0
  mov R0, 32
  mov [SP+1], R0
  call __function___check
  mov R0, [90]
  ieq R0, 108
  jf R0, __cond_else_10
  mov R0, 1
  jmp __cond_end_11
__cond_else_10:
  mov R0, 0
__cond_end_11:
  mov [SP], R0
  mov R0, 33
  mov [SP+1], R0
  call __function___check
  mov R0, 5
  mov [SP], R0
  call __function_f_down
  ieq R0, 30
  jf R0, __cond_else_12
  mov R0, 1
  jmp __cond_end_13
__cond_else_12:
  mov R0, 0
__cond_end_13:
  mov [BP-2], R0
  mov R0, 34
  mov [SP+1], R0
  mov R0, [BP-2]
  mov [SP], R0
  call __function___check
  mov R0, 7
  mov [SP], R0
  call __function_f_step2
  ieq R0, 12
  jf R0, __cond_else_14
  mov R0, 1
  jmp __cond_end_15
__cond_else_14:
  mov R0, 0
__cond_end_15:
  mov [BP-2], R0
  mov R0, 35
  mov [SP+1], R0
  mov R0, [BP-2]
  mov [SP], R0
  call __function___check
  mov R0, 3
  mov [SP], R0
  call __function_f_offset
  ieq R0, 12
  jf R0, __cond_else_16
  mov R0, 1
  jmp __cond_end_17
__cond_else_16:
  mov R0, 0
__cond_end_17:
  mov [BP-2], R0
  mov R0, 36
  mov [SP+1], R0
  mov R0, [BP-2]
  mov [SP], R0
  call __function___check
  mov R0, 3
  mov [SP], R0
  call __function_r1_shape
  ieq R0, 6
  jf R0, __cond_else_18
  mov R0, 1
  jmp __cond_end_19
__cond_else_18:
  mov R0, 0
__cond_end_19:
  mov [BP-2], R0
  mov R0, 37
  mov [SP+1], R0
  mov R0, [BP-2]
  mov [SP], R0
  call __function___check
  mov R0, 3
  mov [SP], R0
  call __function_r2_call
  ieq R0, 9
  jf R0, __cond_else_20
  mov R0, 1
  jmp __cond_end_21
__cond_else_20:
  mov R0, 0
__cond_end_21:
  mov [BP-2], R0
  mov R0, 38
  mov [SP+1], R0
  mov R0, [BP-2]
  mov [SP], R0
  call __function___check
  mov R0, 8
  mov [SP], R0
  call __function_r3_counter
  ieq R0, 22
  jf R0, __cond_else_22
  mov R0, 1
  jmp __cond_end_23
__cond_else_22:
  mov R0, 0
__cond_end_23:
  mov [BP-2], R0
  mov R0, 39
  mov [SP+1], R0
  mov R0, [BP-2]
  mov [SP], R0
  call __function___check
  mov R0, 5
  mov [SP], R0
  call __function_r4_goto
  ieq R0, 10
  jf R0, __cond_else_24
  mov R0, 1
  jmp __cond_end_25
__cond_else_24:
  mov R0, 0
__cond_end_25:
  mov [BP-2], R0
  mov R0, 40
  mov [SP+1], R0
  mov R0, [BP-2]
  mov [SP], R0
  call __function___check
  mov R0, 2
  mov [SP], R0
  call __function_r4_goto
  ieq R0, 1
  jf R0, __cond_else_26
  mov R0, 1
  jmp __cond_end_27
__cond_else_26:
  mov R0, 0
__cond_end_27:
  mov [BP-2], R0
  mov R0, 41
  mov [SP+1], R0
  mov R0, [BP-2]
  mov [SP], R0
  call __function___check
  mov R0, 5
  mov [SP], R0
  call __function_r8_base
  ieq R0, 12
  jf R0, __cond_else_28
  mov R0, 1
  jmp __cond_end_29
__cond_else_28:
  mov R0, 0
__cond_end_29:
  mov [BP-2], R0
  mov R0, 42
  mov [SP+1], R0
  mov R0, [BP-2]
  mov [SP], R0
  call __function___check
  mov R0, 0
__function_main_return:
  mov SP, BP
  pop BP
  ret

