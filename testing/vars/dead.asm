;@lang c
;@src -
;@var g loc=global_g type=int
; program start section
  call __function_main
  hlt

; location of global variables
  %ifndef global_g
  %define global_g 1
  %endif

__function_main:
;@src e.c:2
  push BP
  mov BP, SP
  isub SP, 3
;@var i loc=BP-1 type=int
;@var junk loc=BP-2 type=int
;@src e.c:5
  mov R0, 0
  mov [BP-2], R0
;@var sum loc=BP-3 type=int
;@src e.c:6
  mov R0, 0
  mov [BP-3], R0
;@src e.c:7
  mov R0, 0
  mov [BP-1], R0
.for_1_start:
  mov R0, [BP-1]
  ilt R0, 10
  jf R0, .for_1_end
;@src e.c:9
  mov R0, [BP-2]
  mov R1, [BP-1]
  iadd R0, R1
  mov [BP-2], R0
;@src e.c:10
  mov R0, [BP-3]
  mov R1, [BP-1]
  iadd R0, R1
  mov [BP-3], R0
.for_1_continue:
  mov R0, [BP-1]
  mov R1, R0
  iadd R1, 1
  mov [BP-1], R1
  jmp .for_1_start
.for_1_end:
;@src e.c:12
  mov R0, [BP-3]
  mov [global_g], R0
;@src e.c:13
  mov R0, 0
;@src e.c:14
.return:
  mov SP, BP
  pop BP
  ret

;@src -
