;@hints 1 abi=caller-save
; program start section
  call __function_main
  hlt

; location of global variables

;@fn __function_plain_s0 leaf escapes=none
__function_plain_s0:
  push BP
  mov BP, SP
  mov R0, [BP+2]
  imul R0, 3
  mov R1, [BP+3]
  iadd R0, R1
  mov R1, [BP+2]
  xor R0, R1
  mov R1, [BP+3]
  shl R1, 2
  mov R2, [BP+2]
  and R2, 7
  isub R1, R2
  imul R1, 5
  iadd R0, R1
  mov R1, [BP+2]
  iadd R0, R1
__function_plain_s0_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_always_s0 alwaysinline leaf escapes=none
__function_always_s0:
  push BP
  mov BP, SP
  mov R0, [BP+2]
  imul R0, 3
  mov R1, [BP+3]
  iadd R0, R1
  mov R1, [BP+2]
  xor R0, R1
  mov R1, [BP+3]
  shl R1, 2
  mov R2, [BP+2]
  and R2, 7
  isub R1, R2
  imul R1, 5
  iadd R0, R1
  mov R1, [BP+2]
  iadd R0, R1
__function_always_s0_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_kw_s0 inline leaf escapes=none
__function_kw_s0:
  push BP
  mov BP, SP
  mov R0, [BP+2]
  imul R0, 3
  mov R1, [BP+3]
  iadd R0, R1
  mov R1, [BP+3]
  shl R1, 2
  iadd R0, R1
  mov R1, [BP+2]
  iadd R0, R1
__function_kw_s0_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_kw_not_s0 leaf escapes=none
__function_kw_not_s0:
  push BP
  mov BP, SP
  mov R0, [BP+2]
  imul R0, 3
  mov R1, [BP+3]
  iadd R0, R1
  mov R1, [BP+3]
  shl R1, 2
  iadd R0, R1
  mov R1, [BP+2]
  iadd R0, R1
__function_kw_not_s0_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_hot_s0 hot leaf escapes=none
__function_hot_s0:
  push BP
  mov BP, SP
  mov R0, [BP+2]
  imul R0, 3
  mov R1, [BP+3]
  iadd R0, R1
  mov R1, [BP+3]
  shl R1, 2
  iadd R0, R1
  mov R1, [BP+2]
  iadd R0, R1
__function_hot_s0_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_cold_always_s0 alwaysinline cold leaf escapes=none
__function_cold_always_s0:
  push BP
  mov BP, SP
  mov R0, [BP+2]
  iadd R0, 2
__function_cold_always_s0_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_cold_tiny_s0 cold leaf escapes=none
__function_cold_tiny_s0:
  push BP
  mov BP, SP
  mov R0, [BP+2]
  iadd R0, 2
__function_cold_tiny_s0_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_never_s0 noinline leaf escapes=none
__function_never_s0:
  push BP
  mov BP, SP
  mov R0, [BP+2]
  iadd R0, 3
__function_never_s0_return:
  mov SP, BP
  pop BP
  ret

;@fn __function_main escapes=none
__function_main:
  push BP
  mov BP, SP
  isub SP, 4
  mov R0, 1
  mov [BP-1], R0
  mov R0, [BP-1]
  mov [SP], R0
  mov R0, 2
  mov [SP+1], R0
  call __function_plain_s0
  mov [BP-2], R0
  mov R1, [BP-1]
  mov [SP], R1
  mov R1, 2
  mov [SP+1], R1
  call __function_always_s0
  mov R1, R0
  mov R0, [BP-2]
  iadd R0, R1
  mov [BP-2], R0
  mov R1, [BP-1]
  mov [SP], R1
  mov R1, 2
  mov [SP+1], R1
  call __function_kw_s0
  mov R1, R0
  mov R0, [BP-2]
  iadd R0, R1
  mov [BP-2], R0
  mov R1, [BP-1]
  mov [SP], R1
  mov R1, 2
  mov [SP+1], R1
  call __function_kw_not_s0
  mov R1, R0
  mov R0, [BP-2]
  iadd R0, R1
  mov [BP-2], R0
  mov R1, [BP-1]
  mov [SP], R1
  mov R1, 2
  mov [SP+1], R1
  call __function_hot_s0
  mov R1, R0
  mov R0, [BP-2]
  iadd R0, R1
  mov [BP-2], R0
  mov R1, [BP-1]
  mov [SP], R1
  call __function_cold_always_s0
  mov R1, R0
  mov R0, [BP-2]
  iadd R0, R1
  mov [BP-2], R0
  mov R1, [BP-1]
  mov [SP], R1
  call __function_cold_tiny_s0
  mov R1, R0
  mov R0, [BP-2]
  iadd R0, R1
  mov [BP-2], R0
  mov R1, [BP-1]
  mov [SP], R1
  call __function_never_s0
  mov R1, R0
  mov R0, [BP-2]
  iadd R0, R1
__function_main_return:
  mov SP, BP
  pop BP
  ret

