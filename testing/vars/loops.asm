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
;@src loops.c:3
  push BP
  mov BP, SP
;@var x loc=BP+2 type=int
;@src loops.c:3
  mov R0, [BP+2]
  iadd R0, 1
;@src loops.c:3
.return:
  mov SP, BP
  pop BP
  ret

;@src -
;@fn __function_lp lines=4..13 escapes=none
__function_lp:
;@src loops.c:4
  push BP
  mov BP, SP
  isub SP, 4
;@var n loc=BP+2 type=int
;@var i loc=BP-1 type=int
;@var s loc=BP-2 type=int
;@src loops.c:8
  mov R0, 0
  mov [BP-2], R0
;@src loops.c:9
  mov R0, 0
  mov [BP-1], R0
.for_1_start:
  mov R0, [BP-1]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, .for_1_end
  mov R0, [BP-2]
  mov R1, [BP-1]
  iadd R0, R1
  mov [BP-2], R0
  mov R0, [BP-2]
  mov R1, [BP-1]
  imul R1, 3
  iadd R0, R1
  mov [BP-2], R0
.for_1_continue:
  mov R0, [BP-1]
  mov R1, R0
  iadd R1, 1
  mov [BP-1], R1
  jmp .for_1_start
.for_1_end:
;@src loops.c:10
  mov R0, [BP-2]
  mov [SP], R0
  call __function_f
  mov [global_g], R0
;@src loops.c:11
  mov R0, 0
  mov [BP-1], R0
.for_2_start:
  mov R0, [BP-1]
  mov R1, [BP+2]
  ilt R0, R1
  jf R0, .for_2_end
;@scope {
;@var t loc=BP-3 type=int
  mov R0, [BP-1]
  imul R0, 2
  mov [BP-3], R0
  mov R0, [BP-2]
  mov R1, [BP-3]
  iadd R0, R1
  mov [BP-2], R0
  mov R0, [BP-2]
  mov R1, [BP-1]
  isub R0, R1
  mov [BP-2], R0
;@scope }
.for_2_continue:
  mov R0, [BP-1]
  mov R1, R0
  iadd R1, 1
  mov [BP-1], R1
  jmp .for_2_start
.for_2_end:
;@src loops.c:12
  mov R0, [BP-2]
  mov R1, [global_g]
  iadd R0, R1
;@src loops.c:13
.return:
  mov SP, BP
  pop BP
  ret

;@src -
;@fn __function_main lines=14..14 escapes=none
__function_main:
;@src loops.c:14
  push BP
  mov BP, SP
  isub SP, 1
;@src loops.c:14
  mov R0, 10
  mov [SP], R0
  call __function_lp
  mov [global_g], R0
;@src loops.c:14
.return:
  mov SP, BP
  pop BP
  ret

;@src -
