; program start section
  call __function_main
  hlt

; Hand-written: JMP over a run of labels (peephole-jumps, commit 6e5873c; "L2(i)" of doc/lua-loops.md).
; "JMP L" is deleted when only labels, comments and blank lines lie between it and L: the jump
; lands on the next instruction anyway. run.sh says what each function must become.
;   fire_*  the JMP goes (over one, two, three labels; with comments and blank lines in the run;
;           the labels stay, one of them is also a JT target)
;   keep_*  unchanged: an instruction between, a directive between (it ends the run), JT and JF
;           (they keep the one-label form)
; main stores each R0 (R7 chooses the branch of the keep_* functions, 0 and 1).

__function_fire_one:
  mov R0, 1
  jmp __fo_a
__fo_a:
  iadd R0, 10
  ret

__function_fire_two:
  mov R0, 2
  jt R7, __ft_a
  mov R1, 0
  jmp __ft_b
__ft_a:
__ft_b:
  iadd R0, 10
  ret

__function_fire_three:
  mov R0, 3
  jmp __fh_c
__fh_a:
__fh_b:
__fh_c:
  iadd R0, 10
  ret

__function_fire_comments:
  mov R0, 4
  jmp __fc_b
  ; a comment in the run

__fc_a:
  ; another one

__fc_b:
  iadd R0, 10
  ret

__function_keep_instr:
  mov R0, 5
  jmp __ki_b
__ki_a:
  iadd R0, 100
__ki_b:
  iadd R0, 10
  ret

__function_keep_directive:
  mov R0, 6
  jmp __kd_b
__kd_a:
%define KEEP_JL_X 1
__kd_b:
  iadd R0, 10
  ret

__function_keep_jt:
  mov R0, 7
  jt R7, __kj_b
__kj_a:
__kj_b:
  iadd R0, 10
  ret

__function_keep_jf:
  mov R0, 8
  jf R7, __kf_b
__kf_a:
__kf_b:
  iadd R0, 10
  ret

__function_main:
  push BP
  mov BP, SP
  mov R7, 0
  call __function_fire_one
  mov [400], R0
  mov R7, 0
  call __function_fire_two
  mov [401], R0
  mov R7, 0
  call __function_fire_three
  mov [402], R0
  mov R7, 0
  call __function_fire_comments
  mov [403], R0
  mov R7, 0
  call __function_keep_instr
  mov [404], R0
  mov R7, 0
  call __function_keep_directive
  mov [405], R0
  mov R7, 0
  call __function_keep_jt
  mov [406], R0
  mov R7, 1
  call __function_keep_jt
  mov [407], R0
  mov R7, 0
  call __function_keep_jf
  mov [408], R0
  mov R7, 1
  call __function_keep_jf
  mov [409], R0
  mov SP, BP
  pop BP
  ret
