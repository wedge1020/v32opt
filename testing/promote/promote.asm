; ===================================================================
; TEST: promote-leaf / promote-regs / promote-loops
;
; One small function per scenario, in the shape the Vircon32 C compiler
; produces (push BP / mov BP, SP / isub SP, N ... mov SP, BP / pop BP).
; run.sh optimizes this file with the three passes, in -d mode, and checks
; per function whether (and how) it was promoted.
;
;   PROMOTES-xxx : the pass must promote something in this function
;   KEEPS-xxx    : the function must come out untouched
; ===================================================================
  call __function_f_main
  hlt

; PROMOTES-LOOPS: a plain counting loop. The preheader load goes BEFORE
; the loop label, the store AFTER the exit label, and nothing is stored
; back before the epilogue (the frame dies there).
__function_loop_basic:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov [BP-2], R0
__for_1_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_1_end
  mov R0, [BP-1]
  iadd R0, 3
  mov [BP-1], R0
  mov R0, [BP-2]
  iadd R0, 1
  mov [BP-2], R0
  jmp __for_1_start
__for_1_end:
  mov R0, [BP-1]
__function_loop_basic_return:
  mov SP, BP
  pop BP
  ret

; PROMOTES-LOOPS, no load: [BP-2] is a scratch slot the loop always writes
; before it reads and nothing reads after the loop. It gets a register with
; neither a preheader load (the loaded value would never be used) nor a store
; at the exit; [BP-1] still needs both (read first, read again afterwards).
__function_loop_temp:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
__for_7_start:
  mov R0, [BP-1]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_7_end
  mov R0, [BP-1]
  imul R0, 3
  mov [BP-2], R0
  mov R0, [BP-2]
  iadd R0, 1
  mov [BP-2], R0
  mov R0, [BP-1]
  iadd R0, R0
  mov [BP-1], R0
  jmp __for_7_start
__for_7_end:
  mov R0, [BP-1]
__function_loop_temp_return:
  mov SP, BP
  pop BP
  ret

; PROMOTES-LOOPS, store kept: [BP-1] is only read inside the inner loop, never
; after it -- but the inner loop is entered again by the outer one, and its
; preheader load must find the accumulated value in memory. The store at the
; inner loop's exit therefore stays (the outer loop has a CALL, so it is the
; inner loop that gets promoted).
__function_reentered:
  push BP
  mov BP, SP
  isub SP, 3
  mov R0, 0
  mov [BP-1], R0
  mov [BP-2], R0
__for_8_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_8_end
  call __function_leaf_helper
  mov R0, 0
  mov [BP-3], R0
__for_9_start:
  mov R0, [BP-3]
  ilt R0, 4
  jf R0, __for_9_end
  mov R0, [BP-1]
  mov R1, [BP-3]
  iadd R0, R1
  mov [BP-1], R0
  mov [global_g], R0
  mov R0, [BP-3]
  iadd R0, 1
  mov [BP-3], R0
  jmp __for_9_start
__for_9_end:
  mov R0, [BP-2]
  iadd R0, 1
  mov [BP-2], R0
  jmp __for_8_start
__for_8_end:
  mov R0, 0
__function_reentered_return:
  mov SP, BP
  pop BP
  ret

; KEEPS-ALIAS: the address of [BP-1] is taken BEFORE the loop; the loop then
; reads and writes the slot directly and through the pointer. Promoting
; [BP-1] would lose the writes made through R3 (the old pass only looked
; for address-taking inside the loop body).
__function_alias_before_loop:
  push BP
  mov BP, SP
  isub SP, 3
  mov R0, 0
  mov [BP-1], R0
  mov [BP-2], R0
  lea R3, [BP-1]
  mov [BP-3], R3
__for_2_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_2_end
  mov R0, [BP-1]
  iadd R0, 1
  mov [BP-1], R0
  mov R3, [BP-3]
  mov R0, [R3]
  iadd R0, 10
  mov [R3], R0
  mov R0, [BP-2]
  iadd R0, 1
  mov [BP-2], R0
  jmp __for_2_start
__for_2_end:
  mov R0, [BP-1]
__function_alias_before_loop_return:
  mov SP, BP
  pop BP
  ret

; KEEPS-GOTO: a jump from outside lands on a label inside the loop body, which
; would skip the preheader load.
__function_goto_into_loop:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov [BP-2], R0
  jmp __mid_3
__for_3_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_3_end
  mov R0, [BP-1]
  iadd R0, 1
  mov [BP-1], R0
__mid_3:
  mov R0, [BP-1]
  iadd R0, 10
  mov [BP-1], R0
  mov R0, [BP-2]
  iadd R0, 1
  mov [BP-2], R0
  jmp __for_3_start
__for_3_end:
  mov R0, [BP-1]
__function_goto_into_loop_return:
  mov SP, BP
  pop BP
  ret

