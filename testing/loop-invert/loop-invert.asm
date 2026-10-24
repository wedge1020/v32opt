; ===================================================================
; TEST: loop-invert - top-tested loops get a bottom test
; Run with: ./v32opt loop-invert.asm -floop-invert -v
;
; The back edge "JMP start" becomes a copy of the test followed by
; "JT Rc, body". Functions whose name starts with inv_ must be inverted
; (once per loop); keep_ ones must be left alone. run.sh checks both.
; ===================================================================
    CALL __function_main
    HLT

__function_main:
    RET

; ✅ plain for loop
__function_inv_for:
    MOV R1, 0
__for_1_start:
    MOV R0, R1
    ILT R0, 10
    JF R0, __for_1_end
    IADD R2, R1
    IADD R1, 1
    JMP __for_1_start
__for_1_end:
    MOV R0, R2
    RET

; ✅ while loop with a continue (jumps to the top test) and a break
__function_inv_while_continue_break:
    MOV R1, 0
__while_2_start:
    MOV R0, R1
    ILT R0, 100
    JF R0, __while_2_end
    IADD R1, 1
    MOV R0, R1
    IEQ R0, 5
    JT R0, __while_2_start     ; continue: must still reach the top test
    MOV R0, R1
    IEQ R0, 50
    JT R0, __while_2_end       ; break
    IADD R2, R1
    JMP __while_2_start
__while_2_end:
    MOV R0, R2
    RET

; ✅ nested loops: both inverted
__function_inv_nested:
    MOV R1, 0
__for_3_start:
    MOV R0, R1
    ILT R0, 4
    JF R0, __for_3_end
    MOV R2, 0
__for_4_start:
    MOV R0, R2
    ILT R0, 4
    JF R0, __for_4_end
    IADD R3, R2
    IADD R2, 1
    JMP __for_4_start
__for_4_end:
    IADD R1, 1
    JMP __for_3_start
__for_3_end:
    MOV R0, R3
    RET

; ❌ the test calls a function: not duplicated
__function_keep_call_in_test:
    MOV R1, 0
__for_5_start:
    CALL __function_main
    MOV R0, R1
    ILT R0, 3
    JF R0, __for_5_end
    IADD R1, 1
    JMP __for_5_start
__for_5_end:
    RET

; ✅ short-circuit test: only the first condition is the test; the second
; stays in the body (the label is not inside the duplicated part)
__function_inv_first_condition_only:
    MOV R1, 0
__for_6_start:
    MOV R0, R1
    ILT R0, 10
    JF R0, __for_6_end
    MOV R0, R2
    ILT R0, 5
    JF R0, __for_6_end
__and_6:
    IADD R1, 1
    JMP __for_6_start
__for_6_end:
    RET

; ❌ the jump is not followed by the exit label
__function_keep_exit_mismatch:
    MOV R1, 0
__for_7_start:
    MOV R0, R1
    ILT R0, 10
    JF R0, __for_7_end
    IADD R1, 1
    JMP __for_7_start
    IADD R2, 1
__for_7_end:
    RET

; ❌ a forward jump to a test is not a back edge: nothing to gain
__function_keep_forward:
    JMP __fwd_start
__fwd_end:
    RET
__fwd_start:
    MOV R0, R1
    ILT R0, 10
    JF R0, __fwd_end
    RET

; ❌ test longer than the limit (8 instructions)
__function_keep_long_test:
    MOV R1, 0
__for_9_start:
    MOV R0, R1
    IADD R0, 1
    IADD R0, 1
    IADD R0, 1
    IADD R0, 1
    IADD R0, 1
    IADD R0, 1
    IADD R0, 1
    ILT R0, 100
    JF R0, __for_9_end
    IADD R1, 1
    JMP __for_9_start
__for_9_end:
    RET

; ❌ the same loop labels twice in the file: ambiguous, left alone
__function_keep_dup_a:
    MOV R1, 0
__dup_start:
    MOV R0, R1
    ILT R0, 10
    JF R0, __dup_end
    IADD R1, 1
    JMP __dup_start
__dup_end:
    RET

__function_keep_dup_b:
    MOV R1, 0
__dup_start:
    MOV R0, R1
    ILT R0, 10
    JF R0, __dup_end
    IADD R1, 1
    JMP __dup_start
__dup_end:
    RET

; ✅ a label already sits right after the JF: it is reused as the body label
__function_inv_existing_label:
    MOV R1, 0
__for_10_start:
    MOV R0, R1
    ILT R0, 10
    JF R0, __for_10_end
__for_10_body:
    IADD R1, 1
    JMP __for_10_start
__for_10_end:
    RET
