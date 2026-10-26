;@hints 1 abi=caller-save
; program start section
  call __function_main
  hlt

; location of global variables
  %define global_g 1

;@fn __function_plain4 leaf escapes=none
__function_plain4:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
__for_1_start:
  mov R0, [BP-2]
  ilt R0, 4
  jf R0, __for_1_end
  mov R0, [BP-1]
  mov R1, global_g
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  xor R0, R1
  mov [BP-1], R0
__for_1_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_1_start
__for_1_end:
  mov R0, [BP-1]
__function_plain4_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_plain20 leaf escapes=none
__function_plain20:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
__for_2_start:
  mov R0, [BP-2]
  ilt R0, 20
  jf R0, __for_2_end
  mov R0, [BP-1]
  mov R1, global_g
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  xor R0, R1
  mov [BP-1], R0
__for_2_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_2_start
__for_2_end:
  mov R0, [BP-1]
__function_plain20_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_plain40 leaf escapes=none
__function_plain40:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
__for_3_start:
  mov R0, [BP-2]
  ilt R0, 40
  jf R0, __for_3_end
  mov R0, [BP-1]
  mov R1, global_g
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  xor R0, R1
  mov [BP-1], R0
__for_3_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_3_start
__for_3_end:
  mov R0, [BP-1]
__function_plain40_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_plain_n leaf escapes=none
__function_plain_n:
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
  mov R1, global_g
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  xor R0, R1
  mov [BP-1], R0
__for_4_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_4_start
__for_4_end:
  mov R0, [BP-1]
__function_plain_n_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_nounroll4 leaf escapes=none
__function_nounroll4:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
;@loop nounroll
__for_5_start:
  mov R0, [BP-2]
  ilt R0, 4
  jf R0, __for_5_end
  mov R0, [BP-1]
  mov R1, global_g
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  xor R0, R1
  mov [BP-1], R0
__for_5_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_5_start
__for_5_end:
  mov R0, [BP-1]
__function_nounroll4_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_unroll40 leaf escapes=none
__function_unroll40:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
;@loop unroll=40
__for_6_start:
  mov R0, [BP-2]
  ilt R0, 40
  jf R0, __for_6_end
  mov R0, [BP-1]
  mov R1, global_g
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  xor R0, R1
  mov [BP-1], R0
__for_6_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_6_start
__for_6_end:
  mov R0, [BP-1]
__function_unroll40_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_unroll_full30 leaf escapes=none
__function_unroll_full30:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
;@loop unroll=full
__for_7_start:
  mov R0, [BP-2]
  ilt R0, 30
  jf R0, __for_7_end
  mov R0, [BP-1]
  mov R1, global_g
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  xor R0, R1
  mov [BP-1], R0
__for_7_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_7_start
__for_7_end:
  mov R0, [BP-1]
__function_unroll_full30_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_unroll_part leaf escapes=none
__function_unroll_part:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
;@loop unroll=6
__for_8_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_8_end
  mov R0, [BP-1]
  mov R1, global_g
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  xor R0, R1
  mov [BP-1], R0
__for_8_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_8_start
__for_8_end:
  mov R0, [BP-1]
__function_unroll_part_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_nounroll_n leaf escapes=none
__function_nounroll_n:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
;@loop nounroll
__for_9_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_9_end
  mov R0, [BP-1]
  mov R1, global_g
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  xor R0, R1
  mov [BP-1], R0
__for_9_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_9_start
__for_9_end:
  mov R0, [BP-1]
__function_nounroll_n_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_hot_loop20 leaf escapes=none
__function_hot_loop20:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
;@loop hot
__for_10_start:
  mov R0, [BP-2]
  ilt R0, 20
  jf R0, __for_10_end
  mov R0, [BP-1]
  mov R1, global_g
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  xor R0, R1
  mov [BP-1], R0
__for_10_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_10_start
__for_10_end:
  mov R0, [BP-1]
__function_hot_loop20_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_cold_loop4 leaf escapes=none
__function_cold_loop4:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
;@loop cold
__for_11_start:
  mov R0, [BP-2]
  ilt R0, 4
  jf R0, __for_11_end
  mov R0, [BP-1]
  mov R1, global_g
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  xor R0, R1
  mov [BP-1], R0
