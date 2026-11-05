;@hints 1 abi=caller-save
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

;@fn __function_f lines=3..3 noinline leaf escapes=none
__function_f:
;@src segment.c:3
  push BP
  mov BP, SP
;@var x loc=BP+2 type=int
;@src segment.c:3
  mov R0, [BP+2]
  iadd R0, 1
;@src segment.c:3
.return:
  mov SP, BP
  pop BP
  ret

;@src -
;@fn __function_seg lines=4..12 escapes=none
__function_seg:
;@src segment.c:4
  push BP
  mov BP, SP
  isub SP, 3
;@var n loc=BP+2 type=int
;@var a loc=BP-1 type=int
;@src segment.c:6
  mov R0, [BP+2]
  mov [BP-1], R0
;@var b loc=BP-2 type=int
;@src segment.c:8
  mov R0, [BP-1]
  iadd R0, 1
  mov [BP-1], R0
  mov R0, [BP-1]
  imul R0, 2
  mov [BP-1], R0
  mov R0, [BP-1]
  iadd R0, 3
  mov [BP-1], R0
;@src segment.c:9
  mov R0, [BP-1]
  mov [SP], R0
  call __function_f
  mov [BP-2], R0
;@src segment.c:10
  mov R0, [BP-2]
  iadd R0, 1
  mov [BP-1], R0
  mov R0, [BP-1]
  iadd R0, 2
  mov [BP-1], R0
  mov R0, [BP-1]
  iadd R0, 3
  mov [BP-1], R0
;@src segment.c:11
  mov R0, [BP-1]
  mov R1, [BP-2]
  iadd R0, R1
;@src segment.c:12
.return:
  mov SP, BP
  pop BP
  ret

;@src -
;@fn __function_main lines=13..13 escapes=none
__function_main:
;@src segment.c:13
  push BP
  mov BP, SP
  isub SP, 1
;@src segment.c:13
  mov R0, 4
  mov [SP], R0
  call __function_seg
  mov [global_g], R0
;@src segment.c:13
.return:
  mov SP, BP
  pop BP
  ret

;@src -
