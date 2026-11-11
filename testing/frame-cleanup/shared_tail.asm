; KNOWN WRONG RESULT, not part of run.sh (see ISSUES): function a jumps into the epilogue of
; function b (a shared tail). Both have "push R4"; a really uses R4, b does not, so frame
; cleanup removes b's PUSH R4 / POP R4 and the jump from a then leaves a's R4 unrestored:
; RAM[201] is 111 unoptimized and 5 with -fomit-frame-pointers (and so -O2, -O3).
; (-O2/-O3 also fail to assemble: dce drops function b, which nothing calls.)
; Check:  cd testing/differential && STRICT=1 python3 run.py --flags "-fomit-frame-pointers" ../frame-cleanup/shared_tail.asm
; program start section
  call __function_main
  hlt

__function_a:
  push BP
  mov BP, SP
  push R4
  mov R4, 5
  mov R0, [BP+2]
  jt R0, __b_tail
  mov R0, 1
  pop R4
  mov SP, BP
  pop BP
  ret

__function_b:
  push BP
  mov BP, SP
  push R4
  mov R0, 2
__b_tail:
  pop R4
  mov SP, BP
  pop BP
  ret

__function_main:
  push BP
  mov BP, SP
  isub SP, 1
  mov R4, 111
  mov R0, 1
  mov [SP], R0
  call __function_a
  mov [200], R0
  mov [201], R4
  mov SP, BP
  pop BP
  ret
