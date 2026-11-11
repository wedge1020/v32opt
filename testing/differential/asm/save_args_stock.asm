; program start section
  call __global_scope_initialization
  call __function_main
  hlt

; location of global variables
  %define global___fails 0
  %define global___checks 1
  %define global___lines 2
  %define global_result 18
  %define global_g0 19
  %define global_g1 20
  %define global_g2 21

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

__function_leaf0:
  push BP
  mov BP, SP
  isub SP, 7
  push R1
  push R2
  mov R0, [BP+2]
  mov [BP-1], R0
  mov R0, [BP+3]
  xor R0, 2
  mov [BP-2], R0
  mov R0, [BP+3]
  xor R0, 3
  mov [BP-3], R0
  mov R0, [BP+3]
  xor R0, 4
  mov [BP-4], R0
  mov R0, [BP+3]
  xor R0, 5
  mov [BP-5], R0
  mov R0, 0
  mov [BP-6], R0
__for_72_start:
  mov R0, [BP-6]
  ilt R0, 2
  jf R0, __for_72_end
  mov R0, [BP-4]
  mov R1, [BP-4]
  mov R2, [BP-6]
  and R1, R2
  iadd R0, R1
  mov [BP-1], R0
  mov R0, [BP-3]
  mov R1, [BP-2]
  mov R2, [BP-6]
  xor R1, R2
  iadd R0, R1
  mov [BP-4], R0
  mov R0, [BP-1]
  mov R1, [BP-1]
  mov R2, [BP-6]
  xor R1, R2
  iadd R0, R1
  mov [BP-1], R0
__for_72_continue:
  mov R0, [BP-6]
  mov R1, R0
  iadd R1, 1
  mov [BP-6], R1
  jmp __for_72_start
__for_72_end:
  mov R0, [BP-4]
  mov R1, [BP-1]
  iadd R0, R1
  mov R1, [BP-3]
  iadd R0, R1
__function_leaf0_return:
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_leaf1:
  push BP
  mov BP, SP
  isub SP, 6
  push R1
  push R2
  mov R0, [BP+3]
  xor R0, 1
  mov [BP-1], R0
  mov R0, [BP+3]
  xor R0, 2
  mov [BP-2], R0
  mov R0, 2
  mov [BP-3], R0
  mov R0, [BP+3]
  mov [BP-4], R0
  mov R0, 0
  mov [BP-5], R0
__for_138_start:
  mov R0, [BP-5]
  ilt R0, 2
  jf R0, __for_138_end
  mov R0, [BP-4]
  mov R1, [BP-3]
  mov R2, [BP-5]
  xor R1, R2
  iadd R0, R1
  mov [BP-2], R0
  mov R0, [BP-2]
  mov R1, [BP-3]
  mov R2, [BP-5]
  and R1, R2
  iadd R0, R1
  mov [BP-1], R0
  mov R0, [BP-4]
  mov R1, [BP-2]
  mov R2, [BP-5]
  xor R1, R2
  isub R0, R1
  mov [BP-3], R0
  mov R0, [BP-4]
  mov R1, [BP-1]
  mov R2, [BP-5]
  iadd R1, R2
  or R0, R1
  mov [BP-4], R0
__for_138_continue:
  mov R0, [BP-5]
  mov R1, R0
  iadd R1, 1
  mov [BP-5], R1
  jmp __for_138_start
__for_138_end:
  mov R0, 0
  mov [BP-5], R0
__for_180_start:
  mov R0, [BP-5]
  ilt R0, 3
  jf R0, __for_180_end
  mov R0, [BP-3]
  mov R1, [BP-3]
  mov R2, [BP-5]
  xor R1, R2
  iadd R0, R1
  mov [BP-2], R0
  mov R0, [BP-2]
  mov R1, [BP-4]
  mov R2, [BP-5]
  xor R1, R2
  isub R0, R1
  mov [BP-1], R0
  mov R0, [BP-4]
  mov R1, [BP-1]
  mov R2, [BP-5]
  and R1, R2
  isub R0, R1
  mov [BP-1], R0
__for_180_continue:
  mov R0, [BP-5]
  mov R1, R0
  iadd R1, 1
  mov [BP-5], R1
  jmp __for_180_start
__for_180_end:
  mov R0, [BP-4]
  mov R1, [BP-3]
  iadd R0, R1
  mov R1, [BP-1]
  iadd R0, R1
__function_leaf1_return:
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_mid0:
  push BP
  mov BP, SP
  isub SP, 5
  push R1
  push R2
  isub SP, 2
  mov R0, [BP+2]
  mov [BP-1], R0
  mov R0, [BP+3]
  mov [BP-2], R0
  mov R0, 2
  mov [BP-3], R0
  mov R0, 0
  mov [BP-4], R0
__for_238_start:
  mov R0, [BP-4]
  ilt R0, 3
  jf R0, __for_238_end
  mov R0, [BP-3]
  mov R1, [BP-2]
  mov R2, [BP-4]
  xor R1, R2
  or R0, R1
  mov [BP-2], R0
  mov R0, [BP-3]
  mov R1, [BP-3]
  mov R2, [BP-4]
  xor R1, R2
  iadd R0, R1
  mov [BP-3], R0
  mov R0, [BP-3]
  mov R1, [BP-1]
  mov R2, [BP-4]
  xor R1, R2
  xor R0, R1
  mov [BP-3], R0
  mov R0, [BP-2]
  mov R1, [BP-2]
  mov R2, [BP-4]
  and R1, R2
  xor R0, R1
  mov [BP-1], R0
  mov R0, 0
  mov [BP-5], R0
