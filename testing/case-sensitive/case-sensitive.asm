; Labels, symbols and %define names are case-sensitive (registers and mnemonics are not).
; C allows foo and Foo, global_x and global_X, _exit and _Exit; none may be mistaken for the other.
  call __function_main
  hlt

  %define global_x 1
  %define global_X 2
  %define global_out 3
  %define global_out2 4
  %define global_in 5

__function_foo:
  push BP
  mov BP, SP
  mov R0, [BP+2]
  imul R0, 3
.return:
  mov SP, BP
  pop BP
  ret

__function_Foo:
  push BP
  mov BP, SP
  mov R0, [BP+2]
  iadd R0, 7
.return:
  mov SP, BP
  pop BP
  ret

__function_Rate:
  push BP
  mov BP, SP
  mov R0, 5
.return:
  mov SP, BP
  pop BP
  ret

__function_main:
  push BP
  mov BP, SP
  isub SP, 2
  mov R0, [global_in]
  mov [SP], R0
  call __function_foo
  mov R1, R0
  mov R0, [global_in]
  mov [SP], R0
  call __function_Foo
  iadd R0, R1
; two different globals read back to back: two loads, not one
  mov R2, [global_x]
  mov R3, [global_X]
  iadd R0, R2
  iadd R0, R3
  mov [global_out], R0
  call __function_Rate
  mov [global_out2], R0
.return:
  mov SP, BP
  pop BP
  ret
