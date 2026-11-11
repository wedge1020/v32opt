; program start section
  call __function_main
  hlt

; Hand-written, compiler (callee-save) shape. Every function here must come out of
; frame-cleanup (omit-frame-pointers) unchanged: one refusal condition each, see
; the header of src/stack.c. main as in frame_remove.asm; keep_falls ends the
; program (it falls into a HLT), so it is called last. The never-taken JT at the
; start of main only keeps dce from dropping halt_stub, which nothing calls.

__function_keep_mentioned:
  push BP
  mov BP, SP
  push R4
  mov R4, [BP+2]
  mov R0, R4
  iadd R0, 1
__function_keep_mentioned_return:
  pop R4
  mov SP, BP
  pop BP
  ret

__function_clobber_r4:
  mov R4, 999
  mov R0, 1
  ret

__function_keep_call:
  push BP
  mov BP, SP
  push R4
  mov R0, [BP+2]
  mov R1, R0
  call __function_clobber_r4
  iadd R0, R1
__function_keep_call_return:
  pop R4
  mov SP, BP
  pop BP
  ret

__function_keep_sp_reach:
  push BP
  mov BP, SP
  push R4
  mov R0, [BP+2]
  mov R1, [SP]
  iadd R0, R1
__function_keep_sp_reach_return:
  pop R4
  mov SP, BP
  pop BP
  ret

__function_keep_bp_reach:
  push BP
  mov BP, SP
  isub SP, 1
  push R4
  mov R0, [BP+2]
  mov [BP-1], R0
  mov R1, [BP-2]
  iadd R0, R1
__function_keep_bp_reach_return:
  pop R4
  mov SP, BP
  pop BP
  ret

__function_keep_isub_bp:
  push BP
  mov BP, SP
  isub SP, 1
  mov R0, [BP+2]
  mov [BP-1], R0
  mov R1, [BP-1]
  iadd R0, R1
__function_keep_isub_bp_return:
  mov SP, BP
  pop BP
  ret

__function_twice:
  push BP
  mov BP, SP
  mov R0, [BP+2]
  iadd R0, R0
  mov SP, BP
  pop BP
  ret

__function_keep_isub_args:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, [BP+2]
  mov [SP], R0
  call __function_twice
  iadd R0, 1
__function_keep_isub_args_return:
  mov SP, BP
  pop BP
  ret

__function_keep_isub_args2:
  push BP
  mov BP, SP
  isub SP, 3
  mov R0, [BP+2]
  mov [BP-1], R0
  iadd R0, 1
  mov [SP+1], R0
  mov R0, [SP+1]
  mov R1, [BP-1]
  iadd R0, R1
__function_keep_isub_args2_return:
  mov SP, BP
  pop BP
  ret

__function_keep_label_depth:
  push BP
  mov BP, SP
  push R4
  mov R0, [BP+2]
  push R0
__kld_mid:
  pop R0
  iadd R0, 1
__function_keep_label_depth_return:
  pop R4
  mov SP, BP
  pop BP
  ret

__function_keep_directive:
  push BP
  mov BP, SP
  push R4
  mov R0, [BP+2]
  %define KEEP_DIR_X 1
  iadd R0, 1
__function_keep_directive_return:
  pop R4
  mov SP, BP
  pop BP
  ret

__function_keep_escape_bp:
  push BP
  mov BP, SP
  isub SP, 1
  push R4
  mov R0, [BP+2]
  lea R1, [BP-1]
  mov [R1], R0
  mov R0, [R1]
__function_keep_escape_bp_return:
  pop R4
  mov SP, BP
  pop BP
  ret

__function_keep_escape_sp:
  push BP
  mov BP, SP
  push R4
  mov R0, [BP+2]
  mov R1, SP
  mov R2, [R1]
  iadd R0, R2
__function_keep_escape_sp_return:
  pop R4
  mov SP, BP
  pop BP
  ret

__function_keep_sp_write:
  push BP
  mov BP, SP
  push R4
  mov R0, [BP+2]
  push R0
  imul SP, 1
  pop R0
__function_keep_sp_write_return:
  pop R4
  mov SP, BP
  pop BP
  ret

__function_keep_sp_write2:
  push BP
  mov BP, SP
  push R4
  mov R0, [BP+2]
  isub SP, R0
  iadd SP, R0
__function_keep_sp_write2_return:
  pop R4
  mov SP, BP
  pop BP
  ret

__function_keep_pop_order:
  push BP
  mov BP, SP
  push R4
  push R5
  mov R0, [BP+2]
  iadd R0, 1
__function_keep_pop_order_return:
  pop R4
  pop R5
  mov SP, BP
  pop BP
  ret

__function_keep_noframe:
  push R4
  mov R0, 5
  pop R4
  ret

__function_keep_r12:
  push BP
  mov BP, SP
  push R12
  mov R0, [BP+2]
  iadd R0, 2
__function_keep_r12_return:
  pop R12
  mov SP, BP
  pop BP
  ret

