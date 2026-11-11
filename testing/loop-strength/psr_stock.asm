; program start section
  call __global_scope_initialization
  call __function_main
  hlt

; location of global variables
  %define global___fails 0
  %define global___checks 1
  %define global___lines 2
  %define global_arr 18
  %define global_brr 82
  %define global_sts 146

__global_scope_initialization:
  push BP
  mov BP, SP
  mov SP, BP
  pop BP
  ret

__function___check:
  push BP
  mov BP, SP
  mov R0, [global___checks]
  mov R1, R0
  iadd R1, 1
  mov [global___checks], R1
__if_13_start:
  mov R0, [BP+2]
  bnot R0
  jf R0, __if_13_end
__if_17_start:
  mov R0, [global___fails]
  ilt R0, 16
  jf R0, __if_17_end
  mov R0, [BP+3]
  mov R1, global___lines
  mov R2, [global___fails]
  iadd R1, R2
  mov [R1], R0
__if_17_end:
  mov R0, [global___fails]
  mov R1, R0
  iadd R1, 1
  mov [global___fails], R1
__if_13_end:
__function___check_return:
  mov SP, BP
  pop BP
  ret

__function_error_handler:
  push BP
  mov BP, SP
  mov R0, 9999
  mov [global___fails], R0
__function_error_handler_return:
  mov SP, BP
  pop BP
  ret

__function_g1:
  push BP
  mov BP, SP
  isub SP, 2
  push R1
  push R2
  mov R0, [BP+2]
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
__for_47_start:
  mov R0, [BP-2]
  ilt R0, 3
  jf R0, __for_47_end
  mov R0, [BP-1]
  mov R1, [BP-2]
  mov R2, [BP+2]
  imul R1, R2
  iadd R0, R1
  mov [BP-1], R0
__for_47_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_47_start
__for_47_end:
  mov R0, [BP-1]
  mov R1, [BP+2]
  imul R1, 3
  isub R0, R1
  iadd R0, 1
  mov R1, [BP+2]
  imul R1, 0
  isub R0, R1
__function_g1_return:
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_f_unit:
  push BP
  mov BP, SP
  isub SP, 2
  push R1
  push R2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
__for_78_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_78_end
  mov R0, [BP-1]
  mov R1, global_arr
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  iadd R0, R1
  mov [BP-1], R0
__for_78_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_78_start
__for_78_end:
  mov R0, [BP-1]
__function_f_unit_return:
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_f_value:
  push BP
  mov BP, SP
  isub SP, 2
  push R1
  push R2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
__for_99_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_99_end
  mov R0, [BP-1]
  mov R1, global_arr
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  mov R2, [BP-2]
  imul R1, R2
  iadd R0, R1
  mov [BP-1], R0
__for_99_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_99_start
__for_99_end:
  mov R0, [BP-1]
__function_f_value_return:
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_f_scaled:
  push BP
  mov BP, SP
  isub SP, 2
  push R1
  push R2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
__for_122_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_122_end
  mov R0, [BP-1]
  mov R1, global_sts
  mov R2, [BP-2]
  imul R2, 3
  iadd R1, R2
  iadd R1, 1
  mov R1, [R1]
  iadd R0, R1
  mov [BP-1], R0
__for_122_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_122_start
__for_122_end:
  mov R0, [BP-1]
__function_f_scaled_return:
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_f_frame:
  push BP
  mov BP, SP
  isub SP, 18
  push R1
  push R2
  mov R0, 0
  mov [BP-18], R0
__for_145_start:
  mov R0, [BP-18]
  ilt R0, 16
  jf R0, __for_145_end
  mov R0, [BP-18]
  imul R0, 2
  lea R1, [BP-16]
  mov R2, [BP-18]
  iadd R1, R2
  mov [R1], R0
__for_145_continue:
  mov R0, [BP-18]
  mov R1, R0
  iadd R1, 1
  mov [BP-18], R1
  jmp __for_145_start
__for_145_end:
  mov R0, 0
  mov [BP-17], R0
  mov R0, 0
  mov [BP-18], R0
__for_164_start:
  mov R0, [BP-18]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_164_end
  mov R0, [BP-17]
  lea R1, [BP-16]
  mov R2, [BP-18]
  iadd R1, R2
  mov R1, [R1]
  iadd R0, R1
  mov [BP-17], R0
__for_164_continue:
  mov R0, [BP-18]
  mov R1, R0
  iadd R1, 1
  mov [BP-18], R1
  jmp __for_164_start
__for_164_end:
  mov R0, [BP-17]
__function_f_frame_return:
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_f_two:
  push BP
  mov BP, SP
  isub SP, 1
  mov R0, 0
  mov [BP-1], R0
