; peephole-immediate-prop: 'mov R12, 300' ... 'iadd R12, 5' folds to 'mov R12, 305' unless something in between reads R12: MOVS does (it copies from [R12]); here it must run with SR=300, leaving 302, then 307. RAM[300],[301] hold 11, 12 first; the copy lands at 310.
; MOVS copies CR (R11) words from SR (R12) to DR (R13) and leaves CR=0, SR+=count, DR+=count:
; here R12 is 300 before and 302 after, R13 310 and 312, R11 2 and 0.
; program start section
  call __function_main
  hlt
__function_main:
  mov R0, 11
  mov [300], R0
  mov R0, 12
  mov [301], R0
  mov R12, 300
  mov R11, 2
  mov R13, 310
  movs
  iadd R12, 5
  mov R0, R12
  mov [400], R0
  mov R0, [310]
  mov [401], R0
  mov R0, [311]
  mov [402], R0
  ret