__for_11_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_11_start
__for_11_end:
  mov R0, [BP-1]
__function_cold_loop4_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_cold_unroll4 leaf escapes=none
__function_cold_unroll4:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
;@loop cold unroll=4
__for_12_start:
  mov R0, [BP-2]
  ilt R0, 4
  jf R0, __for_12_end
  mov R0, [BP-1]
  mov R1, global_g
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  xor R0, R1
  mov [BP-1], R0
__for_12_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_12_start
__for_12_end:
  mov R0, [BP-1]
__function_cold_unroll4_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_hot_fn20 hot leaf escapes=none
__function_hot_fn20:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
__for_13_start:
  mov R0, [BP-2]
  ilt R0, 20
  jf R0, __for_13_end
  mov R0, [BP-1]
  mov R1, global_g
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  xor R0, R1
  mov [BP-1], R0
__for_13_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_13_start
__for_13_end:
  mov R0, [BP-1]
__function_hot_fn20_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_cold_fn4 cold leaf escapes=none
__function_cold_fn4:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov R0, 0
  mov [BP-2], R0
__for_14_start:
  mov R0, [BP-2]
  ilt R0, 4
  jf R0, __for_14_end
  mov R0, [BP-1]
  mov R1, global_g
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  xor R0, R1
  mov [BP-1], R0
__for_14_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_14_start
__for_14_end:
  mov R0, [BP-1]
__function_cold_fn4_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_cold_fn_n cold leaf escapes=none
__function_cold_fn_n:
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
  mov R1, global_g
  mov R2, [BP-2]
  iadd R1, R2
  mov R1, [R1]
  xor R0, R1
  mov [BP-1], R0
__for_15_continue:
  mov R0, [BP-2]
  mov R1, R0
  iadd R1, 1
  mov [BP-2], R1
  jmp __for_15_start
__for_15_end:
  mov R0, [BP-1]
__function_cold_fn_n_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_main escapes=none
__function_main:
  push BP
  mov BP, SP
  isub SP, 2
  call __function_plain4
  mov [BP-1], R0
  call __function_plain20
  mov R1, R0
  mov R0, [BP-1]
  iadd R0, R1
  mov [BP-1], R0
  call __function_plain40
  mov R1, R0
  mov R0, [BP-1]
  iadd R0, R1
  mov [BP-1], R0
  mov R1, 3
  mov [SP], R1
  call __function_plain_n
  mov R1, R0
  mov R0, [BP-1]
  iadd R0, R1
  mov [BP-1], R0
  call __function_nounroll4
  mov R1, R0
  mov R0, [BP-1]
  iadd R0, R1
  mov [BP-1], R0
  call __function_unroll40
  mov R1, R0
  mov R0, [BP-1]
  iadd R0, R1
  mov [BP-1], R0
  call __function_unroll_full30
  mov R1, R0
  mov R0, [BP-1]
  iadd R0, R1
  mov [BP-1], R0
  mov R1, 5
  mov [SP], R1
  call __function_unroll_part
  mov R1, R0
  mov R0, [BP-1]
  iadd R0, R1
  mov [BP-1], R0
  mov R1, 5
  mov [SP], R1
  call __function_nounroll_n
  mov R1, R0
  mov R0, [BP-1]
  iadd R0, R1
  mov [BP-1], R0
  call __function_hot_loop20
  mov R1, R0
  mov R0, [BP-1]
  iadd R0, R1
  mov [BP-1], R0
  call __function_cold_loop4
  mov R1, R0
  mov R0, [BP-1]
  iadd R0, R1
  mov [BP-1], R0
  call __function_cold_unroll4
  mov R1, R0
  mov R0, [BP-1]
  iadd R0, R1
  mov [BP-1], R0
  call __function_hot_fn20
  mov R1, R0
  mov R0, [BP-1]
  iadd R0, R1
  mov [BP-1], R0
  call __function_cold_fn4
  mov R1, R0
  mov R0, [BP-1]
  iadd R0, R1
  mov [BP-1], R0
  mov R1, 3
  mov [SP], R1
  call __function_cold_fn_n
  mov R1, R0
  mov R0, [BP-1]
  iadd R0, R1
__function_main_return:
  mov SP, BP
  pop BP
  ret

