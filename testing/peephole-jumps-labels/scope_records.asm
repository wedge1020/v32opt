; peephole-jumps (unreachable-code removal, commit 2f6a632): the code after an unconditional JMP up to the
; next label is dead and goes, but the compiler's ';@' record lines in it must stay, or a scope is left
; unclosed. Reduced from tests/audit/c6_misc.lua of v32lua (-g): "JMP L1 / ;@scope } / JMP L2" inside a
; scope. run.sh: --check-vars must exit 0 and the ';@scope }' line must still be in the output, with the
; second JMP gone.
; program start section
  call __function_main
  hlt
__function_main:
  push BP
  mov BP, SP
  isub SP, 2
  ;@var x loc=BP-1 type=lua
  ;@scope {
  mov R0, [200]
  jt R0, __sr_l1
  mov R0, 5
  mov [BP-1], R0
  jmp __sr_l1
  ;@scope }
  jmp __sr_l2
__sr_l1:
  mov R0, [BP-1]
  mov [201], R0
__sr_l2:
  mov SP, BP
  pop BP
  ret
