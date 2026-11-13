; dce-reach: a function reached only by falling into it. __function_b is never named by a CALL or a jump:
; main calls __function_a, which runs on into __function_b. dce (7dcd2e8) must keep __function_b (it dropped it).
; RAM[200] is 3. (hand-written, from /home/claude/work-t3/dce/fall.asm)
  call __function_main
  hlt
__function_a:
  mov R0, 1
__function_b:
  iadd R0, 2
  ret
__function_main:
  call __function_a
  mov [200], R0
  ret
