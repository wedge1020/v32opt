; ===================================================================
; TEST: loop-unroll-partial - K iterations per round behind one guard
; Run with: ./v32opt loop-unroll-partial.asm -floop-unroll-partial -v  (see run.sh)
;
; part_* functions get a guarded main loop (a "__v32opt_unroll_N" label)
; with the original loop kept as the remainder; keep_* must be left alone.
; ===================================================================
%define global_a 100
    CALL __function_main
    HLT

__function_main:
    CALL __function_part_imm
    CALL __function_part_reg
    CALL __function_part_slot
    CALL __function_part_down
    CALL __function_part_step3_le
    CALL __function_keep_bound_register_written
    CALL __function_keep_slot_written
    CALL __function_keep_slot_escapes
    CALL __function_keep_call
    CALL __function_keep_scratch_live
    CALL __function_keep_ine
    CALL __function_keep_wrong_direction
    CALL __function_keep_big_body
    CALL __function_keep_counter_written
    RET

__function_part_imm:
    PUSH BP
    MOV BP, SP
    ISUB SP, 2
    MOV R4, 0
__part_imm_start:
    MOV R0, R4
    ILT R0, 100
    JF R0, __part_imm_end
    IADD R5, R4
    IADD R4, 1
    JMP __part_imm_start
__part_imm_end:
    MOV R0, R5
    MOV SP, BP
    POP BP
    RET

__function_part_reg:
    PUSH BP
    MOV BP, SP
    ISUB SP, 2
    MOV R4, 0
__part_reg_start:
    MOV R0, R4
    ILT R0, R6
    JF R0, __part_reg_end
    MOV R1, global_a
    IADD R1, R4
    MOV R2, [R1]
    IADD R5, R2
    IADD R4, 1
    JMP __part_reg_start
__part_reg_end:
    MOV R0, R5
    MOV SP, BP
    POP BP
    RET

__function_part_slot:
    PUSH BP
    MOV BP, SP
    ISUB SP, 2
    MOV R4, 0
__part_slot_start:
    MOV R0, R4
    MOV R1, [BP+2]
    ILT R0, R1
    JF R0, __part_slot_end
    MOV R1, global_a
    IADD R1, R4
    MOV R2, [R1]
    IADD R5, R2
    IADD R4, 1
    JMP __part_slot_start
__part_slot_end:
    MOV R0, R5
    MOV SP, BP
    POP BP
    RET

__function_part_down:
    PUSH BP
    MOV BP, SP
    ISUB SP, 2
    MOV R4, 90
__part_down_start:
    MOV R0, R4
    MOV R1, [BP+2]
    IGT R0, R1
    JF R0, __part_down_end
    IADD R5, R4
    ISUB R4, 1
    JMP __part_down_start
__part_down_end:
    MOV R0, R5
    MOV SP, BP
    POP BP
    RET

__function_part_step3_le:
    PUSH BP
    MOV BP, SP
    ISUB SP, 2
    MOV R4, 0
__part_step3_le_start:
    MOV R0, R4
    MOV R1, [BP+2]
    ILE R0, R1
    JF R0, __part_step3_le_end
    IADD R5, R4
    IADD R4, 3
    JMP __part_step3_le_start
__part_step3_le_end:
    MOV R0, R5
    MOV SP, BP
    POP BP
    RET

__function_keep_bound_register_written:
    PUSH BP
    MOV BP, SP
    ISUB SP, 2
    MOV R4, 0
__keep_bound_register_written_start:
    MOV R0, R4
    ILT R0, R6
    JF R0, __keep_bound_register_written_end
    IADD R6, 0
    IADD R5, R4
    IADD R4, 1
    JMP __keep_bound_register_written_start
__keep_bound_register_written_end:
    MOV R0, R5
    MOV SP, BP
    POP BP
    RET

__function_keep_slot_written:
    PUSH BP
    MOV BP, SP
    ISUB SP, 2
    MOV R4, 0
__keep_slot_written_start:
    MOV R0, R4
    MOV R1, [BP+2]
    ILT R0, R1
    JF R0, __keep_slot_written_end
    MOV [BP+2], R5
    IADD R5, R4
    IADD R4, 1
    JMP __keep_slot_written_start