__for_182_start:
  mov R0, [BP-1]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_182_end
  mov R0, global_arr
  mov R1, [BP-1]
  iadd R0, R1
  mov R0, [R0]
  iadd R0, 1
  mov R1, global_brr
  mov R2, [BP-1]
  iadd R1, R2
  mov [R1], R0
__for_182_continue:
  mov R0, [BP-1]
  mov R1, R0
  iadd R1, 1
  mov [BP-1], R1
  jmp __for_182_start
__for_182_end:
__function_f_two_return:
  mov SP, BP
  pop BP
  ret

__function_f_down:
  push BP
  mov BP, SP
  isub SP, 2
  push R1
  push R2
  mov R0, 0
  mov [BP-1], R0
  mov R0, [BP+2]
  isub R0, 1
  mov [BP-2], R0
__for_205_start:
  mov R0, [BP-2]
  ige R0, 0
  jf R0, __for_205_end
  mov R0, [BP-1]
  mov R1, global_arr
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  mov R2, [BP-2]
  imul R1, R2
  iadd R0, R1
  mov [BP-1], R0
__for_205_continue:
  mov R0, [BP-2]
  mov R1, R0
  isub R1, 1
  mov [BP-2], R1
  jmp __for_205_start
__for_205_end:
  mov R0, [BP-1]
__function_f_down_return:
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_f_step2:
  push BP
  mov BP, SP
  isub SP, 2
  push R1
  push R2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
__for_230_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_230_end
  mov R0, [BP-1]
  mov R1, global_arr
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  iadd R0, R1
  mov [BP-1], R0
__for_230_continue:
  mov R0, [BP-2]
  iadd R0, 2
  mov [BP-2], R0
  jmp __for_230_start
__for_230_end:
  mov R0, [BP-1]
__function_f_step2_return:
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_f_offset:
  push BP
  mov BP, SP
  isub SP, 2
  push R1
  push R2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
__for_252_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_252_end
  mov R0, [BP-1]
  mov R1, global_arr
  mov R2, [BP-2]
  iadd R2, 3
  iadd R1, R2
  mov R1, [R1]
  iadd R0, R1
  mov [BP-1], R0
__for_252_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_252_start
__for_252_end:
  mov R0, [BP-1]
__function_f_offset_return:
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_r1_shape:
  push BP
  mov BP, SP
  isub SP, 2
  push R1
  push R2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
__while_278_start:
__while_278_continue:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __while_278_end
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
  jmp __while_278_start
__while_278_end:
  mov R0, [BP-1]
__function_r1_shape_return:
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_r2_call:
  push BP
  mov BP, SP
  isub SP, 2
  push R1
  push R2
  push R3
  push R4
  isub SP, 1
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
__for_297_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_297_end
  mov R1, [BP-1]
  mov R2, global_arr
  mov R3, [BP-2]
  iadd R2, R3
  mov R2, [R2]
  mov R4, [BP-2]
  mov [SP], R4
  call __function_g1
  mov R3, R0
  iadd R2, R3
  iadd R1, R2
  mov [BP-1], R1
  mov R0, R1
__for_297_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_297_start
__for_297_end:
  mov R0, [BP-1]
__function_r2_call_return:
  iadd SP, 1
  pop R4
  pop R3
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_r3_counter:
  push BP
  mov BP, SP
  isub SP, 2
  push R1
  push R2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
__for_321_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_321_end
  mov R0, [BP-1]
  mov R1, global_arr
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  iadd R0, R1
  mov [BP-1], R0
__if_336_start:
  mov R0, global_arr
  mov R1, [BP-2]
  iadd R0, R1
  mov R0, [R0]
  ieq R0, 5
  jf R0, __if_336_end
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
__if_336_end:
__for_321_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_321_start
__for_321_end:
  mov R0, [BP-1]
__function_r3_counter_return:
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_r4_goto:
  push BP
  mov BP, SP
  isub SP, 2
  push R1
  push R2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 1
  mov [BP-2], R0
__if_354_start:
  mov R0, [BP+2]
  igt R0, 3
  jf R0, __if_354_end
  jmp __label_369_inside
__if_354_end:
  mov R0, 0
  mov [BP-2], R0
__for_359_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_359_end
__label_369_inside:
  mov R0, [BP-1]
  mov R1, global_arr
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  iadd R0, R1
  mov [BP-1], R0
__for_359_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_359_start
__for_359_end:
  mov R0, [BP-1]
__function_r4_goto_return:
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_r8_base:
  push BP
  mov BP, SP
  isub SP, 3
  push R1
  push R2
  mov R0, global_arr
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
  mov R0, 0
  mov [BP-3], R0
