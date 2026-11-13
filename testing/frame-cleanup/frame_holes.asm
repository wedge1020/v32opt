; program start section
  call __function_main
  hlt

; Hand-written, compiler shape: the two holes next to the shared-tail one (commit d0814d8), each
; a function that another function jumps into. Run with -fomit-frame-pointers, -O1, -O2: -O3
; inlines the callees and drops the labels the jumps need.
;
; 1. (omit-frame-pointers only takes a frame nothing reads through BP: rl_a reads its argument
;    at [BP+2], so keeps its frame; rl_b takes its selector from RAM[500].)
;    rl_b has the ordinary "_return" label of a compiler (its early return jumps to it), and rl_a
;    jumps there too. omit-frame-pointers used to take rl_b's frame away (the label is its own);
;    rl_a then arrived at a bare RET with its frame still on the stack. It must keep rl_b's frame.
;
; 2. Stock (callee-save) convention: every register a function uses is saved by it. fs_b has five
;    locals in its ISUB area (promotable to registers, as the stock compiler's leaf functions have)
;    and saves R1 and R2; fs_a, which saved the same two, jumps to fs_b's epilogue "pop R2 / pop R1 /
;    mov SP, BP / pop BP / ret". promote would give fs_b fresh saves ("push R4" ...) and put their
;    POPs in the shared epilogue, where they would pop the words fs_a's R1 and R2 are in.
;    frame_info() must refuse the fresh saves. RAM[301], [401] are the caller's R1, R2 after the
;    call: 111 and 222 unless the epilogue went wrong.

__function_rl_a:
  push BP
  mov BP, SP
  mov R0, [BP+2]
  jt R0, __function_rl_b_return
  mov R0, 3
  mov SP, BP
  pop BP
  ret

__function_rl_b:
  push BP
  mov BP, SP
  mov R0, [500]
  jf R0, __function_rl_b_return
  mov R0, 9
__function_rl_b_return:
  mov SP, BP
  pop BP
  ret

__function_fs_a:
  push BP
  mov BP, SP
  push R1
  push R2
  mov R1, 5
  mov R2, 6
  mov R0, [BP+2]
  jt R0, __fs_b_tail
  mov R0, 1
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_fs_b:
  push BP
  mov BP, SP
  isub SP, 5
  push R1
  push R2
  mov R0, [BP+2]
  iadd R0, 1
  mov [BP-1], R0
  mov R0, [BP+2]
  iadd R0, 2
  mov [BP-2], R0
  mov R0, [BP+2]
  iadd R0, 3
  mov [BP-3], R0
  mov R0, [BP+2]
  iadd R0, 4
  mov [BP-4], R0
  mov R0, [BP+2]
  iadd R0, 5
  mov [BP-5], R0
  mov R0, [BP-1]
  mov R1, [BP-2]
  iadd R0, R1
  mov [BP-1], R0
  mov R0, [BP-2]
  mov R1, [BP-3]
  iadd R0, R1
  mov [BP-2], R0
  mov R0, [BP-3]
  mov R1, [BP-4]
  iadd R0, R1
  mov [BP-3], R0
  mov R0, [BP-4]
  mov R1, [BP-5]
  iadd R0, R1
  mov [BP-4], R0
  mov R0, [BP-5]
  mov R1, [BP-1]
  iadd R0, R1
  mov [BP-5], R0
  mov R0, [BP-1]
  mov R1, [BP-2]
  iadd R0, R1
  mov R1, [BP-3]
  iadd R0, R1
  mov R1, [BP-4]
  iadd R0, R1
  mov R1, [BP-5]
  iadd R0, R1
  mov R2, [BP-3]
  iadd R0, R2
__fs_b_tail:
  pop R2
  pop R1
  mov SP, BP
  pop BP
  ret

__function_main:
  push BP
  mov BP, SP
  isub SP, 1
  mov R1, 111
  mov R2, 222
  mov R0, 1
  mov [SP], R0
  call __function_rl_a
  mov [200], R0
  mov [300], R1
  mov [400], R2
  mov R0, 0
  mov [SP], R0
  call __function_rl_a
  mov [201], R0
  mov R0, 4
  mov [500], R0
  call __function_rl_b
  mov [202], R0
  mov R0, 0
  mov [500], R0
  call __function_rl_b
  mov [203], R0
  mov R1, 111
  mov R2, 222
  mov R0, 1
  mov [SP], R0
  call __function_fs_a
  mov [204], R0
  mov [301], R1
  mov [401], R2
  mov R1, 111
  mov R2, 222
  mov R0, 0
  mov [SP], R0
  call __function_fs_a
  mov [205], R0
  mov [302], R1
  mov [402], R2
  mov R1, 111
  mov R2, 222
  mov R0, 2
  mov [SP], R0
  call __function_fs_b
  mov [206], R0
  mov [303], R1
  mov [403], R2
  mov SP, BP
  pop BP
  ret
