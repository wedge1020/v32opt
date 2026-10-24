; ===================================================================
; TEST: loop-unroll - counted loops with a constant trip count
; Run with: ./v32opt loop-unroll.asm -floop-unroll -v     (see run.sh)
;
; unr_* functions must be unrolled (their loop labels disappear), keep_*
; functions must be left alone.
; ===================================================================
    CALL __function_main
    HLT

__function_main:
    RET

__function_unr_up:
    MOV R4, 0
__u1_start:
    MOV R0, R4
    ILT R0, 4
    JF R0, __u1_end
    MOV R1, global_arr
    IADD R1, R4
    MOV [R1], R2
    IADD R4, 1
    JMP __u1_start
__u1_end:
    RET

__function_unr_down:
    MOV R4, 10
__u2_start:
    MOV R0, R4
    IGT R0, 0
    JF R0, __u2_end
    IADD R5, R4
    ISUB R4, 3
    JMP __u2_start
__u2_end:
    RET

__function_unr_continue_break:
    MOV R4, 0
__u3_start:
    MOV R0, R4
    ILT R0, 6
    JF R0, __u3_end
    MOV R1, R4
    IEQ R1, 2
    JT R1, __u3_continue
    MOV R1, R4
    IEQ R1, 4
    JT R1, __u3_end
    IADD R5, R4
__u3_continue:
    IADD R4, 1
    JMP __u3_start
__u3_end:
    MOV [0], R0
    RET

__function_unr_nested:
    MOV R4, 0
__u4_outer:
    MOV R0, R4
    ILT R0, 3
    JF R0, __u4_oend
    MOV R5, 0
__u4_inner:
    MOV R0, R5
    ILT R0, 3
    JF R0, __u4_iend
    IADD R6, R5
    IADD R5, 1
    JMP __u4_inner
__u4_iend:
    IADD R4, 1
    JMP __u4_outer
__u4_oend:
    RET

__function_unr_init_not_adjacent:
    MOV R4, 0
    MOV R6, 5
__u5_start:
    MOV R0, R4
    ILT R0, 3
    JF R0, __u5_end
    IADD R6, R4
    IADD R4, 1
    JMP __u5_start
__u5_end:
    RET

; ❌ the body writes the counter
__function_keep_counter_written:
    MOV R4, 0
__k1_start:
    MOV R0, R4
    ILT R0, 5
    JF R0, __k1_end
    IADD R4, 1
    IADD R4, 1
    JMP __k1_start
__k1_end:
    RET

; ❌ a CALL in the body
__function_keep_call:
    MOV R4, 0
__k2_start:
    MOV R0, R4
    ILT R0, 3
    JF R0, __k2_end
    CALL __function_main
    IADD R4, 1
    JMP __k2_start
__k2_end:
    RET

; ❌ more than 16 trips
__function_keep_many_trips:
    MOV R4, 0
__k3_start:
    MOV R0, R4
    ILT R0, 17
    JF R0, __k3_end
    IADD R5, R4
    IADD R4, 1
    JMP __k3_start
__k3_end:
    RET

; ❌ the bound is not a constant
__function_keep_register_bound:
    MOV R4, 0
__k4_start:
    MOV R0, R4
    ILT R0, R6
    JF R0, __k4_end
    IADD R5, R4
    IADD R4, 1
    JMP __k4_start
__k4_end:
    RET

; ❌ something else jumps to the start label (a "continue" in a while loop)
__function_keep_other_ref_to_start:
    MOV R4, 0
__k5_start:
    MOV R0, R4
    ILT R0, 4
    JF R0, __k5_end
    IADD R4, 1
    IEQ R1, 1
    JT R1, __k5_start
    IADD R5, R4
    JMP __k5_start
__k5_end:
    RET

; ❌ a jump from outside into the middle of the body
__function_keep_jump_into_body:
    JMP __k6_mid
    MOV R4, 0
__k6_start:
    MOV R0, R4
    ILT R0, 4
    JF R0, __k6_end
__k6_mid:
    IADD R5, R4
    IADD R4, 1
    JMP __k6_start
__k6_end:
    RET

; ❌ the init is not a constant
__function_keep_init_not_constant:
    MOV R4, R7
__k7_start:
    MOV R0, R4
    ILT R0, 4
    JF R0, __k7_end
    IADD R5, R4
    IADD R4, 1
    JMP __k7_start
__k7_end:
    RET

; ❌ there is a branch between the init and the loop
__function_keep_init_across_branch:
    MOV R4, 0
    JT R1, __k8_start
    MOV R4, 2
__k8_start:
    MOV R0, R4
    ILT R0, 4
    JF R0, __k8_end
    IADD R5, R4
    IADD R4, 1
    JMP __k8_start
__k8_end:
    RET