__function_keep_bp_label_ref:
  push BP
  mov BP, SP
  mov R0, 1
  jt R0, __kblr_x
  mov R0, 2
__kblr_x:
  iadd R0, 3
__function_keep_bp_label_ref_return:
  mov SP, BP
  pop BP
  ret

__function_keep_falls:
  push BP
  mov BP, SP
  push R4
  mov R0, [BP+2]
  jt R0, __kf_t
  pop R4
  mov SP, BP
  pop BP
  ret
__kf_t:
  mov [250], R0

__function_halt_stub:
  hlt

__function_main:
  push BP
  mov BP, SP
  isub SP, 1
  mov R7, 0
  jt R7, __function_halt_stub
  mov R4, 111
  mov R5, 222
  mov R6, 333
  mov R0, 3
  mov [SP], R0
  call __function_keep_mentioned
  mov [200], R0
  mov [300], R4
  mov [400], R5
  mov [500], R6
  mov R4, 111
  mov R5, 222
  mov R6, 333
  mov R0, 3
  mov [SP], R0
  call __function_keep_call
  mov [201], R0
  mov [301], R4
  mov [401], R5
  mov [501], R6
  mov R4, 111
  mov R5, 222
  mov R6, 333
  mov R0, 3
  mov [SP], R0
  call __function_keep_sp_reach
  mov [202], R0
  mov [302], R4
  mov [402], R5
  mov [502], R6
  mov R4, 111
  mov R5, 222
  mov R6, 333
  mov R0, 3
  mov [SP], R0
  call __function_keep_bp_reach
  mov [203], R0
  mov [303], R4
  mov [403], R5
  mov [503], R6
  mov R4, 111
  mov R5, 222
  mov R6, 333
  mov R0, 3
  mov [SP], R0
  call __function_keep_isub_bp
  mov [204], R0
  mov [304], R4
  mov [404], R5
  mov [504], R6
  mov R4, 111
  mov R5, 222
  mov R6, 333
  mov R0, 3
  mov [SP], R0
  call __function_keep_isub_args
  mov [205], R0
  mov [305], R4
  mov [405], R5
  mov [505], R6
  mov R4, 111
  mov R5, 222
  mov R6, 333
  mov R0, 3
  mov [SP], R0
  call __function_keep_isub_args2
  mov [206], R0
  mov [306], R4
  mov [406], R5
  mov [506], R6
  mov R4, 111
  mov R5, 222
  mov R6, 333
  mov R0, 3
  mov [SP], R0
  call __function_keep_label_depth
  mov [207], R0
  mov [307], R4
  mov [407], R5
  mov [507], R6
  mov R4, 111
  mov R5, 222
  mov R6, 333
  mov R0, 3
  mov [SP], R0
  call __function_keep_directive
  mov [208], R0
  mov [308], R4
  mov [408], R5
  mov [508], R6
  mov R4, 111
  mov R5, 222
  mov R6, 333
  mov R0, 3
  mov [SP], R0
  call __function_keep_escape_bp
  mov [209], R0
  mov [309], R4
  mov [409], R5
  mov [509], R6
  mov R4, 111
  mov R5, 222
  mov R6, 333
  mov R0, 3
  mov [SP], R0
  call __function_keep_escape_sp
  mov [210], R0
  mov [310], R4
  mov [410], R5
  mov [510], R6
  mov R4, 111
  mov R5, 222
  mov R6, 333
  mov R0, 3
  mov [SP], R0
  call __function_keep_sp_write
  mov [211], R0
  mov [311], R4
  mov [411], R5
  mov [511], R6
  mov R4, 111
  mov R5, 222
  mov R6, 333
  mov R0, 3
  mov [SP], R0
  call __function_keep_sp_write2
  mov [212], R0
  mov [312], R4
  mov [412], R5
  mov [512], R6
  mov R4, 111
  mov R5, 222
  mov R6, 333
  mov R0, 3
  mov [SP], R0
  call __function_keep_pop_order
  mov [213], R0
  mov [313], R4
  mov [413], R5
  mov [513], R6
  mov R4, 111
  mov R5, 222
  mov R6, 333
  mov R0, 3
  mov [SP], R0
  call __function_keep_noframe
  mov [214], R0
  mov [314], R4
  mov [414], R5
  mov [514], R6
  mov R4, 111
  mov R5, 222
  mov R6, 333
  mov R0, 3
  mov [SP], R0
  call __function_keep_r12
  mov [215], R0
  mov [315], R4
  mov [415], R5
  mov [515], R6
  mov R4, 111
  mov R5, 222
  mov R6, 333
  mov R0, 3
  mov [SP], R0
  call __function_keep_bp_label_ref
  mov [216], R0
  mov [316], R4
  mov [416], R5
  mov [516], R6
  mov R4, 111
  mov R5, 222
  mov R6, 333
  mov R0, 1
  mov [SP], R0
  call __function_keep_falls
  mov [217], R0
  mov [317], R4
  mov [417], R5
  mov [517], R6
  mov SP, BP
  pop BP
  ret
