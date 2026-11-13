; program start section
  call __function_main
  hlt

; Hand-written: MOVS, SETS and CMPS write R11-R13 (count, source, destination) without
; naming them, so the repeated MOV R11/R12/R13 after each must stay (peephole-repeat-imm).
; Run with -fpeephole-repeat-imm only (peephole-movs used to delete these MOVs: fixed in 1aacb50).

__function_keep_movs:
  mov R11, 2
  mov R12, 300
  mov R13, 310
  movs
  mov R11, 2
  mov R12, 300
  mov R13, 310
  mov R0, R12
  iadd R0, R13
  ret

__function_keep_sets:
  mov R11, 2
  mov R12, 7
  mov R13, 310
  sets
  mov R11, 2
  mov R12, 7
  mov R13, 310
  mov R0, R13
  ret

__function_keep_cmps:
  mov R11, 2
  mov R12, 300
  mov R13, 310
  cmps R0
  mov R11, 2
  mov R12, 300
  mov R13, 310
  mov R0, R12
  iadd R0, R11
  ret

__function_main:
  push BP
  mov BP, SP
  isub SP, 1
  mov R7, 0
  call __function_keep_movs
  mov [400], R0
  mov R7, 0
  call __function_keep_sets
  mov [401], R0
  mov R7, 0
  call __function_keep_cmps
  mov [402], R0
  mov SP, BP
  pop BP
  ret
