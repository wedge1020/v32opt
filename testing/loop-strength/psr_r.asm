  call __function_main
  hlt
%define global_arr 100

__function_leaf:
  push BP
  mov BP, SP
  mov SP, BP
  pop BP
  ret

__function_ok_unit:
  push BP
  mov BP, SP
  push R1
  push R2
  push R4
  push R5
  mov R5, 0
  mov R4, 0
__ok_unit_start:
  mov R0, R4
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __ok_unit_end
  mov R1, global_arr
  iadd R1, R4
  mov R1, [R1]
  iadd R5, R1
  iadd R5, R4
  iadd R4, 1
  jmp __ok_unit_start
__ok_unit_end:
  mov R0, R5
__function_ok_unit_return:
  pop R5
  pop R4
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_a2_call:
  push BP
  mov BP, SP
  push R1
  push R2
  push R4
  push R5
  mov R5, 0
  mov R4, 0
__a2_call_start:
  mov R0, R4
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __a2_call_end
  mov R1, global_arr
  iadd R1, R4
  mov R1, [R1]
  iadd R5, R1
  call __function_leaf
  iadd R5, R4
  iadd R4, 1
  jmp __a2_call_start
__a2_call_end:
  mov R0, R5
__function_a2_call_return:
  pop R5
  pop R4
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_a2_dir:
  push BP
  mov BP, SP
  push R1
  push R2
  push R4
  push R5
  mov R5, 0
  mov R4, 0
__a2_dir_start:
  mov R0, R4
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __a2_dir_end
  mov R1, global_arr
  iadd R1, R4
  mov R1, [R1]
  iadd R5, R1
%define PSR_X 1
  iadd R5, R4
  iadd R4, 1
  jmp __a2_dir_start
__a2_dir_end:
  mov R0, R5
__function_a2_dir_return:
  pop R5
  pop R4
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_a2_vol:
  push BP
  mov BP, SP
  push R1
  push R2
  push R4
  push R5
  mov R5, 0
  mov R4, 0
__a2_vol_start:
  mov R0, R4
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __a2_vol_end
  mov R1, 100   ;@v
  iadd R1, R4
  mov R1, [R1]
  iadd R5, R1
  iadd R5, R4
  iadd R4, 1
  jmp __a2_vol_start
__a2_vol_end:
  mov R0, R5
__function_a2_vol_return:
  pop R5
  pop R4
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_a3_bp:
  push BP
  mov BP, SP
  isub SP, 8
  push R1
  push R2
  push R4
  push R5
  mov R5, 0
  mov R4, 0
  mov R0, 3
  mov [BP-1], R0
  mov R0, 6
  mov [BP-2], R0
  mov R0, 9
  mov [BP-3], R0
  mov R0, 12
  mov [BP-4], R0
  mov R0, 15
  mov [BP-5], R0
  mov R0, 18
  mov [BP-6], R0
  mov R0, 21
  mov [BP-7], R0
  mov R0, 24
  mov [BP-8], R0
__a3_bp_start:
  mov R0, R4
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __a3_bp_end
  lea R1, [BP-8]
  iadd R1, R4
  mov R1, [R1]
  iadd BP, 1
  isub BP, 1
  iadd R5, R1
  iadd R5, R4
  iadd R4, 1
  jmp __a3_bp_start
__a3_bp_end:
  mov R0, R5
__function_a3_bp_return:
  pop R5
  pop R4
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_a5_cold:
  push BP
  mov BP, SP
  push R1
  push R2
  push R4
  push R5
  mov R5, 0
  mov R4, 0
;@loop cold
__a5_cold_start:
  mov R0, R4
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __a5_cold_end
  mov R1, global_arr
  iadd R1, R4
  mov R1, [R1]
  iadd R5, R1
  iadd R5, R4
  iadd R4, 1
  jmp __a5_cold_start
__a5_cold_end:
  mov R0, R5
__function_a5_cold_return:
  pop R5
  pop R4
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_a6_ru:
  push BP
  mov BP, SP
  push R1
  push R2
  push R4
  push R5
  mov R5, 0
  mov R4, 0
__a6_ru_start:
  mov R0, R4
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __a6_ru_end
  mov R1, global_arr
  mov R2, R4
  imul R2, 2
  iadd R1, R2
  mov R1, [R1]
  iadd R5, R1
  iadd R5, R2
  iadd R5, R4
  iadd R4, 1
  jmp __a6_ru_start
__a6_ru_end:
  mov R0, R5
__function_a6_ru_return:
  pop R5
  pop R4
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_a7_k0:
  push BP
  mov BP, SP
  push R1
  push R2
  push R4
  push R5
  mov R5, 0
  mov R4, 0
__a7_k0_start:
  mov R0, R4
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __a7_k0_end
  mov R1, global_arr
  mov R2, R4
  imul R2, 0
  iadd R1, R2
  mov R1, [R1]
  iadd R5, R1
  iadd R5, R4
  iadd R4, 1
  jmp __a7_k0_start
__a7_k0_end:
  mov R0, R5
__function_a7_k0_return:
  pop R5
  pop R4
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_a8_rbri:
  push BP
  mov BP, SP
  push R1
  push R2
  push R4
  push R5
  mov R5, 0
  mov R4, 0
__a8_rbri_start:
  mov R0, R4
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __a8_rbri_end
  mov R1, R4
  iadd R1, R4
  mov R1, [R1]
  iadd R5, R1
  iadd R5, R4
  iadd R4, 1
  jmp __a8_rbri_start
__a8_rbri_end:
  mov R0, R5
__function_a8_rbri_return:
  pop R5
  pop R4
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_a8_rbw:
  push BP
  mov BP, SP
  push R1
  push R2
  push R4
  push R5
  mov R5, 0
  mov R4, 0
  mov R2, global_arr
__a8_rbw_start:
  mov R0, R4
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __a8_rbw_end
  mov R1, R2
  iadd R1, R4
  mov R1, [R1]
  iadd R5, R1
  iadd R2, 1
  iadd R5, R4
  iadd R4, 1
  jmp __a8_rbw_start
__a8_rbw_end:
  mov R0, R5
__function_a8_rbw_return:
  pop R5
  pop R4
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_main:
  push BP
  mov BP, SP
  isub SP, 1
  mov R0, 0
__init:
  mov R1, R0
  iadd R1, 100
  mov [R1], R0
  iadd R0, 1
  mov R1, R0
  ilt R1, 40
  jt R1, __init
  mov R0, 6
  mov [SP], R0
  call __function_ok_unit
  mov [200], R0
  mov R0, 6
  mov [SP], R0
  call __function_a2_call
  mov [201], R0
  mov R0, 6
  mov [SP], R0
  call __function_a2_dir
  mov [202], R0
  mov R0, 6
  mov [SP], R0
  call __function_a2_vol
  mov [203], R0
  mov R0, 6
  mov [SP], R0
  call __function_a3_bp
  mov [204], R0
  mov R0, 6
  mov [SP], R0
  call __function_a5_cold
  mov [205], R0
  mov R0, 6
  mov [SP], R0
  call __function_a6_ru
  mov [206], R0
  mov R0, 6
  mov [SP], R0
  call __function_a7_k0
  mov [207], R0
  mov R0, 6
  mov [SP], R0
  call __function_a8_rbri
  mov [208], R0
  mov R0, 6
  mov [SP], R0
  call __function_a8_rbw
  mov [209], R0
  mov SP, BP
  pop BP
  ret
