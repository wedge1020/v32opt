_test_dse_function:
    ; --- TEST 1: Standard Dead Store ---
    MOV R1, 100         ; <--- SHOULD BE ELIMINATED (Dead Store)
    MOV R1, 200         ; Overwrites R1 without reading 100
    IADD R1, 5          ; Uses 200 (Result: 205)
    
    ; --- TEST 2: Memory Pointer Safety Check ---
    MOV R2, _BaseAddr   ; <--- MUST NOT BE ELIMINATED!
    MOV [R2], R1        ; Reads R2 as memory pointer, stores 205
    
    ; --- TEST 3: Terminal Dead Store ---
    MOV R3, 999         ; <--- SHOULD BE ELIMINATED (Dead before RET)
    MOV R0, 0           ; Set return value
    RET                 ; R3 is not live-out across RET

_BaseAddr:
	HLT
