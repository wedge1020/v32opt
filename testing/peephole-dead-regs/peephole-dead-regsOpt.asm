; ===================================================================
; TEST: peephole-dead-regs - writes of registers nobody reads
; Run with: ./v32opt peephole-dead-regs.asm -fpeephole-dead-regs -v
;
; An instruction whose only effect is to write a register is removed when
; control-flow-aware liveness proves the register is never read again on
; any path (R0 is live at every RET: it is the return value).
; ===================================================================
%define global_x 1

    CALL __function_main
    HLT

; ===================================================================
; ✅ SECTION 1: overwritten / never read
; ===================================================================
__function_overwritten:
    MOV R1, 6
    MOV R0, R1
    RET

__function_never_read:
    MOV R0, 1
    RET

__function_rmw_dead:
    MOV R0, 0
    RET

__function_dead_load:
    MOV R0, 2
    RET

__function_dead_both_ways:
    JF R0, __db_else
    RET
__db_else:
    RET

__function_compare_dead:
    MOV R0, 1
    RET

; ===================================================================
; ❌ SECTION 2: must be kept
; ===================================================================
__function_used_after:
    MOV R1, 5           ; KEEP read below
    MOV R0, R1
    RET

__function_return_value:
    MOV R0, 5           ; KEEP R0 is the return value
    RET

__function_live_on_one_path:
    MOV R1, 5           ; KEEP read on the fall-through side
    JF R0, __lp_skip
    MOV R0, R1
    RET
__lp_skip:
    MOV R0, 0
    RET

__function_loop_carried:
    MOV R1, 0           ; KEEP
__lc_top:
    IADD R1, 1          ; KEEP read by the next iteration and the exit
    ILT R2, 10          ; KEEP R2 feeds the branch
    JT R2, __lc_top
    MOV R0, R1
    RET

__function_callee_reads:
    MOV R5, 3           ; KEEP the callee below reads R5
    CALL __function_uses_r5
    RET

__function_uses_r5:
    MOV R0, R5
    RET

__function_callee_saves:
    CALL __function_saves_r5
    RET

__function_saves_r5:
    PUSH R5
    POP R5
    MOV R0, 0
    RET

__function_pushed:
    MOV R1, 5           ; KEEP pushed below
    PUSH R1
    POP R0
    RET

__function_port_read:
    IN R1, GPU_RemainingPixels  ; KEEP hardware state, never removed
    MOV R0, 0
    RET

__function_division:
    MOV R1, 8
    IDIV R1, R2         ; KEEP may raise a division error
    MOV R0, 0
    RET

__function_pop:
    POP R1              ; KEEP moves the stack pointer
    MOV R0, 0
    RET

__function_address_used:
    MOV R1, global_x    ; KEEP
    MOV R0, [R1]
    RET

__function_two_results:
    MOV R0, 1
    MOV R1, 2           ; KEEP a caller reads R1 after the call: an output
    RET

__function_reads_second_result:
    CALL __function_two_results
    MOV R0, R1
    RET

__function_main:
    CALL __function_overwritten
    HLT                 ; (nothing reads a register after the call)
    CALL __function_never_read
    HLT                 ; (nothing reads a register after the call)
    CALL __function_rmw_dead
    HLT                 ; (nothing reads a register after the call)
    CALL __function_dead_load
    HLT                 ; (nothing reads a register after the call)
    CALL __function_dead_both_ways
    HLT                 ; (nothing reads a register after the call)
    CALL __function_compare_dead
    HLT                 ; (nothing reads a register after the call)
    CALL __function_used_after
    HLT                 ; (nothing reads a register after the call)
    CALL __function_return_value
    HLT                 ; (nothing reads a register after the call)
    CALL __function_live_on_one_path
    HLT                 ; (nothing reads a register after the call)
    CALL __function_loop_carried
    HLT                 ; (nothing reads a register after the call)
    CALL __function_callee_reads
    HLT                 ; (nothing reads a register after the call)
    CALL __function_uses_r5
    HLT                 ; (nothing reads a register after the call)
    CALL __function_callee_saves
    HLT                 ; (nothing reads a register after the call)
    CALL __function_saves_r5
    HLT                 ; (nothing reads a register after the call)
    CALL __function_pushed
    HLT                 ; (nothing reads a register after the call)
    CALL __function_port_read
    HLT                 ; (nothing reads a register after the call)
    CALL __function_division
    HLT                 ; (nothing reads a register after the call)
    CALL __function_pop
    HLT                 ; (nothing reads a register after the call)
    CALL __function_address_used
    HLT                 ; (nothing reads a register after the call)
    CALL __function_two_results
    HLT                 ; (nothing reads a register after the call)
    CALL __function_reads_second_result

    RET

