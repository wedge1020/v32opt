; ===================================================================
; TEST: promote-* in the stock C compiler's callee-save convention (-L c)
;
; A callee pushes every register it uses right after "ISUB SP, N" and pops
; them before "MOV SP, BP". See run.sh for what is checked.
; ===================================================================
    CALL __function_main
    HLT

; A promoted local needs a register the function does not save yet: it gets
; a PUSH after the existing push and a POP before the existing pop.
__function_fresh:
  push BP
  mov BP, SP
  isub SP, 1
  push R1
  mov R0, 0
  mov [BP-1], R0
__for_1_start:
  mov R0, [BP-1]
  ilt R0, 100
  jf R0, __for_1_end
  mov R1, [BP-1]
  iadd R1, 2
  mov [BP-1], R1
  mov R0, [BP-1]
  iadd R0, 1
  mov [BP-1], R0
  jmp __for_1_start
__for_1_end:
  mov R0, [BP-1]
__function_fresh_return:
  pop R1
  mov SP, BP
  pop BP
  ret

; R2 is pushed but unused in the loop's region: it is free and already saved
; (its push reads the caller's value; the pop restores it).
__function_reuse:
  push BP
  mov BP, SP
  isub SP, 1
  push R2
  mov R0, 0
  mov [BP-1], R0
__for_2_start:
  mov R0, [BP-1]
  ilt R0, 100
  jf R0, __for_2_end
  mov R0, [BP-1]
  iadd R0, 3
  mov [BP-1], R0
  jmp __for_2_start
__for_2_end:
  mov R0, [BP-1]
__function_reuse_return:
  pop R2
  mov SP, BP
  pop BP
  ret

; Not every exit is a "MOV SP, BP / POP BP / RET" epilogue: nowhere to put
; a POP, so no register may be added.
__function_no_epilogue:
  push BP
  mov BP, SP
  isub SP, 1
  mov R0, 0
  mov [BP-1], R0
__for_3_start:
  mov R0, [BP-1]
  ilt R0, 100
  jf R0, __for_3_end
  mov R0, [BP-1]
  iadd R0, 1
  mov [BP-1], R0
  jmp __for_3_start
__for_3_end:
  mov R0, [BP-1]
__function_no_epilogue_return:
  ret

; A jump out of the function would skip the new POP.
__function_tail_jump:
  push BP
  mov BP, SP
  isub SP, 1
  mov R0, 0
  mov [BP-1], R0
__for_4_start:
  mov R0, [BP-1]
  ilt R0, 100
  jf R0, __for_4_end
  mov R0, [BP-1]
  iadd R0, 1
  mov [BP-1], R0
  jmp __for_4_start
__for_4_end:
  mov SP, BP
  pop BP
  jmp __function_main
__function_tail_jump_return:
  ret

; Two promoted locals, two registers saved once each.
__function_two_loops:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov [BP-2], R0
__for_5_start:
  mov R0, [BP-1]
  ilt R0, 100
  jf R0, __for_5_end
  mov R0, [BP-2]
  iadd R0, [BP-1]
  mov [BP-2], R0
  mov R0, [BP-1]
  iadd R0, 1
  mov [BP-1], R0
  jmp __for_5_start
__for_5_end:
  mov R0, [BP-2]
__function_two_loops_return:
  mov SP, BP
  pop BP
  ret

__function_main:
  push BP
  mov BP, SP
  call __function_fresh
  call __function_reuse
  call __function_no_epilogue
  call __function_two_loops
__function_main_return:
  mov SP, BP
  pop BP
  ret
