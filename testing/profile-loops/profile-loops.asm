; ===================================================================
; TEST: --profile - hot loops get more unrolling, cold ones none
; Run with: ./run.sh   (it writes a profile that says: __hot_* ran 100000
; times per line, __cold_* once, everything else once)
; ===================================================================
%define global_a 100
    CALL __function_main
    HLT

__function_main:
    CALL __function_hot
    CALL __function_cold
    CALL __function_warm
    RET

__function_hot:
    MOV R4, 0
    MOV R5, 0
__hot_start:
    MOV R0, R4
    ILT R0, 100
    JF R0, __hot_end
    IADD R5, R4
    IADD R4, 1
    JMP __hot_start
__hot_end:
    MOV R0, R5
    RET

__function_cold:
    MOV R4, 0
    MOV R5, 0
__cold_start:
    MOV R0, R4
    ILT R0, 100
    JF R0, __cold_end
    IADD R5, R4
    IADD R4, 1
    JMP __cold_start
__cold_end:
    MOV R0, R5
    RET

__function_warm:
    MOV R4, 0
    MOV R5, 0
__warm_start:
    MOV R0, R4
    ILT R0, 100
    JF R0, __warm_end
    IADD R5, R4
    IADD R4, 1
    JMP __warm_start
__warm_end:
    MOV R0, R5
    RET
