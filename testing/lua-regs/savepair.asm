    ;@hints 1 abi=caller-save lang=lua
    CALL  __user
    HLT

;; R1 is an argument of __f. __f saves it around a call ("PUSH R1 / CALL __g / POP R1") and then reads it:
;; the MOV R1 below (ARG-1) is an input, not a dead store.
__user:
    PUSH  BP
    MOV   BP, SP
    MOV   R1, 7                   ; ARG-1
    CALL  __f
    MOV   [9], R0
    MOV   SP, BP
    POP   BP
    RET

__f:
    PUSH  R2
    PUSH  R1
    CALL  __g
    POP   R1
    MOV   R0, R1                  ; the caller's R1, given back by the POP
    POP   R2
    RET

__g:
    MOV   R1, 3
    RET
