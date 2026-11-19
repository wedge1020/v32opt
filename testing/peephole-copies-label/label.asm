; program start section
  call __function_main
  hlt

; Hand-written: a LABEL (a data or function address) fed through "MOV Rt, label" into the integer
; ALU forms. v32as accepts a label only in MOV and in a memory address ("[label]"); every other
; integer form takes a register or a literal integer ("IADD second operand must be a register or
; an integer"). run.sh says what each function must become.

  %define global_x 300

__function_fire_mov:
  mov R1, __lbl_arr
  mov R0, R1
  ret

__function_fire_load:
  mov R1, __lbl_arr
  mov R0, [R1]
  ret

__function_fire_define_alu:
  mov R0, [200]
  mov R1, global_x
  iadd R0, R1
  ret

__function_keep_iadd:
  mov R0, [200]
  mov R1, __lbl_arr
  iadd R0, R1
  ret

__function_keep_isub:
  mov R0, [200]
  mov R1, __lbl_arr
  isub R0, R1
  ret

__function_keep_and:
  mov R0, [200]
  mov R1, __lbl_arr
  and R0, R1
  ret

__function_keep_imin:
  mov R0, [200]
  mov R1, __lbl_arr
  imin R0, R1
  ret

__function_keep_ilt:
  mov R0, [200]
  mov R1, __lbl_arr
  ilt R0, R1
  ret

__function_main:
  push BP
  mov BP, SP
  call __function_fire_mov
  call __function_fire_load
  call __function_fire_define_alu
  call __function_keep_iadd
  call __function_keep_isub
  call __function_keep_and
  call __function_keep_imin
  call __function_keep_ilt
  mov SP, BP
  pop BP
  ret

__lbl_arr:
  integer 1
  integer 2