; KEEPS-EXIT: something outside the loop also branches to its exit label; the
; exit store would run without the preheader load.
__function_shared_exit:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0
  mov [BP-2], R0
  mov R0, [BP+2]
  jf R0, __for_4_end
__for_4_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_4_end
  mov R0, [BP-1]
  iadd R0, 3
  mov [BP-1], R0
  mov R0, [BP-2]
  iadd R0, 1
  mov [BP-2], R0
  jmp __for_4_start
__for_4_end:
  mov R0, [BP-1]
__function_shared_exit_return:
  mov SP, BP
  pop BP
  ret

; PROMOTES-LOOPS: the outer loop contains a CALL and cannot be promoted, but
; the loop nested in it can (the old pass skipped the whole outer loop,
; inner loops included).
__function_outer_call:
  push BP
  mov BP, SP
  isub SP, 4
  mov R0, 0
  mov [BP-1], R0
  mov [BP-2], R0
__for_5_start:
  mov R0, [BP-2]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, __for_5_end
  call __function_leaf_helper
  mov R0, 0
  mov [BP-3], R0
__for_6_start:
  mov R0, [BP-3]
  ilt R0, 8
  jf R0, __for_6_end
  mov R0, [BP-1]
  iadd R0, 2
  mov [BP-1], R0
  mov R0, [BP-3]
  iadd R0, 1
  mov [BP-3], R0
  jmp __for_6_start
__for_6_end:
  mov R0, [BP-2]
  iadd R0, 1
  mov [BP-2], R0
  jmp __for_5_start
__for_5_end:
  mov R0, [BP-1]
__function_outer_call_return:
  mov SP, BP
  pop BP
  ret

__function_leaf_helper:
  push BP
  mov BP, SP
  mov R0, 1
__function_leaf_helper_return:
  mov SP, BP
  pop BP
  ret

; PROMOTES-REGS: [BP-1] is used three times after the call. R2 and R3 carry
; the callee's extra return values through this function untouched (v32lua
; multi-value returns), so neither may be picked as the promotion register
; -- not even though R1 is taken here and R2 is the next one nothing in the
; segment mentions.
__function_passthrough:
  push BP
  mov BP, SP
  isub SP, 2
  call __function_leaf_helper
  mov R1, 7
  mov R0, [BP-1]
  iadd R0, R1
  mov [BP-1], R0
  mov R0, [BP-1]
  imul R0, 2
  mov [BP-1], R0
__function_passthrough_return:
  mov SP, BP
  pop BP
  ret

; KEEPS-PARAM: parameters ([BP+N]) are never promoted -- a register copy of one
; would cost a load and save nothing -- read-only ...
__function_param_ro:
  push BP
  mov BP, SP
  mov R0, [BP+2]
  iadd R0, [BP+2]
  imul R0, [BP+2]
__function_param_ro_return:
  mov SP, BP
  pop BP
  ret

; ... or written.
__function_param_rw:
  push BP
  mov BP, SP
  mov R0, [BP+2]
  iadd R0, 1
  mov [BP+2], R0
  mov R0, [BP+2]
  iadd R0, [BP+2]
__function_param_rw_return:
  mov SP, BP
  pop BP
  ret

; KEEPS-SP: the stack pointer is copied into a register, so a pointer into the
; frame may exist.
__function_sp_escape:
  push BP
  mov BP, SP
  isub SP, 1
  mov R4, SP
  mov R0, [BP-1]
  iadd R0, 1
  mov [BP-1], R0
  mov R0, [BP-1]
  iadd R0, [BP-1]
__function_sp_escape_return:
  mov SP, BP
  pop BP
  ret

; KEEPS-NOFRAME: no "MOV BP, SP" -- there is no frame of its own to promote
; slots of.
__function_naked:
  mov R0, [BP-1]
  iadd R0, 1
  mov [BP-1], R0
  mov R0, [BP-1]
  iadd R0, [BP-1]
  ret

__function_f_main:
  push BP
  mov BP, SP
  isub SP, 1
  mov R0, 5
  mov [SP], R0
  call __function_loop_basic
  mov R0, 5
  mov [SP], R0
  call __function_reentered
  mov R0, 5
  mov [SP], R0
  call __function_loop_temp
  mov R0, 5
  mov [SP], R0
  call __function_alias_before_loop
  mov R0, 5
  mov [SP], R0
  call __function_goto_into_loop
  mov R0, 5
  mov [SP], R0
  call __function_shared_exit
  mov R0, 5
  mov [SP], R0
  call __function_outer_call
  call __function_passthrough
  mov R0, 5
  mov [SP], R0
  call __function_param_ro
  mov R0, 5
  mov [SP], R0
  call __function_param_rw
  call __function_sp_escape
  call __function_naked
__function_f_main_return:
  mov SP, BP
  pop BP
  ret

%define global_g 1
