; dce-reach: reachability through a data word, and the control that dead code still goes.
;   __function_cb     named only by "pointer __function_cb" in the table, reached by a computed CALL:
;                     must stay (RAM[200] is 7)
;   __function_dead   named by nothing: must be removed
;   __function_dead2  named only by __function_dead: must be removed with it (unreachable from main)
;   __function_calls  called from main: stays (RAM[201] is 40), and so does its callee __function_leaf
  call __function_main
  hlt
__function_main:
  mov R3, [__cb_tab]
  call R3
  mov [200], R0
  call __function_calls
  mov [201], R0
  ret
__function_cb:
  mov R0, 7
  ret
__function_dead:
  mov R0, 99
  call __function_dead2
  ret
__function_dead2:
  mov R0, 98
  ret
__function_calls:
  call __function_leaf
  imul R0, 4
  ret
__function_leaf:
  mov R0, 10
  ret
__cb_tab:
  pointer __function_cb