__keep_slot_written_end:
    MOV R0, R5
    MOV SP, BP
    POP BP
    RET

__function_keep_slot_escapes:
    PUSH BP
    MOV BP, SP
    ISUB SP, 2
    MOV R9, BP
    MOV R4, 0
__keep_slot_escapes_start:
    MOV R0, R4
    MOV R1, [BP+2]
    ILT R0, R1
    JF R0, __keep_slot_escapes_end
    IADD R5, R4
    IADD R4, 1
    JMP __keep_slot_escapes_start
__keep_slot_escapes_end:
    MOV R0, R5
    MOV SP, BP
    POP BP
    RET

__function_keep_call:
    PUSH BP
    MOV BP, SP
    ISUB SP, 2
    MOV R4, 0
__keep_call_start:
    MOV R0, R4
    ILT R0, 100
    JF R0, __keep_call_end
    CALL __function_main
    IADD R4, 1
    JMP __keep_call_start
__keep_call_end:
    MOV R0, R5
    MOV SP, BP
    POP BP
    RET

__function_keep_scratch_live:
    PUSH BP
    MOV BP, SP
    ISUB SP, 2
    MOV R4, 0
__keep_scratch_live_start:
    MOV R0, R4
    MOV R1, [BP+2]
    ILT R0, R1
    JF R0, __keep_scratch_live_end
    IADD R5, R1
    IADD R4, 1
    JMP __keep_scratch_live_start
__keep_scratch_live_end:
    MOV R0, R5
    MOV SP, BP
    POP BP
    RET

__function_keep_ine:
    PUSH BP
    MOV BP, SP
    ISUB SP, 2
    MOV R4, 0
__keep_ine_start:
    MOV R0, R4
    INE R0, 100
    JF R0, __keep_ine_end
    IADD R5, R4
    IADD R4, 1
    JMP __keep_ine_start
__keep_ine_end:
    MOV R0, R5
    MOV SP, BP
    POP BP
    RET

__function_keep_wrong_direction:
    PUSH BP
    MOV BP, SP
    ISUB SP, 2
    MOV R4, 0
__keep_wrong_direction_start:
    MOV R0, R4
    ILT R0, 100
    JF R0, __keep_wrong_direction_end
    IADD R5, R4
    ISUB R4, 1
    JMP __keep_wrong_direction_start
__keep_wrong_direction_end:
    MOV R0, R5
    MOV SP, BP
    POP BP
    RET

__function_keep_big_body:
    PUSH BP
    MOV BP, SP
    ISUB SP, 2
    MOV R4, 0
__keep_big_body_start:
    MOV R0, R4
    ILT R0, 100
    JF R0, __keep_big_body_end
    IADD R5, 1
    IADD R5, 2
    IADD R5, 3
    IADD R5, 4
    IADD R5, 5
    IADD R5, 6
    IADD R5, 7
    IADD R5, 8
    IADD R5, 9
    IADD R5, 10
    IADD R5, 11
    IADD R5, 12
    IADD R5, 13
    IADD R5, 14
    IADD R5, 15
    IADD R5, 16
    IADD R5, 17
    IADD R5, 18
    IADD R5, 19
    IADD R5, 20
    IADD R5, 21
    IADD R5, 22
    IADD R5, 23
    IADD R5, 24
    IADD R5, 25
    IADD R5, 26
    IADD R5, 27
    IADD R5, 28
    IADD R5, 29
    IADD R4, 1
    JMP __keep_big_body_start
__keep_big_body_end:
    MOV R0, R5
    MOV SP, BP
    POP BP
    RET

__function_keep_counter_written:
    PUSH BP
    MOV BP, SP
    ISUB SP, 2
    MOV R4, 0
__keep_counter_written_start:
    MOV R0, R4
    ILT R0, 100
    JF R0, __keep_counter_written_end
    IADD R4, 1
    IADD R4, 1
    JMP __keep_counter_written_start
__keep_counter_written_end:
    MOV R0, R5
    MOV SP, BP
    POP BP
    RET

