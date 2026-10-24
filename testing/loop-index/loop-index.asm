; ===================================================================
; TEST: loop-index - fold "base + i" into the counter
; Run with: ./v32opt loop-index.asm -floop-index -v     (see run.sh)
;
; fold_* functions must get "IADD R4, global_a" in front of the loop and
; the pairs "MOV Rt, global_a / IADD Rt, R4" turned into "MOV Rt, R4";
; keep_* functions must be left alone.
; ===================================================================
%define global_a 100
%define global_b 300
    CALL __function_main
    HLT

__function_main:
    CALL __function_fold_one
    CALL __function_fold_two_uses
    CALL __function_fold_early_exit_dead
    CALL __function_keep_value_use
    CALL __function_keep_two_bases
    CALL __function_keep_live_after
    CALL __function_keep_call
    CALL __function_keep_register_bound
    CALL __function_keep_exit_target_uses_counter
    CALL __function_keep_no_pair
    RET

__function_fold_one:
    MOV R4, 0
__fold_one_start:
    MOV R0, R4
    ILT R0, 64
    JF R0, __fold_one_end
    MOV R1, global_a
    IADD R1, R4
    MOV R2, [R1]
    IADD R5, R2
    IADD R4, 1
    JMP __fold_one_start
__fold_one_end:
    RET

__function_fold_two_uses:
    MOV R4, 0
__fold_two_uses_start:
    MOV R0, R4
    ILT R0, 64
    JF R0, __fold_two_uses_end
    MOV R1, global_a
    IADD R1, R4
    MOV R2, [R1]
    MOV R1, global_a
    IADD R1, R4
    MOV [R1], R2
    IADD R4, 1
    JMP __fold_two_uses_start
__fold_two_uses_end:
    RET

__function_fold_early_exit_dead:
    MOV R4, 0
__fold_early_exit_dead_start:
    MOV R0, R4
    ILT R0, 64
    JF R0, __fold_early_exit_dead_end
    MOV R1, global_a
    IADD R1, R4
    MOV R2, [R1]
    IEQ R2, 7
    JT R2, __fold_early_exit_dead_end
    IADD R4, 1
    JMP __fold_early_exit_dead_start
__fold_early_exit_dead_end:
    RET

__function_keep_value_use:
    MOV R4, 0
__keep_value_use_start:
    MOV R0, R4
    ILT R0, 64
    JF R0, __keep_value_use_end
    MOV R1, global_a
    IADD R1, R4
    MOV [R1], R4
    IADD R4, 1
    JMP __keep_value_use_start
__keep_value_use_end:
    RET

__function_keep_two_bases:
    MOV R4, 0
__keep_two_bases_start:
    MOV R0, R4
    ILT R0, 64
    JF R0, __keep_two_bases_end
    MOV R1, global_a
    IADD R1, R4
    MOV R2, [R1]
    MOV R1, global_b
    IADD R1, R4
    MOV [R1], R2
    IADD R4, 1
    JMP __keep_two_bases_start
__keep_two_bases_end:
    RET

__function_keep_live_after:
    MOV R4, 0
__keep_live_after_start:
    MOV R0, R4
    ILT R0, 64
    JF R0, __keep_live_after_end
    MOV R1, global_a
    IADD R1, R4
    MOV R2, [R1]
    IADD R4, 1
    JMP __keep_live_after_start
__keep_live_after_end:
    MOV R0, R4
    RET

__function_keep_call:
    MOV R4, 0
__keep_call_start:
    MOV R0, R4
    ILT R0, 64
    JF R0, __keep_call_end
    MOV R1, global_a
    IADD R1, R4
    MOV R2, [R1]
    CALL __function_main
    IADD R4, 1
    JMP __keep_call_start
__keep_call_end:
    RET

__function_keep_register_bound:
    MOV R4, 0
__keep_register_bound_start:
    MOV R0, R4
    ILT R0, R6
    JF R0, __keep_register_bound_end
    MOV R1, global_a
    IADD R1, R4
    MOV R2, [R1]
    IADD R4, 1
    JMP __keep_register_bound_start
__keep_register_bound_end:
    RET

__function_keep_exit_target_uses_counter:
    MOV R4, 0
__keep_exit_target_uses_counter_start:
    MOV R0, R4
    ILT R0, 64
    JF R0, __keep_exit_target_uses_counter_end
    MOV R1, global_a
    IADD R1, R4
    MOV R2, [R1]
    IEQ R2, 7
    JT R2, __keep_exit_target_uses_counter_out
    IADD R4, 1
    JMP __keep_exit_target_uses_counter_start
__keep_exit_target_uses_counter_end:
    RET
__keep_exit_target_uses_counter_out:
    MOV R0, R4
    RET

__function_keep_no_pair:
    MOV R4, 0
__keep_no_pair_start:
    MOV R0, R4
    ILT R0, 64
    JF R0, __keep_no_pair_end
    IADD R5, R4
    IADD R4, 1
    JMP __keep_no_pair_start
__keep_no_pair_end:
    RET

