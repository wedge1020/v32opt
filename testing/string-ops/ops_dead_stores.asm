; peephole-dead-stores: SETS reads R11 (count), R12 (value) and R13 (address) without naming them: 'mov R11, 3' before it is not overwritten-unread by the 'mov R11, 2' after it. RAM[400..402] read back what SETS stored at 300..302 (7 each); the second SETS (alias spellings) stores 9 at 310, 311.
; MOVS copies CR (R11) words from SR (R12) to DR (R13) and leaves CR=0, SR+=count, DR+=count:
; here R12 is 300 before and 302 after, R13 310 and 312, R11 2 and 0.
; program start section
  call __function_main
  hlt
__function_main:
  mov R11, 3
  mov R12, 7
  mov R13, 300
  sets
  mov R11, 2
  mov SR, 9
  mov DR, 310
  sets
  mov R0, [300]
  mov [400], R0
  mov R0, [301]
  mov [401], R0
  mov R0, [302]
  mov [402], R0
  mov R0, [310]
  mov [403], R0
  mov R0, [311]
  mov [404], R0
  ret