__for_385_start:
  mov R0, [BP-3]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_385_end
  mov R0, [BP-2]
  mov R1, [BP-1]
  mov R2, [BP-3]
  iadd R1, R2
  mov R1, [R1]
  iadd R0, R1
  mov [BP-2], R0
__if_400_start:
  mov R0, [BP-3]
  ieq R0, 2
  jf R0, __if_400_end
  mov R0, global_brr
  mov [BP-1], R0
__if_400_end:
__for_385_continue:
  mov R0, [BP-3]
  mov R1, R0
  iadd R1, 1
  mov [BP-3], R1
  jmp __for_385_start
__for_385_end:
  mov R0, [BP-2]
__function_r8_base_return:
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_main:
  push BP
  mov BP, SP
  isub SP, 3
  mov R0, 0
  mov [BP-1], R0
__for_410_start:
  mov R0, [BP-1]
  ilt R0, 64
  jf R0, __for_410_end
  mov R0, [BP-1]
  mov R1, global_arr
  mov R2, [BP-1]
  iadd R1, R2
  mov [R1], R0
  mov R0, [BP-1]
  iadd R0, 100
  mov R1, global_brr
  mov R2, [BP-1]
  iadd R1, R2
  mov [R1], R0
__for_410_continue:
  mov R0, [BP-1]
  mov R1, R0
  iadd R1, 1
  mov [BP-1], R1
  jmp __for_410_start
__for_410_end:
  mov R0, 0
  mov [BP-1], R0
__for_432_start:
  mov R0, [BP-1]
  ilt R0, 16
  jf R0, __for_432_end
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
  iadd R1, 1
  mov [R1], R0
  mov R0, 2
  mov R1, global_sts
  mov R2, [BP-1]
  imul R2, 3
  iadd R1, R2
  iadd R1, 2
  mov [R1], R0
__for_432_continue:
  mov R0, [BP-1]
  mov R1, R0
  iadd R1, 1
  mov [BP-1], R1
  jmp __for_432_start
__for_432_end:
__if_465_start:
  mov R2, 10
  mov [SP], R2
  call __function_f_unit
  mov R1, R0
  ieq R1, 45
  mov R0, R1
  jf R0, __if_465_else
  mov R1, 1
  mov [SP], R1
  mov R1, 27
  mov [SP+1], R1
  call __function___check
  jmp __if_465_end
__if_465_else:
  mov R1, 0
  mov [SP], R1
  mov R1, 27
  mov [SP+1], R1
  call __function___check
__if_465_end:
__if_476_start:
  mov R2, 4
  mov [SP], R2
  call __function_f_value
  mov R1, R0
  ieq R1, 14
  mov R0, R1
  jf R0, __if_476_else
  mov R1, 1
  mov [SP], R1
  mov R1, 28
  mov [SP+1], R1
  call __function___check
  jmp __if_476_end
__if_476_else:
  mov R1, 0
  mov [SP], R1
  mov R1, 28
  mov [SP+1], R1
  call __function___check
__if_476_end:
__if_487_start:
  mov R2, 4
  mov [SP], R2
  call __function_f_scaled
  mov R1, R0
  ieq R1, 60
  mov R0, R1
  jf R0, __if_487_else
  mov R1, 1
  mov [SP], R1
  mov R1, 29
  mov [SP+1], R1
  call __function___check
  jmp __if_487_end
__if_487_else:
  mov R1, 0
  mov [SP], R1
  mov R1, 29
  mov [SP+1], R1
  call __function___check
__if_487_end:
__if_498_start:
  mov R2, 5
  mov [SP], R2
  call __function_f_frame
  mov R1, R0
  ieq R1, 20
  mov R0, R1
  jf R0, __if_498_else
  mov R1, 1
  mov [SP], R1
  mov R1, 30
  mov [SP+1], R1
  call __function___check
  jmp __if_498_end
__if_498_else:
  mov R1, 0
  mov [SP], R1
  mov R1, 30
  mov [SP+1], R1
  call __function___check
__if_498_end:
  mov R1, 8
  mov [SP], R1
  call __function_f_two
__if_511_start:
  mov R0, [89]
  ieq R0, 8
  jf R0, __if_511_else
  mov R1, 1
  mov [SP], R1
  mov R1, 32
  mov [SP+1], R1
  call __function___check
  jmp __if_511_end
__if_511_else:
  mov R1, 0
  mov [SP], R1
  mov R1, 32
  mov [SP+1], R1
  call __function___check
__if_511_end:
__if_523_start:
  mov R0, [90]
  ieq R0, 108
  jf R0, __if_523_else
  mov R1, 1
  mov [SP], R1
  mov R1, 33
  mov [SP+1], R1
  call __function___check
  jmp __if_523_end
