; _t is reached by falling out of _g (r6 = 5) and by a computed call through the data word
; _tab (r6 = 13). Its name is mentioned only by "pointer _t", never by a jump or call: the
; label table counts that as an address taken, so _t starts with every register unknown
; and "mov r2, r6" is not folded into "mov r2, 5".
    call _main
    hlt
_g:
    mov r6, 5
_t:
    mov r2, r6
    ret
_main:
    call _g
    mov [13], r2
    mov r6, 13
    mov r3, [_tab]
    call r3
    mov [14], r2
    ret
_tab:
    pointer _t
