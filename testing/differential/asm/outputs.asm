; A helper that returns a second result in R1 (like v32cc's unsigned divide):
; the write to R1 before its RET is not dead. Results go to RAM[0..].
    call _main
    hlt

_divmod:                        ; R0 = R0 / R1, R1 = R0 % R1 (remainder left in R1)
    mov r2, r0
    idiv r0, r1
    imod r2, r1
    mov r1, r2
    ret

_two:                           ; R0 = 1, R1 = 2, R4 = 3: three outputs
    mov r0, 1
    mov r1, 2
    mov r4, 3
    ret

_main:
    mov r0, 100
    mov r1, 7
    call _divmod
    mov [0], r0
    mov [1], r1
    call _two
    mov [2], r0
    mov [3], r1
    mov [4], r4
    mov r5, 9
    call _two                   ; R5 is not an output: callers' own value
    mov [5], r5
    ret
