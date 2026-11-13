; ';@v' belongs on a load or a store. Here it is on two instructions that access no memory
; ("mov R1, 5" and "iadd R1, 1"): v32opt warns ONCE at parse time that the hint marks loads and
; stores only (commit afabd28) and never reports a volatile access "optimized away" for them. The
; instructions are still barriers. The one real volatile load stays and counts as kept.
  call __function_main
  hlt
__function_main:
  mov R1, 5 ;@v
  iadd R1, 1 ;@v
  mov R0, [200] ;@v
  mov [201], R1
  ret