__for_280_start:
  mov R0, [BP-5]
  ilt R0, 2
  jf R0, __for_280_end
  mov R0, [BP-2]
  mov R1, [BP-2]
  mov R2, [BP-5]
  iadd R1, R2
  iadd R0, R1
  mov [BP-2], R0
__for_280_continue:
  mov R0, [BP-5]
  mov R1, R0
  iadd R1, 1
  mov [BP-5], R1
  jmp __for_280_start
__for_280_end:
__for_238_continue:
  mov R0, [BP-4]
  mov R1, R0
  iadd R1, 1
  mov [BP-4], R1
  jmp __for_238_start
__for_238_end:
  mov R2, [BP-1]
  mov [SP], R2
  mov R2, [BP-3]
  mov [SP+1], R2
  call __function_leaf0
  mov R1, R0
  mov [BP-2], R1
  mov R0, R1
  mov R0, [BP-3]
  mov R1, [BP-3]
  iadd R0, R1
  mov [BP-3], R0
  mov R0, [BP-3]
  mov R1, [BP-2]
  iadd R0, R1
  mov [BP-3], R0
  mov R0, [BP-2]
  mov R1, [BP-1]
  iadd R0, R1
  mov R1, [BP-3]
  iadd R0, R1
__function_mid0_return:
  iadd SP, 2
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_main:
  push BP
  mov BP, SP
  isub SP, 3
  mov R0, 3
  mov [global_g0], R0
  mov R0, 5
  mov [global_g1], R0
  mov R0, 7
  mov [global_g2], R0
  mov R1, [global_result]
  imul R1, 3
  mov R2, [global_g0]
  mov R3, [global_g1]
  mov R4, [global_g2]
  mov R5, [global_g2]
  mov R7, 2
  mov [SP], R7
  mov R7, 1
  mov [SP+1], R7
  call __function_leaf0
  mov R6, R0
  mov R7, [global_g1]
  iadd R7, 3
  isub R6, R7
  xor R6, 1
  iadd R6, 8
  imul R5, R6
  iadd R4, R5
  imul R3, R4
  iadd R2, R3
  iadd R1, R2
  mov [global_result], R1
  mov R0, R1
  mov R1, [global_result]
  imul R1, 3
  mov R3, 3
  mov [SP], R3
  mov R3, 1
  mov [SP+1], R3
  mov R5, 1234          ; hand-written: R5 kept across a call into stock-layout mid0
  call __function_mid0
  mov [2000], R5
  mov R2, R0
  mov R4, 0
  mov [SP], R4
  mov R4, 3
  mov [SP+1], R4
  call __function_mid0
  mov R3, R0
  mov R4, [global_g1]
  mov R5, [global_g2]
  mov R6, [global_g0]
  mov R8, 3
  mov [SP], R8
  mov R8, 1
  mov [SP+1], R8
  call __function_mid0
  mov R7, R0
  mov R8, [global_g1]
  mov R9, [global_g2]
  mov R10, [global_g0]
  mov R12, 4
  mov [SP], R12
  mov R12, 1
  mov [SP+1], R12
  call __function_leaf1
  mov R11, R0
  xor R10, R11
  isub R9, R10
  isub R8, R9
  iadd R7, R8
  iadd R6, R7
  xor R5, R6
  iadd R4, R5
  imul R3, R4
  isub R2, R3
  iadd R1, R2
  mov [global_result], R1
  mov R0, R1
  mov R1, [global_result]
  imul R1, 3
  mov R2, [global_g0]
  mov R3, [global_g2]
  mov R4, [global_g2]
  mov R6, 3
  mov [SP], R6
  mov R6, 1
  mov [SP+1], R6
  call __function_leaf0
  mov R5, R0
  mov R6, [global_g0]
  xor R6, 6
  iadd R5, R6
  iadd R4, R5
  imul R3, R4
  iadd R2, R3
  imul R2, 7
  iadd R1, R2
  mov [global_result], R1
  mov R0, R1
  mov R1, [global_result]
  imul R1, 3
  mov R2, [global_g2]
  mov R3, [global_g2]
  mov R4, [global_g0]
  mov R6, 1
  mov [SP], R6
  mov R6, 1
  mov [SP+1], R6
  call __function_leaf0
  mov R5, R0
  mov R6, [global_g1]
  mov R8, 3
  mov [SP], R8
  mov R8, 2
  mov [SP+1], R8
  call __function_leaf1
  mov R7, R0
  mov R8, [global_g2]
  mov R10, 0
  mov [SP], R10
  mov R10, 2
  mov [SP+1], R10
  call __function_leaf0
  mov R9, R0
  isgn R9
  iadd R9, 1
  iadd R8, R9
  imul R7, R8
  xor R6, R7
  isub R5, R6
  isub R4, R5
  iadd R3, R4
  imul R2, R3
  iadd R1, R2
  mov [global_result], R1
  mov R0, R1
__if_466_start:
  mov R0, 1
  cib R0
  jf R0, __if_466_else
  mov R1, 1
  mov [SP], R1
  mov R1, 65
  mov [SP+1], R1
  call __function___check
  jmp __if_466_end
__if_466_else:
  mov R1, 0
  mov [SP], R1
  mov R1, 65
  mov [SP+1], R1
  call __function___check
__if_466_end:
__function_main_return:
  mov SP, BP
  pop BP
  ret