__if_523_else:
  mov R1, 0
  mov [SP], R1
  mov R1, 33
  mov [SP+1], R1
  call __function___check
__if_523_end:
__if_535_start:
  mov R2, 5
  mov [SP], R2
  call __function_f_down
  mov R1, R0
  ieq R1, 30
  mov R0, R1
  jf R0, __if_535_else
  mov R1, 1
  mov [SP], R1
  mov R1, 34
  mov [SP+1], R1
  call __function___check
  jmp __if_535_end
__if_535_else:
  mov R1, 0
  mov [SP], R1
  mov R1, 34
  mov [SP+1], R1
  call __function___check
__if_535_end:
__if_546_start:
  mov R2, 7
  mov [SP], R2
  call __function_f_step2
  mov R1, R0
  ieq R1, 12
  mov R0, R1
  jf R0, __if_546_else
  mov R1, 1
  mov [SP], R1
  mov R1, 35
  mov [SP+1], R1
  call __function___check
  jmp __if_546_end
__if_546_else:
  mov R1, 0
  mov [SP], R1
  mov R1, 35
  mov [SP+1], R1
  call __function___check
__if_546_end:
__if_557_start:
  mov R2, 3
  mov [SP], R2
  call __function_f_offset
  mov R1, R0
  ieq R1, 12
  mov R0, R1
  jf R0, __if_557_else
  mov R1, 1
  mov [SP], R1
  mov R1, 36
  mov [SP+1], R1
  call __function___check
  jmp __if_557_end
__if_557_else:
  mov R1, 0
  mov [SP], R1
  mov R1, 36
  mov [SP+1], R1
  call __function___check
__if_557_end:
__if_568_start:
  mov R2, 3
  mov [SP], R2
  call __function_r1_shape
  mov R1, R0
  ieq R1, 6
  mov R0, R1
  jf R0, __if_568_else
  mov R1, 1
  mov [SP], R1
  mov R1, 37
  mov [SP+1], R1
  call __function___check
  jmp __if_568_end
__if_568_else:
  mov R1, 0
  mov [SP], R1
  mov R1, 37
  mov [SP+1], R1
  call __function___check
__if_568_end:
__if_579_start:
  mov R2, 3
  mov [SP], R2
  call __function_r2_call
  mov R1, R0
  ieq R1, 9
  mov R0, R1
  jf R0, __if_579_else
  mov R1, 1
  mov [SP], R1
  mov R1, 38
  mov [SP+1], R1
  call __function___check
  jmp __if_579_end
__if_579_else:
  mov R1, 0
  mov [SP], R1
  mov R1, 38
  mov [SP+1], R1
  call __function___check
__if_579_end:
__if_592_start:
  mov R2, 8
  mov [SP], R2
  call __function_r3_counter
  mov R1, R0
  ieq R1, 22
  mov R0, R1
  jf R0, __if_592_else
  mov R1, 1
  mov [SP], R1
  mov R1, 39
  mov [SP+1], R1
  call __function___check
  jmp __if_592_end
__if_592_else:
  mov R1, 0
  mov [SP], R1
  mov R1, 39
  mov [SP+1], R1
  call __function___check
__if_592_end:
__if_603_start:
  mov R2, 5
  mov [SP], R2
  call __function_r4_goto
  mov R1, R0
  ieq R1, 10
  mov R0, R1
  jf R0, __if_603_else
  mov R1, 1
  mov [SP], R1
  mov R1, 40
  mov [SP+1], R1
  call __function___check
  jmp __if_603_end
__if_603_else:
  mov R1, 0
  mov [SP], R1
  mov R1, 40
  mov [SP+1], R1
  call __function___check
__if_603_end:
__if_614_start:
  mov R2, 2
  mov [SP], R2
  call __function_r4_goto
  mov R1, R0
  ieq R1, 1
  mov R0, R1
  jf R0, __if_614_else
  mov R1, 1
  mov [SP], R1
  mov R1, 41
  mov [SP+1], R1
  call __function___check
  jmp __if_614_end
__if_614_else:
  mov R1, 0
  mov [SP], R1
  mov R1, 41
  mov [SP+1], R1
  call __function___check
__if_614_end:
__if_625_start:
  mov R2, 5
  mov [SP], R2
  call __function_r8_base
  mov R1, R0
  ieq R1, 12
  mov R0, R1
  jf R0, __if_625_else
  mov R1, 1
  mov [SP], R1
  mov R1, 42
  mov [SP+1], R1
  call __function___check
  jmp __if_625_end
__if_625_else:
  mov R1, 0
  mov [SP], R1
  mov R1, 42
  mov [SP+1], R1
  call __function___check
__if_625_end:
__function_main_return:
  mov SP, BP
  pop BP
  ret

