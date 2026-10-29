; A function whose name ends in "_return" is a function, not the internal return label of another one.
;   get_return   real function, no "get" exists, reached only through its address (no hints)
;   bar_return   real function although "bar" (never called) exists too; only its ';@fn' line says so
;   foo_return   the ordinary case: internal label of function foo, a jump target inside it
;@hints 1 abi=caller-save
;@fn __function_bar_return
  call __function_main
  hlt
__function_main:
  mov R1, 4
  push R1
  call __function_foo
  iadd SP, 1
  mov R1, __function_get_return
  call R1
  mov R1, __function_bar_return
  call R1
  ret
__function_unused:
  mov R0, 9
  ret
__function_get_return:
  mov R0, 5
  ret
__function_foo:
  push BP
  mov BP, SP
  mov R0, [BP+2]
  jt R0, __function_foo_return
  mov R0, 1
__function_foo_return:
  mov SP, BP
  pop BP
  ret
__function_bar:
  mov R0, 6
  ret
__function_bar_return:
  mov R0, 7
  ret
