; Two globals whose names share their first 31 characters. A pass that compares
; operand names cut at 31 characters sees "mov [..._angle], R0" as overwritten by
; "mov [..._angleVel], R0" and deletes the first store. (Found in crisp's Ricochetpins.)
%define global_a_very_long_symbol_name_angle 4000
%define global_a_very_long_symbol_name_angleVel 4001
%define global_result 4002

__function_main:
  mov R0, 7
  mov [global_a_very_long_symbol_name_angle], R0
  mov R0, 9
  mov [global_a_very_long_symbol_name_angleVel], R0
  mov R0, [global_a_very_long_symbol_name_angle]
  mov [global_result], R0
  hlt
