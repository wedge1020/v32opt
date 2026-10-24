; Registers read by callees, saved by callees, and passed through them.
; Results go to RAM[0..]; run.py compares the whole image.
    call _main
    hlt

_readsr5:                        ; reads R5 (an argument in a register)
    mov r0, r5
    iadd r0, 1
    ret

_savesr4:                        ; stock style: saves what it uses
    push bp
    mov bp, sp
    push r4
    mov r4, 99
    iadd r4, 1
    pop r4
    mov r0, 5
    mov sp, bp
    pop bp
    ret

_clobbers:                       ; caller-save style: trashes R6
    mov r6, 1234
    mov r0, 0
    ret

_passthrough:
    call _readsr5
    ret

_main:
    push bp
    mov bp, sp
    mov r5, 10
    mov r4, 77
    mov r6, 5
    call _readsr5                ; R5 is an input of the call
    mov [0], r0
    call _savesr4                ; R4 survives
    mov [1], r4
    mov r1, r4                  ; copy that must outlive the call
    call _savesr4
    iadd r1, 1
    mov r4, r1
    mov [2], r4
    mov r5, 3
    mov r5, 4                   ; first write is dead, second is read by the callee
    call _passthrough
    mov [3], r0
    mov r6, 5
    call _clobbers               ; R6 is overwritten: the write above is dead
    mov [4], r0
    mov r7, 8
    mov r2, r7                  ; R2 copy
    iadd r2, 1
    mov r7, r2
    mov [5], r7
    mov sp, bp
    pop bp
    ret
