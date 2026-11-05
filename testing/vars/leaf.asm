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

;@fn __function_leafsum lines=14..21 leaf escapes=none
__function_leafsum:
;@src leaf.c:14
  push BP
  mov BP, SP
  isub SP, 2
;@var n loc=BP+2 type=int
;@var i loc=BP-1 type=int
;@var s loc=BP-2 type=int
;@src leaf.c:18
  mov R0, 0
  mov [BP-2], R0
;@src leaf.c:19
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
.for_1_continue:
  mov R0, [BP-1]
  mov R1, R0
  iadd R1, 1
  mov [BP-1], R1
  jmp .for_1_start
.for_1_end:
;@src leaf.c:20
  mov R0, [BP-2]
;@src leaf.c:21
.return:
  mov SP, BP
  pop BP
  ret

;@src -
;@fn __function_main lines=22..22 escapes=none
__function_main:
;@src leaf.c:22
  push BP
  mov BP, SP
  isub SP, 1
;@src leaf.c:22
  mov R0, 10
  mov [SP], R0
  call __function_leafsum
  mov [global_g], R0
;@src leaf.c:22
.return:
  mov SP, BP
  pop BP
  ret

;@src -
