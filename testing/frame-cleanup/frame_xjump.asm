; program start section
  call __function_main
  hlt

; Hand-written, compiler shape: keep_bp_jump_out leaves for the epilogue of another
; function (a shared tail). omit-frame-pointers must keep its frame; keep_jump_out_save
; has an orphan save that the epilogue it jumps into pops: it must stay. Only run with
; -fomit-frame-pointers and -O2: -O3 inlines kbjo_b and drops the label the jump needs.

__function_keep_bp_jump_out:
  push BP
  mov BP, SP
  mov R0, 5
  jt R0, __kbjo_tail
  mov R0, 6
__function_keep_bp_jump_out_return:
  mov SP, BP
  pop BP
  ret

__function_keep_jump_out_save:
  push BP
  mov BP, SP
  push R4
  mov R0, [BP+2]
  jt R0, __kjos_tail
  mov R0, 3
__function_keep_jump_out_save_return:
  pop R4
  mov SP, BP
  pop BP
  ret

__function_kjos_b:
  push BP
  mov BP, SP
  push R4
  mov R4, 40
  mov R0, R4
__kjos_tail:
  pop R4
  mov SP, BP
  pop BP
  ret

__function_kbjo_b:
  push BP
  mov BP, SP
  mov R0, 9
__kbjo_tail:
  mov SP, BP
  pop BP
  ret

__function_main:
  push BP
  mov BP, SP
  isub SP, 1
  mov R4, 111
  mov R5, 222
  mov R6, 333
  mov R0, 3
  mov [SP], R0
  call __function_keep_bp_jump_out
  mov [200], R0
  mov [300], R4
  mov [400], R5
  mov [500], R6
  mov R4, 111
  mov R5, 222
  mov R6, 333
  mov R0, 0
  mov [SP], R0
  call __function_keep_jump_out_save
  mov [201], R0
  mov [301], R4
  mov [401], R5
  mov [501], R6
  mov R4, 111
  mov R5, 222
  mov R6, 333
  mov R0, 1
  mov [SP], R0
  call __function_keep_jump_out_save
  mov [202], R0
  mov [302], R4
  mov [402], R5
  mov [502], R6
  mov R4, 111
  mov R5, 222
  mov R6, 333
  mov R0, 3
  mov [SP], R0
  call __function_kjos_b
  mov [203], R0
  mov [303], R4
  mov [403], R5
  mov [503], R6
  mov R4, 111
  mov R5, 222
  mov R6, 333
  mov R0, 3
  mov [SP], R0
  call __function_kbjo_b
  mov [204], R0
  mov [304], R4
  mov [404], R5
  mov [504], R6
  mov SP, BP
  pop BP
  ret
