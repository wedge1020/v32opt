;@hints 1 abi=caller-save
;@fn __function_main escapes=none
;@fn __function_vloc escapes=none
; Accesses marked volatile must survive every pass.
  call __function_main
  hlt

  %define global_flag 1
  %define global_out  2

__function_main:
  push BP
  mov BP, SP
; (a) two loads of the same word: must stay two loads
  mov R0, [global_flag]   ;@v
  mov R1, [global_flag]   ;@v
  iadd R0, R1
  mov [global_out], R0
; (b) store then load of the same word: the load must still read memory
  mov R2, 7
  mov [global_flag], R2   ;@v
  mov R0, [global_flag]   ;@v
  mov [global_out], R0
; (c) two stores in a row: the first one must still happen
  mov R1, 1
  mov [global_flag], R1   ;@v
  mov R2, 2
  mov [global_flag], R2   ;@v
; (d) a load whose result is never used: must still happen
  mov R3, [global_flag]   ;@v
; (e) the same expression computed from the same load twice
  mov R1, [global_flag]   ;@v
  iadd R1, 5
  mov R2, [global_flag]   ;@v
  iadd R2, 5
  iadd R1, R2
  mov [global_out], R1
  call __function_vloc
.return:
  mov SP, BP
  pop BP
  ret

; (f) a volatile local: never kept in a register, even in a loop
__function_vloc:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, 0
  mov [BP-1], R0          ;@v
  mov R0, 0
  mov [BP-2], R0
.vloc_start:
  mov R0, [BP-2]
  ilt R0, 10
  jf R0, .vloc_end
  mov R0, [BP-1]          ;@v
  iadd R0, 1
  mov [BP-1], R0          ;@v
  mov R0, [BP-2]
  iadd R0, 1
  mov [BP-2], R0
  jmp .vloc_start
.vloc_end:
  mov R0, [BP-1]          ;@v
  mov [global_out], R0
.return:
  mov SP, BP
  pop BP
  ret
