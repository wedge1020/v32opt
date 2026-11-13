;; directed.asm: v32lua -w -g --no-build-scripts -o directed.asm directed.lua (v32lua with the upstream loop-variable patch); this comment line added by hand
;; --- Global Variable RAM Map ---
%define  V32_CART_PAGE            0x20000000
%define  NAN_VALUE                0x7F800000
%define  BOXED_CATEGORY           0x80000000 ; RAM(1), ROM(0)
%define  BOXED_TYPE               0x00400000 ; TABLE/FUNCTION (0), STRING (1)
%define  BOXED_DATA               0xFFC00000 ; common bitmask to indicate boxed data
%define  BOXED_FUNCTION           0x7F800000 ; bitmask for boxed lua function (ROM)
%define  BOXED_ROMSTRING          0x7FC00000 ; bitmank for boxed lua string literal (ROM)
%define  BOXED_TABLE              0xFF800000 ; bitmask for boxed lua table (RAM)
%define  BOXED_RAMSTRING          0xFFC00000 ; starting at offset 4
%define  BOXED_NIL                0xFFC00000
%define  BOXED_FALSE              0xFFC00001
%define  BOXED_BOOLEAN            0xFFC00001
%define  BOXED_TRUE               0xFFC00002
%define  BOXED_TOMBSTONE          0xFFC00003 ; future feature
%define  BOXED_PAYLOAD            0x003FFFFF
%define  TABLE_ARRAYSIZE          0x0000FFFF
%define  BOXED_CLOSURE_FLAG       0x00200000
%define  CLOSURE_ADDR_MASK        0x001FFFFF
%define  HEAP_POINTER             0x00000000
%define  FTOA_SCRATCH_PTR_A       0x00000001
%define  FTOA_SCRATCH_PTR_B       0x00000002
%define  VIRCON32_BTN_PREV_STATE  0x00000003
%define  VIRCON32_SFX_CURSOR      0x0000002F
%define  V32IO_GAMEPAD            0x00000030
%define  VIRCON32_MUSIC_CHANNEL_MASK 0x00000031
%define  VIRCON32_SFX_CHANNEL_MASK   0x00000032
%define  RET_COUNT                0x00000033
%define  MV_BUF                   0x00000034
%define  MV_MAX                   32
%define  META_CALL_DEPTH          0x00000054
%define  META_CALL_STACK          0x00000055
%define  META_CALL_STACK_SIZE     32
%define  VIRCON32_MEMCARD_BASE       0x30000000
%define  VIRCON32_MEMCARD_DATA_BASE  0x30000018
%define  VIRCON32_MEMCARD_CURSOR_ADDR 0x30000017
%define  VIRCON32_MEMCARD_END        0x3003FFFF
%define  var_r_p01                0x00000075
%define  var_r_p02a               0x00000076
%define  var_r_p02b               0x00000077
%define  var_r_p03                0x00000078
%define  var_r_p04a               0x00000079
%define  var_r_p04b               0x0000007A
%define  var_r_p05                0x0000007B
%define  var_r_p06a               0x0000007C
%define  var_r_p06b               0x0000007D
%define  var_r_p07                0x0000007E
%define  var_r_p08                0x0000007F
%define  var_r_p09                0x00000080
%define  var_r_p10                0x00000081
%define  var_r_p11a               0x00000082
%define  var_r_p11b               0x00000083
%define  var_r_p12                0x00000084
%define  func_p01_fill            0x00000085
%define  func_p01                 0x00000086
%define  var_s                    0x00000087
%define  func_p02a                0x00000088
%define  func_p02b                0x00000089
%define  func_p03                 0x0000008A
%define  func_p04                 0x0000008B
%define  func_p05                 0x0000008C
%define  func_p06                 0x0000008D
%define  func_p07_sq              0x0000008E
%define  func_p07                 0x0000008F
%define  func_p08                 0x00000090
%define  func_p09                 0x00000091
%define  var_i                    0x00000092
%define  func_p10_n               0x00000093
%define  func_p10                 0x00000094
%define  func_p11a                0x00000095
%define  func_p11b                0x00000096
%define  var_z                    0x00000097
%define  func_p12                 0x00000098
%define  func_main                0x00000099
%define  func___anon_0            0x0000009A
%define  HEAP_START               0x0000009B
;@var r_p01 loc=@0x00000075 type=lua global
;@var r_p02a loc=@0x00000076 type=lua global
;@var r_p02b loc=@0x00000077 type=lua global
;@var r_p03 loc=@0x00000078 type=lua global
;@var r_p04a loc=@0x00000079 type=lua global
;@var r_p04b loc=@0x0000007A type=lua global
;@var r_p05 loc=@0x0000007B type=lua global
;@var r_p06a loc=@0x0000007C type=lua global
;@var r_p06b loc=@0x0000007D type=lua global
;@var r_p07 loc=@0x0000007E type=lua global
;@var r_p08 loc=@0x0000007F type=lua global
;@var r_p09 loc=@0x00000080 type=lua global
;@var r_p10 loc=@0x00000081 type=lua global
;@var r_p11a loc=@0x00000082 type=lua global
;@var r_p11b loc=@0x00000083 type=lua global
;@var r_p12 loc=@0x00000084 type=lua global

;; Highest used global RAM address: 0x0000009A
;; Dynamic heap will start at runtime address: 0x0000009B


    ;@hints 1 abi=caller-save lang=lua
;@src -
;; --- Compiled Code Entry Vector ---
    CALL  __global_scope_initialization  ; Run top-level setups first
    CALL  __function_main ; Execute main execution cycle
    HLT   ; Safe-guard halt

;; --- Function Definitions ---
    JMP   __p01_fill_skip
__function_p01_fill:
;@src directed.lua:9
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 18 ; Reserve stack for locals + spills
    ; --- Default implicit return value(s): nil until overwritten ---
    MOV   R0, BOXED_NIL
    ;@var t loc=BP+2 type=lua
    ;@scope {
    MOV   R1, 0x41200000 ; 10
    MOV   [BP - 1], R1 ; Save loop limit to stack
    MOV   R1, 1.000000 ; Default step = 1.0
    MOV   [BP - 2], R1 ; Save loop step to stack
    MOV   R1, 0x3F800000 ; 1
    MOV   [BP - 3], R1 ; Save loop control counter to stack
    ;@var i loc=BP-4 type=lua
    MOV   [BP - 4], R1 ; init local 'i'
    ;@loop kind=for counter=-3 limit=-1 step=-2 var=-4 stepval=1 from=1 to=10
__p01_fill_for_start_0:
    MOV   R1, [BP - 3]
    MOV   R3, R1 ; preserve control value -- FGT/FLT below is destructive
    MOV   R2, [BP - 1]
    FGT   R1, R2 ; Check if control > limit (exit condition)
    JT    R1, __p01_fill_for_end_0 ; Jump to end if loop condition fails
    MOV   [BP - 4], R3 ; init local 'i'
    MOV   R2, [BP - 4] ; load 'i'
    PUSH  R2 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R5, 0x40000000 ; 2
    POP   R2 ; reload spilled left operand
    FMUL  R2, R5
    PUSH  R2 ; spill value (protect across possible nested CALLs in table/key exprs)
    MOV   R3, [BP + 2] ; load 't'
    PUSH  R3 ; spill table pointer (protect across possible nested CALLs in key expr)
    MOV   R4, [BP - 4] ; load 'i'
    POP   R3 ; reload spilled table pointer
    POP   R2 ; reload spilled value
    PUSH  R3 ; table pointer
    PUSH  R4 ; key
    PUSH  R2 ; value
    CALL  __builtin_table_set ; store key-value pair in table
    IADD  SP, 3 ; clean up stack arguments
    MOV   R1, [BP - 3]
    MOV   R2, [BP - 2]
    FADD  R1, R2 ; control += step
    MOV   [BP - 3], R1 ; Save incremented control counter
    JMP   __p01_fill_for_start_0 ; Loop back to condition
__p01_fill_for_end_0:
    ;@scope }
    ; --- Re-establish Nil for IMPLICIT (fall-through) return ---
    MOV   R0, BOXED_NIL ; implicit return: fell off the end without 'return'
    MOV   R1, 0
    MOV   [RET_COUNT], R1 ; no values returned
__p01_fill_return:
    MOV   SP, BP
    POP   BP
    RET  

    ;@fn __function_p01_fill escapes=none
__p01_fill_skip:
;@src directed.lua:9
    JMP   __p01_skip
__function_p01:
;@src directed.lua:10
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 20 ; Reserve stack for locals + spills
    ; --- Default implicit return value(s): nil until overwritten ---
    MOV   R0, BOXED_NIL
;@src directed.lua:11
    CALL  __builtin_table_new
    MOV   R1, R0
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    ;@var t loc=BP-1 type=lua
    MOV   [BP - 1], R1 ; init local 't'
    ;; Load and box address of the mangled function
    MOV   R2, __function_p01_fill
    OR    R2, BOXED_FUNCTION ; Box as Function
    MOV   [BP -10], R2 ; REGSPILL: Store to spill slot
    ; --- Pushing explicit arguments Right-to-Left ---
    MOV   R3, [BP - 1] ; load 't'
    PUSH  R3
    MOV   R2, [BP -10] ; REGSPILL: Load from spill slot
    MOV   R0, R2 ; Prepare boxed target for validation
    CALL  __builtin_exec ; Validate and execute
    IADD  SP, 1 ; Clean up call arguments
    MOV   R1, R0 ; Store return value
;@src directed.lua:12
    MOV   R1, 0x00000000 ; 0
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    ;@var s loc=BP-2 type=lua
    MOV   [BP - 2], R1 ; init local 's'
    ;@scope {
    MOV   R1, 0x41200000 ; 10
    MOV   [BP - 3], R1 ; Save loop limit to stack
    MOV   R1, 1.000000 ; Default step = 1.0
    MOV   [BP - 4], R1 ; Save loop step to stack
    MOV   R1, 0x3F800000 ; 1
    MOV   [BP - 5], R1 ; Save loop control counter to stack
    ;@var k loc=BP-6 type=lua
    MOV   [BP - 6], R1 ; init local 'k'
    ;@loop kind=for counter=-5 limit=-3 step=-4 var=-6 stepval=1 from=1 to=10
__p01_for_start_0:
    MOV   R1, [BP - 5]
    MOV   R3, R1 ; preserve control value -- FGT/FLT below is destructive
    MOV   R2, [BP - 3]
    FGT   R1, R2 ; Check if control > limit (exit condition)
    JT    R1, __p01_for_end_0 ; Jump to end if loop condition fails
    MOV   [BP - 6], R3 ; init local 'k'
    MOV   R1, [BP - 2] ; load 's'
    PUSH  R1 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R3, [BP - 1] ; load 't'
    MOV   R4, [BP - 6] ; load 'k'
    MOV   R0, R3 ; inline t[i]
    AND   R0, BOXED_DATA
    IEQ   R0, BOXED_TABLE
    JF    R0, __p01_idx_slow_1
    MOV   R0, R4
    AND   R0, NAN_VALUE
    IEQ   R0, NAN_VALUE
    JT    R0, __p01_idx_slow_1 ; not a number
    MOV   R5, R4
    CFI   R5
    MOV   R0, R5
    CIF   R0
    IEQ   R0, R4
    JF    R0, __p01_idx_slow_1 ; not a whole number (or -0)
    MOV   R0, R5
    ILT   R0, 1
    JT    R0, __p01_idx_slow_1
    MOV   R0, R3
    AND   R0, BOXED_PAYLOAD
    MOV   R0, [R0] ; flags: array capacity in the low 16 bits
    AND   R0, TABLE_ARRAYSIZE
    ILT   R0, R5 ; capacity < i ?
    JT    R0, __p01_idx_slow_1
    MOV   R0, R3
    AND   R0, BOXED_PAYLOAD
    MOV   R0, [R0+2] ; array data
    IADD  R0, R5
    ISUB  R0, 1
    MOV   R0, [R0]
    MOV   R2, R0
    JMP   __p01_idx_done_1
    ;@cold
__p01_idx_slow_1:
    PUSH  R3
    PUSH  R4
    CALL  __builtin_table_get
    IADD  SP, 2
    MOV   R2, R0
__p01_idx_done_1:
    POP   R1 ; reload spilled left operand
    MOV   R0, R2
    AND   R0, NAN_VALUE
    IEQ   R0, NAN_VALUE ; not a number?
    JF    R0, __p01_num_2
    ;@cold {
    MOV   R0, R2
    CALL  __arith_coerce ; a numeric string -> its number
    MOV   R2, R0
    ;@cold }
__p01_num_2:
    FADD  R1, R2
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [BP - 2], R1 ; store 's'
    MOV   R1, [BP - 5]
    MOV   R2, [BP - 4]
    FADD  R1, R2 ; control += step
    MOV   [BP - 5], R1 ; Save incremented control counter
    JMP   __p01_for_start_0 ; Loop back to condition
__p01_for_end_0:
    ;@scope }
;@src directed.lua:13
    MOV   R1, [BP - 2] ; load 's'
    PUSH  R1 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R3, [BP - 1] ; load 't'
    MOV   R4, 0x40E00000 ; 7
    MOV   R0, R3 ; inline t[i]
    AND   R0, BOXED_DATA
    IEQ   R0, BOXED_TABLE
    JF    R0, __p01_idx_slow_3
    MOV   R5, 7
    MOV   R0, R3
    AND   R0, BOXED_PAYLOAD
    MOV   R0, [R0] ; flags: array capacity in the low 16 bits
    AND   R0, TABLE_ARRAYSIZE
    ILT   R0, R5 ; capacity < i ?
    JT    R0, __p01_idx_slow_3
    MOV   R0, R3
    AND   R0, BOXED_PAYLOAD
    MOV   R0, [R0+2] ; array data
    IADD  R0, R5
    ISUB  R0, 1
    MOV   R0, [R0]
    MOV   R2, R0
    JMP   __p01_idx_done_3
    ;@cold
__p01_idx_slow_3:
    PUSH  R3
    PUSH  R4
    CALL  __builtin_table_get
    IADD  SP, 2
    MOV   R2, R0
__p01_idx_done_3:
    POP   R1 ; reload spilled left operand
    MOV   R0, R2
    AND   R0, NAN_VALUE
    IEQ   R0, NAN_VALUE ; not a number?
    JF    R0, __p01_num_4
    ;@cold {
    MOV   R0, R2
    CALL  __arith_coerce ; a numeric string -> its number
    MOV   R2, R0
    ;@cold }
__p01_num_4:
    FADD  R1, R2
    PUSH  R1 ; spill return value 0 (protect across later nested CALLs)
    POP   R0 ; restore return value 0
    MOV   R1, 1
    MOV   [RET_COUNT], R1 ; number of values returned
    JMP   __p01_return
    ; --- Re-establish Nil for IMPLICIT (fall-through) return ---
    MOV   R0, BOXED_NIL ; implicit return: fell off the end without 'return'
    MOV   R1, 0
    MOV   [RET_COUNT], R1 ; no values returned
__p01_return:
    MOV   SP, BP
    POP   BP
    RET  

    ;@fn __function_p01 escapes=none
__p01_skip:
;@src directed.lua:13
    JMP   __p02a_skip
__function_p02a:
;@src directed.lua:17
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 23 ; Reserve stack for locals + spills
    ; --- Default implicit return value(s): nil until overwritten ---
    MOV   R0, BOXED_NIL
    MOV   R1, 0x00000000 ; 0
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    ;@var s loc=BP-1 type=lua
    MOV   [BP - 1], R1 ; init local 's'
    ;@scope {
    MOV   R1, 0x40400000 ; 3
    MOV   [BP - 2], R1 ; Save loop limit to stack
    MOV   R1, 1.000000 ; Default step = 1.0
    MOV   [BP - 3], R1 ; Save loop step to stack
    MOV   R1, 0x3F800000 ; 1
    MOV   [BP - 4], R1 ; Save loop control counter to stack
    ;@var i loc=BP-5 type=lua
    MOV   [BP - 5], R1 ; init local 'i'
    ;@loop kind=for counter=-4 limit=-2 step=-3 stepval=1 from=1 to=3
__p02a_for_start_0:
    MOV   R1, [BP - 4]
    MOV   R3, R1 ; preserve control value -- FGT/FLT below is destructive
    MOV   R2, [BP - 2]
    FGT   R1, R2 ; Check if control > limit (exit condition)
    JT    R1, __p02a_for_end_0 ; Jump to end if loop condition fails
    MOV   [BP - 5], R3 ; init local 'i'
    ;@scope {
    MOV   R1, 0x40800000 ; 4
    MOV   [BP - 6], R1 ; Save loop limit to stack
    MOV   R1, 1.000000 ; Default step = 1.0
    MOV   [BP - 7], R1 ; Save loop step to stack
    MOV   R1, 0x3F800000 ; 1
    MOV   [BP - 8], R1 ; Save loop control counter to stack
    ;@var i loc=BP-9 type=lua
    MOV   [BP - 9], R1 ; init local 'i'
    ;@loop kind=for counter=-8 limit=-6 step=-7 var=-9 stepval=1 from=1 to=4
__p02a_for_start_1:
    MOV   R1, [BP - 8]
    MOV   R3, R1 ; preserve control value -- FGT/FLT below is destructive
    MOV   R2, [BP - 6]
    FGT   R1, R2 ; Check if control > limit (exit condition)
    JT    R1, __p02a_for_end_1 ; Jump to end if loop condition fails
    MOV   [BP - 9], R3 ; init local 'i'
    MOV   R1, [BP - 1] ; load 's'
    PUSH  R1 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R2, [BP - 9] ; load 'i'
    POP   R1 ; reload spilled left operand
    FADD  R1, R2
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [BP - 1], R1 ; store 's'
    MOV   R1, [BP - 8]
    MOV   R2, [BP - 7]
    FADD  R1, R2 ; control += step
    MOV   [BP - 8], R1 ; Save incremented control counter
    JMP   __p02a_for_start_1 ; Loop back to condition
__p02a_for_end_1:
    ;@scope }
    MOV   R1, [BP - 1] ; load 's'
    PUSH  R1 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R2, [BP - 5] ; load 'i'
    PUSH  R2 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R3, 0x42C80000 ; 100
    POP   R2 ; reload spilled left operand
    FMUL  R2, R3
    POP   R1 ; reload spilled left operand
    FADD  R1, R2
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [BP - 1], R1 ; store 's'
    MOV   R1, [BP - 4]
    MOV   R2, [BP - 3]
    FADD  R1, R2 ; control += step
    MOV   [BP - 4], R1 ; Save incremented control counter
    JMP   __p02a_for_start_0 ; Loop back to condition
__p02a_for_end_0:
    ;@scope }
    MOV   R1, [BP - 1] ; load 's'
    PUSH  R1 ; spill return value 0 (protect across later nested CALLs)
    POP   R0 ; restore return value 0
    MOV   R1, 1
    MOV   [RET_COUNT], R1 ; number of values returned
    JMP   __p02a_return
    ; --- Re-establish Nil for IMPLICIT (fall-through) return ---
    MOV   R0, BOXED_NIL ; implicit return: fell off the end without 'return'
    MOV   R1, 0
    MOV   [RET_COUNT], R1 ; no values returned
__p02a_return:
    MOV   SP, BP
    POP   BP
    RET  

    ;@fn __function_p02a escapes=none
__p02a_skip:
;@src directed.lua:17
    JMP   __p02b_skip
__function_p02b:
;@src directed.lua:18
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 22 ; Reserve stack for locals + spills
    ; --- Default implicit return value(s): nil until overwritten ---
    MOV   R0, BOXED_NIL
    ;@var t loc=BP+2 type=lua
    ;@scope {
    MOV   R1, 0x40400000 ; 3
    MOV   [BP - 1], R1 ; Save loop limit to stack
    MOV   R1, 1.000000 ; Default step = 1.0
    MOV   [BP - 2], R1 ; Save loop step to stack
    MOV   R1, 0x3F800000 ; 1
    MOV   [BP - 3], R1 ; Save loop control counter to stack
    ;@var i loc=BP-4 type=lua
    MOV   [BP - 4], R1 ; init local 'i'
    ;@loop kind=for counter=-3 limit=-1 step=-2 stepval=1 from=1 to=3
__p02b_for_start_0:
    MOV   R1, [BP - 3]
    MOV   R3, R1 ; preserve control value -- FGT/FLT below is destructive
    MOV   R2, [BP - 1]
    FGT   R1, R2 ; Check if control > limit (exit condition)
    JT    R1, __p02b_for_end_0 ; Jump to end if loop condition fails
    MOV   [BP - 4], R3 ; init local 'i'
    MOV   R2, [BP - 4] ; load 'i'
    PUSH  R2 ; spill value (protect across possible nested CALLs in table/key exprs)
    MOV   R3, [BP + 2] ; load 't'
    PUSH  R3 ; spill table pointer (protect across possible nested CALLs in key expr)
    MOV   R4, [BP - 4] ; load 'i'
    POP   R3 ; reload spilled table pointer
    POP   R2 ; reload spilled value
    PUSH  R3 ; table pointer
    PUSH  R4 ; key
    PUSH  R2 ; value
    CALL  __builtin_table_set ; store key-value pair in table
    IADD  SP, 3 ; clean up stack arguments
    ;@scope {
    MOV   R1, 0x40C00000 ; 6
    MOV   [BP - 5], R1 ; Save loop limit to stack
    MOV   R1, 1.000000 ; Default step = 1.0
    MOV   [BP - 6], R1 ; Save loop step to stack
    MOV   R1, 0x40A00000 ; 5
    MOV   [BP - 7], R1 ; Save loop control counter to stack
    ;@var i loc=BP-8 type=lua
    MOV   [BP - 8], R1 ; init local 'i'
    ;@loop kind=for counter=-7 limit=-5 step=-6 var=-8 stepval=1 from=5 to=6
__p02b_for_start_1:
    MOV   R1, [BP - 7]
    MOV   R3, R1 ; preserve control value -- FGT/FLT below is destructive
    MOV   R2, [BP - 5]
    FGT   R1, R2 ; Check if control > limit (exit condition)
    JT    R1, __p02b_for_end_1 ; Jump to end if loop condition fails
    MOV   [BP - 8], R3 ; init local 'i'
    MOV   R2, [BP - 8] ; load 'i'
    PUSH  R2 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R5, 0x40400000 ; 3
    POP   R2 ; reload spilled left operand
    FMUL  R2, R5
    PUSH  R2 ; spill value (protect across possible nested CALLs in table/key exprs)
    MOV   R3, [BP + 2] ; load 't'
    PUSH  R3 ; spill table pointer (protect across possible nested CALLs in key expr)
    MOV   R4, [BP - 8] ; load 'i'
    POP   R3 ; reload spilled table pointer
    POP   R2 ; reload spilled value
    PUSH  R3 ; table pointer
    PUSH  R4 ; key
    PUSH  R2 ; value
    CALL  __builtin_table_set ; store key-value pair in table
    IADD  SP, 3 ; clean up stack arguments
    MOV   R1, [BP - 7]
    MOV   R2, [BP - 6]
    FADD  R1, R2 ; control += step
    MOV   [BP - 7], R1 ; Save incremented control counter
    JMP   __p02b_for_start_1 ; Loop back to condition
__p02b_for_end_1:
    ;@scope }
    MOV   R1, [BP - 3]
    MOV   R2, [BP - 2]
    FADD  R1, R2 ; control += step
    MOV   [BP - 3], R1 ; Save incremented control counter
    JMP   __p02b_for_start_0 ; Loop back to condition
__p02b_for_end_0:
    ;@scope }
    MOV   R2, [BP + 2] ; load 't'
    MOV   R3, 0x40000000 ; 2
    MOV   R0, R2 ; inline t[i]
    AND   R0, BOXED_DATA
    IEQ   R0, BOXED_TABLE
    JF    R0, __p02b_idx_slow_2
    MOV   R4, 2
    MOV   R0, R2
    AND   R0, BOXED_PAYLOAD
    MOV   R0, [R0] ; flags: array capacity in the low 16 bits
    AND   R0, TABLE_ARRAYSIZE
    ILT   R0, R4 ; capacity < i ?
    JT    R0, __p02b_idx_slow_2
    MOV   R0, R2
    AND   R0, BOXED_PAYLOAD
    MOV   R0, [R0+2] ; array data
    IADD  R0, R4
    ISUB  R0, 1
    MOV   R0, [R0]
    MOV   R1, R0
    JMP   __p02b_idx_done_2
    ;@cold
__p02b_idx_slow_2:
    PUSH  R2
    PUSH  R3
    CALL  __builtin_table_get
    IADD  SP, 2
    MOV   R1, R0
__p02b_idx_done_2:
    PUSH  R1 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R3, [BP + 2] ; load 't'
    MOV   R4, 0x40C00000 ; 6
    MOV   R0, R3 ; inline t[i]
    AND   R0, BOXED_DATA
    IEQ   R0, BOXED_TABLE
    JF    R0, __p02b_idx_slow_3
    MOV   R5, 6
    MOV   R0, R3
    AND   R0, BOXED_PAYLOAD
    MOV   R0, [R0] ; flags: array capacity in the low 16 bits
    AND   R0, TABLE_ARRAYSIZE
    ILT   R0, R5 ; capacity < i ?
    JT    R0, __p02b_idx_slow_3
    MOV   R0, R3
    AND   R0, BOXED_PAYLOAD
    MOV   R0, [R0+2] ; array data
    IADD  R0, R5
    ISUB  R0, 1
    MOV   R0, [R0]
    MOV   R2, R0
    JMP   __p02b_idx_done_3
    ;@cold
__p02b_idx_slow_3:
    PUSH  R3
    PUSH  R4
    CALL  __builtin_table_get
    IADD  SP, 2
    MOV   R2, R0
__p02b_idx_done_3:
    POP   R1 ; reload spilled left operand
    MOV   R0, R1
    AND   R0, NAN_VALUE
    IEQ   R0, NAN_VALUE ; not a number?
    JF    R0, __p02b_num_4
    ;@cold {
    MOV   R0, R1
    CALL  __arith_coerce ; a numeric string -> its number
    MOV   R1, R0
    ;@cold }
__p02b_num_4:
    MOV   R0, R2
    AND   R0, NAN_VALUE
    IEQ   R0, NAN_VALUE ; not a number?
    JF    R0, __p02b_num_5
    ;@cold {
    MOV   R0, R2
    CALL  __arith_coerce ; a numeric string -> its number
    MOV   R2, R0
    ;@cold }
__p02b_num_5:
    FADD  R1, R2
    PUSH  R1 ; spill return value 0 (protect across later nested CALLs)
    POP   R0 ; restore return value 0
    MOV   R1, 1
    MOV   [RET_COUNT], R1 ; number of values returned
    JMP   __p02b_return
    ; --- Re-establish Nil for IMPLICIT (fall-through) return ---
    MOV   R0, BOXED_NIL ; implicit return: fell off the end without 'return'
    MOV   R1, 0
    MOV   [RET_COUNT], R1 ; no values returned
__p02b_return:
    MOV   SP, BP
    POP   BP
    RET  

    ;@fn __function_p02b escapes=none
__p02b_skip:
;@src directed.lua:18
    JMP   __p03_skip
__function_p03:
;@src directed.lua:21
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 23 ; Reserve stack for locals + spills
    ; --- Default implicit return value(s): nil until overwritten ---
    MOV   R0, BOXED_NIL
    ;@var t loc=BP+2 type=lua
    MOV   R1, 0x00000000 ; 0
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    ;@var s loc=BP-1 type=lua
    MOV   [BP - 1], R1 ; init local 's'
    ;@scope {
    MOV   R1, 0x40800000 ; 4
    MOV   [BP - 2], R1 ; Save loop limit to stack
    MOV   R1, 1.000000 ; Default step = 1.0
    MOV   [BP - 3], R1 ; Save loop step to stack
    MOV   R1, 0x3F800000 ; 1
    MOV   [BP - 4], R1 ; Save loop control counter to stack
    ;@var i loc=BP-5 type=lua
    MOV   [BP - 5], R1 ; init local 'i'
    ;@loop kind=for counter=-4 limit=-2 step=-3 var=-5 stepval=1 from=1 to=4
__p03_for_start_0:
    MOV   R1, [BP - 4]
    MOV   R3, R1 ; preserve control value -- FGT/FLT below is destructive
    MOV   R2, [BP - 2]
    FGT   R1, R2 ; Check if control > limit (exit condition)
    JT    R1, __p03_for_end_0 ; Jump to end if loop condition fails
    MOV   [BP - 5], R3 ; init local 'i'
    ;@scope {
    MOV   R1, 0x40A00000 ; 5
    MOV   [BP - 6], R1 ; Save loop limit to stack
    MOV   R1, 1.000000 ; Default step = 1.0
    MOV   [BP - 7], R1 ; Save loop step to stack
    MOV   R1, 0x3F800000 ; 1
    MOV   [BP - 8], R1 ; Save loop control counter to stack
    ;@var j loc=BP-9 type=lua
    MOV   [BP - 9], R1 ; init local 'j'
    ;@loop kind=for counter=-8 limit=-6 step=-7 var=-9 stepval=1 from=1 to=5
__p03_for_start_1:
    MOV   R1, [BP - 8]
    MOV   R3, R1 ; preserve control value -- FGT/FLT below is destructive
    MOV   R2, [BP - 6]
    FGT   R1, R2 ; Check if control > limit (exit condition)
    JT    R1, __p03_for_end_1 ; Jump to end if loop condition fails
    MOV   [BP - 9], R3 ; init local 'j'
    MOV   R2, [BP - 5] ; load 'i'
    PUSH  R2 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R5, [BP - 9] ; load 'j'
    POP   R2 ; reload spilled left operand
    FMUL  R2, R5
    PUSH  R2 ; spill value (protect across possible nested CALLs in table/key exprs)
    MOV   R3, [BP + 2] ; load 't'
    PUSH  R3 ; spill table pointer (protect across possible nested CALLs in key expr)
    MOV   R4, [BP - 9] ; load 'j'
    POP   R3 ; reload spilled table pointer
    POP   R2 ; reload spilled value
    PUSH  R3 ; table pointer
    PUSH  R4 ; key
    PUSH  R2 ; value
    CALL  __builtin_table_set ; store key-value pair in table
    IADD  SP, 3 ; clean up stack arguments
    MOV   R1, [BP - 1] ; load 's'
    PUSH  R1 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R3, [BP + 2] ; load 't'
    MOV   R4, [BP - 9] ; load 'j'
    MOV   R0, R3 ; inline t[i]
    AND   R0, BOXED_DATA
    IEQ   R0, BOXED_TABLE
    JF    R0, __p03_idx_slow_2
    MOV   R0, R4
    AND   R0, NAN_VALUE
    IEQ   R0, NAN_VALUE
    JT    R0, __p03_idx_slow_2 ; not a number
    MOV   R5, R4
    CFI   R5
    MOV   R0, R5
    CIF   R0
    IEQ   R0, R4
    JF    R0, __p03_idx_slow_2 ; not a whole number (or -0)
    MOV   R0, R5
    ILT   R0, 1
    JT    R0, __p03_idx_slow_2
    MOV   R0, R3
    AND   R0, BOXED_PAYLOAD
    MOV   R0, [R0] ; flags: array capacity in the low 16 bits
    AND   R0, TABLE_ARRAYSIZE
    ILT   R0, R5 ; capacity < i ?
    JT    R0, __p03_idx_slow_2
    MOV   R0, R3
    AND   R0, BOXED_PAYLOAD
    MOV   R0, [R0+2] ; array data
    IADD  R0, R5
    ISUB  R0, 1
    MOV   R0, [R0]
    MOV   R2, R0
    JMP   __p03_idx_done_2
    ;@cold
__p03_idx_slow_2:
    PUSH  R3
    PUSH  R4
    CALL  __builtin_table_get
    IADD  SP, 2
    MOV   R2, R0
__p03_idx_done_2:
    POP   R1 ; reload spilled left operand
    MOV   R0, R2
    AND   R0, NAN_VALUE
    IEQ   R0, NAN_VALUE ; not a number?
    JF    R0, __p03_num_3
    ;@cold {
    MOV   R0, R2
    CALL  __arith_coerce ; a numeric string -> its number
    MOV   R2, R0
    ;@cold }
__p03_num_3:
    FADD  R1, R2
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [BP - 1], R1 ; store 's'
    MOV   R1, [BP - 8]
    MOV   R2, [BP - 7]
    FADD  R1, R2 ; control += step
    MOV   [BP - 8], R1 ; Save incremented control counter
    JMP   __p03_for_start_1 ; Loop back to condition
__p03_for_end_1:
    ;@scope }
    MOV   R1, [BP - 4]
    MOV   R2, [BP - 3]
    FADD  R1, R2 ; control += step
    MOV   [BP - 4], R1 ; Save incremented control counter
    JMP   __p03_for_start_0 ; Loop back to condition
__p03_for_end_0:
    ;@scope }
    MOV   R1, [BP - 1] ; load 's'
    PUSH  R1 ; spill return value 0 (protect across later nested CALLs)
    POP   R0 ; restore return value 0
    MOV   R1, 1
    MOV   [RET_COUNT], R1 ; number of values returned
    JMP   __p03_return
    ; --- Re-establish Nil for IMPLICIT (fall-through) return ---
    MOV   R0, BOXED_NIL ; implicit return: fell off the end without 'return'
    MOV   R1, 0
    MOV   [RET_COUNT], R1 ; no values returned
__p03_return:
    MOV   SP, BP
    POP   BP
    RET  

    ;@fn __function_p03 escapes=none
__p03_skip:
;@src directed.lua:21
    JMP   __p04_skip
__function_p04:
;@src directed.lua:24
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 19 ; Reserve stack for locals + spills
    ; --- Default implicit return value(s): nil until overwritten ---
    MOV   R0, BOXED_NIL
    ;@var t loc=BP+2 type=lua
    ;@var n loc=BP+3 type=lua
    MOV   R1, 0x00000000 ; 0
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    ;@var s loc=BP-1 type=lua
    MOV   [BP - 1], R1 ; init local 's'
    ;@scope {
    MOV   R1, 0x41A00000 ; 20
    MOV   [BP - 2], R1 ; Save loop limit to stack
    MOV   R1, 1.000000 ; Default step = 1.0
    MOV   [BP - 3], R1 ; Save loop step to stack
    MOV   R1, 0x3F800000 ; 1
    MOV   [BP - 4], R1 ; Save loop control counter to stack
    ;@var i loc=BP-5 type=lua
    MOV   [BP - 5], R1 ; init local 'i'
    ;@loop kind=for counter=-4 limit=-2 step=-3 var=-5 stepval=1 from=1 to=20
__p04_for_start_0:
    MOV   R1, [BP - 4]
    MOV   R3, R1 ; preserve control value -- FGT/FLT below is destructive
    MOV   R2, [BP - 2]
    FGT   R1, R2 ; Check if control > limit (exit condition)
    JT    R1, __p04_for_end_0 ; Jump to end if loop condition fails
    MOV   [BP - 5], R3 ; init local 'i'
    MOV   R2, [BP - 5] ; load 'i'
    PUSH  R2 ; spill value (protect across possible nested CALLs in table/key exprs)
    MOV   R3, [BP + 2] ; load 't'
    PUSH  R3 ; spill table pointer (protect across possible nested CALLs in key expr)
    MOV   R4, [BP - 5] ; load 'i'
    POP   R3 ; reload spilled table pointer
    POP   R2 ; reload spilled value
    PUSH  R3 ; table pointer
    PUSH  R4 ; key
    PUSH  R2 ; value
    CALL  __builtin_table_set ; store key-value pair in table
    IADD  SP, 3 ; clean up stack arguments
    MOV   R1, [BP - 5] ; load 'i'
    PUSH  R1 ; spill left operand (right operand may CALL)
    MOV   R2, [BP + 3] ; load 'n'
    POP   R1 ; reload left operand
    MOV   R0, R1
    AND   R0, NAN_VALUE
    IEQ   R0, NAN_VALUE
    JT    R0, __p04_cmp_slow_2 ; left not a number
    MOV   R0, R2
    AND   R0, NAN_VALUE
    IEQ   R0, NAN_VALUE
    JT    R0, __p04_cmp_slow_2 ; right not a number
    MOV   R0, R1
    FGT   R0, R2
    JF    R0, __p04_else_1
    JMP   __p04_cmp_done_3
__p04_cmp_slow_2:
    PUSH  R1
    PUSH  R2
    CALL  __builtin_relcmp ; -1 / 0 / 1, or 2 if not comparable
    IADD  SP, 2
    IEQ   R0, 1
    JF    R0, __p04_else_1
__p04_cmp_done_3:
    ;@scope {
    JMP   __p04_for_end_0
    ;@scope }
    JMP   __p04_end_if_1
__p04_else_1:
__p04_end_if_1:
    MOV   R1, [BP - 1] ; load 's'
    PUSH  R1 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R2, [BP - 5] ; load 'i'
    POP   R1 ; reload spilled left operand
    FADD  R1, R2
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [BP - 1], R1 ; store 's'
    MOV   R1, [BP - 4]
    MOV   R2, [BP - 3]
    FADD  R1, R2 ; control += step
    MOV   [BP - 4], R1 ; Save incremented control counter
    JMP   __p04_for_start_0 ; Loop back to condition
__p04_for_end_0:
    ;@scope }
    MOV   R1, [BP - 1] ; load 's'
    PUSH  R1 ; spill return value 0 (protect across later nested CALLs)
    POP   R0 ; restore return value 0
    MOV   R1, 1
    MOV   [RET_COUNT], R1 ; number of values returned
    JMP   __p04_return
    ; --- Re-establish Nil for IMPLICIT (fall-through) return ---
    MOV   R0, BOXED_NIL ; implicit return: fell off the end without 'return'
    MOV   R1, 0
    MOV   [RET_COUNT], R1 ; no values returned
__p04_return:
    MOV   SP, BP
    POP   BP
    RET  

    ;@fn __function_p04 escapes=none
__p04_skip:
;@src directed.lua:24
    JMP   __p05_skip
__function_p05:
;@src directed.lua:25
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 19 ; Reserve stack for locals + spills
    ; --- Default implicit return value(s): nil until overwritten ---
    MOV   R0, BOXED_NIL
    ;@var t loc=BP+2 type=lua
    MOV   R1, 0x00000000 ; 0
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    ;@var s loc=BP-1 type=lua
    MOV   [BP - 1], R1 ; init local 's'
    ;@scope {
    MOV   R1, 0x41400000 ; 12
    MOV   [BP - 2], R1 ; Save loop limit to stack
    MOV   R1, 1.000000 ; Default step = 1.0
    MOV   [BP - 3], R1 ; Save loop step to stack
    MOV   R1, 0x3F800000 ; 1
    MOV   [BP - 4], R1 ; Save loop control counter to stack
    ;@var i loc=BP-5 type=lua
    MOV   [BP - 5], R1 ; init local 'i'
    ;@loop kind=for counter=-4 limit=-2 step=-3 var=-5 stepval=1 from=1 to=12
__p05_for_start_0:
    MOV   R1, [BP - 4]
    MOV   R3, R1 ; preserve control value -- FGT/FLT below is destructive
    MOV   R2, [BP - 2]
    FGT   R1, R2 ; Check if control > limit (exit condition)
    JT    R1, __p05_for_end_0 ; Jump to end if loop condition fails
    MOV   [BP - 5], R3 ; init local 'i'
    MOV   R1, [BP - 5] ; load 'i'
    PUSH  R1 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R2, 0x40400000 ; 3
    POP   R1 ; reload spilled left operand
    MOV   R3, R1 ; quot = a
    FDIV  R3, R2 ; quot = a / b
    FLR   R3 ; quot = floor(a / b)
    FMUL  R3, R2 ; quot = floor(a / b) * b
    FSUB  R1, R3 ; dest = a - floor(a / b) * b
    MOV   R0, 0x00000000 ; 0
    FEQ   R1, R0 ; a boxed value is a NaN: never equal
    JF    R1, __p05_else_2
    ;@scope {
    JMP   __p05_label_continue_1 ; goto continue
    ;@scope }
    JMP   __p05_end_if_2
__p05_else_2:
__p05_end_if_2:
    MOV   R2, [BP - 5] ; load 'i'
    PUSH  R2 ; spill value (protect across possible nested CALLs in table/key exprs)
    MOV   R3, [BP + 2] ; load 't'
    PUSH  R3 ; spill table pointer (protect across possible nested CALLs in key expr)
    MOV   R4, [BP - 5] ; load 'i'
    POP   R3 ; reload spilled table pointer
    POP   R2 ; reload spilled value
    PUSH  R3 ; table pointer
    PUSH  R4 ; key
    PUSH  R2 ; value
    CALL  __builtin_table_set ; store key-value pair in table
    IADD  SP, 3 ; clean up stack arguments
    MOV   R1, [BP - 1] ; load 's'
    PUSH  R1 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R2, [BP - 5] ; load 'i'
    POP   R1 ; reload spilled left operand
    FADD  R1, R2
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [BP - 1], R1 ; store 's'
__p05_label_continue_1:
    MOV   R1, [BP - 4]
    MOV   R2, [BP - 3]
    FADD  R1, R2 ; control += step
    MOV   [BP - 4], R1 ; Save incremented control counter
    JMP   __p05_for_start_0 ; Loop back to condition
__p05_for_end_0:
    ;@scope }
    MOV   R1, [BP - 1] ; load 's'
    PUSH  R1 ; spill return value 0 (protect across later nested CALLs)
    POP   R0 ; restore return value 0
    MOV   R1, 1
    MOV   [RET_COUNT], R1 ; number of values returned
    JMP   __p05_return
    ; --- Re-establish Nil for IMPLICIT (fall-through) return ---
    MOV   R0, BOXED_NIL ; implicit return: fell off the end without 'return'
    MOV   R1, 0
    MOV   [RET_COUNT], R1 ; no values returned
__p05_return:
    MOV   SP, BP
    POP   BP
    RET  

    ;@fn __function_p05 escapes=none
__p05_skip:
;@src directed.lua:25
    JMP   __p06_skip
__function_p06:
;@src directed.lua:26
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 18 ; Reserve stack for locals + spills
    ; --- Default implicit return value(s): nil until overwritten ---
    MOV   R0, BOXED_NIL
    ;@var t loc=BP+2 type=lua
    ;@var x loc=BP+3 type=lua
    ;@scope {
    MOV   R1, [BP + 2] ; load 't'
    PUSH  R1
    CALL  __builtin_len
    IADD  SP, 1
    MOV   R1, R0
    MOV   [BP - 1], R1 ; Save loop limit to stack
    MOV   R1, 1.000000 ; Default step = 1.0
    MOV   [BP - 2], R1 ; Save loop step to stack
    MOV   R1, 0x3F800000 ; 1
    MOV   [BP - 3], R1 ; Save loop control counter to stack
    ;@var i loc=BP-4 type=lua
    MOV   [BP - 4], R1 ; init local 'i'
    ;@loop kind=for counter=-3 limit=-1 step=-2 var=-4 stepval=1
__p06_for_start_0:
    MOV   R1, [BP - 3]
    MOV   R3, R1 ; preserve control value -- FGT/FLT below is destructive
    MOV   R2, [BP - 1]
    FGT   R1, R2 ; Check if control > limit (exit condition)
    JT    R1, __p06_for_end_0 ; Jump to end if loop condition fails
    MOV   [BP - 4], R3 ; init local 'i'
    MOV   R2, [BP + 2] ; load 't'
    MOV   R3, [BP - 4] ; load 'i'
    MOV   R0, R2 ; inline t[i]
    AND   R0, BOXED_DATA
    IEQ   R0, BOXED_TABLE
    JF    R0, __p06_idx_slow_2
    MOV   R0, R3
    AND   R0, NAN_VALUE
    IEQ   R0, NAN_VALUE
    JT    R0, __p06_idx_slow_2 ; not a number
    MOV   R4, R3
    CFI   R4
    MOV   R0, R4
    CIF   R0
    IEQ   R0, R3
    JF    R0, __p06_idx_slow_2 ; not a whole number (or -0)
    MOV   R0, R4
    ILT   R0, 1
    JT    R0, __p06_idx_slow_2
    MOV   R0, R2
    AND   R0, BOXED_PAYLOAD
    MOV   R0, [R0] ; flags: array capacity in the low 16 bits
    AND   R0, TABLE_ARRAYSIZE
    ILT   R0, R4 ; capacity < i ?
    JT    R0, __p06_idx_slow_2
    MOV   R0, R2
    AND   R0, BOXED_PAYLOAD
    MOV   R0, [R0+2] ; array data
    IADD  R0, R4
    ISUB  R0, 1
    MOV   R0, [R0]
    MOV   R1, R0
    JMP   __p06_idx_done_2
    ;@cold
__p06_idx_slow_2:
    PUSH  R2
    PUSH  R3
    CALL  __builtin_table_get
    IADD  SP, 2
    MOV   R1, R0
__p06_idx_done_2:
    PUSH  R1 ; spill left operand (right operand may CALL)
    MOV   R2, [BP + 3] ; load 'x'
    POP   R1 ; reload left operand
    MOV   R0, R1
    IEQ   R0, R2 ; identical?
    JT    R0, __p06_eq_skip_3
    MOV   R0, R1
    AND   R0, NAN_VALUE
    IEQ   R0, NAN_VALUE
    JT    R0, __p06_eq_boxed_4 ; left is boxed
    MOV   R0, R1 ; left is a number: numeric compare (-0 == 0; boxed right is a NaN)
    FEQ   R0, R2
    JT    R0, __p06_eq_skip_3
    JMP   __p06_else_1
__p06_eq_boxed_4:
    MOV   R0, R1
    AND   R0, 0x7FC00000
    IEQ   R0, 0x7FC00000
    JF    R0, __p06_else_1 ; left is a table/function: identity only
    MOV   R0, R2
    AND   R0, 0x7FC00000
    IEQ   R0, 0x7FC00000
    JF    R0, __p06_else_1 ; right is not a string
    PUSH  R1
    PUSH  R2
    CALL  __builtin_eq
    IADD  SP, 2
    IEQ   R0, BOXED_TRUE
    JF    R0, __p06_else_1
__p06_eq_skip_3:
    ;@scope {
    MOV   R1, [BP - 4] ; load 'i'
    PUSH  R1 ; spill return value 0 (protect across later nested CALLs)
    POP   R0 ; restore return value 0
    MOV   R1, 1
    MOV   [RET_COUNT], R1 ; number of values returned
    JMP   __p06_return
    ;@scope }
    JMP   __p06_end_if_1
__p06_else_1:
__p06_end_if_1:
    MOV   R1, [BP - 3]
    MOV   R2, [BP - 2]
    FADD  R1, R2 ; control += step
    MOV   [BP - 3], R1 ; Save incremented control counter
    JMP   __p06_for_start_0 ; Loop back to condition
__p06_for_end_0:
    ;@scope }
    MOV   R1, 0xBF800000 ; -1
    PUSH  R1 ; spill return value 0 (protect across later nested CALLs)
    POP   R0 ; restore return value 0
    MOV   R1, 1
    MOV   [RET_COUNT], R1 ; number of values returned
    JMP   __p06_return
    ; --- Re-establish Nil for IMPLICIT (fall-through) return ---
    MOV   R0, BOXED_NIL ; implicit return: fell off the end without 'return'
    MOV   R1, 0
    MOV   [RET_COUNT], R1 ; no values returned
__p06_return:
    MOV   SP, BP
    POP   BP
    RET  

    ;@fn __function_p06 escapes=none
__p06_skip:
;@src directed.lua:26
    JMP   __p07_sq_skip
__function_p07_sq:
;@src directed.lua:29
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 14 ; Reserve stack for locals + spills
    ; --- Default implicit return value(s): nil until overwritten ---
    MOV   R0, BOXED_NIL
    ;@var x loc=BP+2 type=lua
    MOV   R1, [BP + 2] ; load 'x'
    PUSH  R1 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R2, [BP + 2] ; load 'x'
    POP   R1 ; reload spilled left operand
    MOV   R0, R1
    AND   R0, NAN_VALUE
    IEQ   R0, NAN_VALUE ; not a number?
    JF    R0, __p07_sq_num_0
    ;@cold {
    MOV   R0, R1
    CALL  __arith_coerce ; a numeric string -> its number
    MOV   R1, R0
    ;@cold }
__p07_sq_num_0:
    MOV   R0, R2
    AND   R0, NAN_VALUE
    IEQ   R0, NAN_VALUE ; not a number?
    JF    R0, __p07_sq_num_1
    ;@cold {
    MOV   R0, R2
    CALL  __arith_coerce ; a numeric string -> its number
    MOV   R2, R0
    ;@cold }
__p07_sq_num_1:
    FMUL  R1, R2
    PUSH  R1 ; spill return value 0 (protect across later nested CALLs)
    POP   R0 ; restore return value 0
    MOV   R1, 1
    MOV   [RET_COUNT], R1 ; number of values returned
    JMP   __p07_sq_return
    ; --- Re-establish Nil for IMPLICIT (fall-through) return ---
    MOV   R0, BOXED_NIL ; implicit return: fell off the end without 'return'
    MOV   R1, 0
    MOV   [RET_COUNT], R1 ; no values returned
__p07_sq_return:
    MOV   SP, BP
    POP   BP
    RET  

    ;@fn __function_p07_sq escapes=none
__p07_sq_skip:
;@src directed.lua:29
    JMP   __p07_skip
__function_p07:
;@src directed.lua:30
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 20 ; Reserve stack for locals + spills
    ; --- Default implicit return value(s): nil until overwritten ---
    MOV   R0, BOXED_NIL
    ;@var t loc=BP+2 type=lua
    MOV   R1, 0x00000000 ; 0
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    ;@var s loc=BP-1 type=lua
    MOV   [BP - 1], R1 ; init local 's'
    ;@scope {
    MOV   R1, 0x40C00000 ; 6
    MOV   [BP - 2], R1 ; Save loop limit to stack
    MOV   R1, 1.000000 ; Default step = 1.0
    MOV   [BP - 3], R1 ; Save loop step to stack
    MOV   R1, 0x3F800000 ; 1
    MOV   [BP - 4], R1 ; Save loop control counter to stack
    ;@var i loc=BP-5 type=lua
    MOV   [BP - 5], R1 ; init local 'i'
    ;@loop kind=for counter=-4 limit=-2 step=-3 var=-5 stepval=1 from=1 to=6
__p07_for_start_0:
    MOV   R1, [BP - 4]
    MOV   R3, R1 ; preserve control value -- FGT/FLT below is destructive
    MOV   R2, [BP - 2]
    FGT   R1, R2 ; Check if control > limit (exit condition)
    JT    R1, __p07_for_end_0 ; Jump to end if loop condition fails
    MOV   [BP - 5], R3 ; init local 'i'
    ;; Load and box address of the mangled function
    MOV   R2, __function_p07_sq
    OR    R2, BOXED_FUNCTION ; Box as Function
    MOV   [BP -10], R2 ; REGSPILL: Store to spill slot
    ; --- Pushing explicit arguments Right-to-Left ---
    MOV   R3, [BP - 5] ; load 'i'
    PUSH  R3
    MOV   R2, [BP -10] ; REGSPILL: Load from spill slot
    MOV   R0, R2 ; Prepare boxed target for validation
    CALL  __builtin_exec ; Validate and execute
    IADD  SP, 1 ; Clean up call arguments
    MOV   R1, R0 ; Store return value
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    ;@var q loc=BP-6 type=lua
    MOV   [BP - 6], R1 ; init local 'q'
    MOV   R2, [BP - 6] ; load 'q'
    PUSH  R2 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R5, [BP - 5] ; load 'i'
    POP   R2 ; reload spilled left operand
    MOV   R0, R2
    AND   R0, NAN_VALUE
    IEQ   R0, NAN_VALUE ; not a number?
    JF    R0, __p07_num_1
    ;@cold {
    MOV   R0, R2
    CALL  __arith_coerce ; a numeric string -> its number
    MOV   R2, R0
    ;@cold }
__p07_num_1:
    FADD  R2, R5
    PUSH  R2 ; spill value (protect across possible nested CALLs in table/key exprs)
    MOV   R3, [BP + 2] ; load 't'
    PUSH  R3 ; spill table pointer (protect across possible nested CALLs in key expr)
    MOV   R4, [BP - 5] ; load 'i'
    POP   R3 ; reload spilled table pointer
    POP   R2 ; reload spilled value
    PUSH  R3 ; table pointer
    PUSH  R4 ; key
    PUSH  R2 ; value
    CALL  __builtin_table_set ; store key-value pair in table
    IADD  SP, 3 ; clean up stack arguments
    MOV   R1, [BP - 1] ; load 's'
    PUSH  R1 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R2, [BP - 5] ; load 'i'
    POP   R1 ; reload spilled left operand
    FADD  R1, R2
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [BP - 1], R1 ; store 's'
    MOV   R1, [BP - 4]
    MOV   R2, [BP - 3]
    FADD  R1, R2 ; control += step
    MOV   [BP - 4], R1 ; Save incremented control counter
    JMP   __p07_for_start_0 ; Loop back to condition
__p07_for_end_0:
    ;@scope }
    MOV   R1, [BP - 1] ; load 's'
    PUSH  R1 ; spill return value 0 (protect across later nested CALLs)
    POP   R0 ; restore return value 0
    MOV   R1, 1
    MOV   [RET_COUNT], R1 ; number of values returned
    JMP   __p07_return
    ; --- Re-establish Nil for IMPLICIT (fall-through) return ---
    MOV   R0, BOXED_NIL ; implicit return: fell off the end without 'return'
    MOV   R1, 0
    MOV   [RET_COUNT], R1 ; no values returned
__p07_return:
    MOV   SP, BP
    POP   BP
    RET  

    ;@fn __function_p07 escapes=none
__p07_skip:
;@src directed.lua:30
    JMP   __p08_skip
__function_p08:
;@src directed.lua:33
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 19 ; Reserve stack for locals + spills
    ; --- Default implicit return value(s): nil until overwritten ---
    MOV   R0, BOXED_NIL
    CALL  __builtin_table_new
    MOV   R1, R0
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    ;@var fs loc=BP-1 type=lua
    MOV   [BP - 1], R1 ; init local 'fs'
    ;@scope {
    MOV   R1, 0x40400000 ; 3
    MOV   [BP - 2], R1 ; Save loop limit to stack
    MOV   R1, 1.000000 ; Default step = 1.0
    MOV   [BP - 3], R1 ; Save loop step to stack
    MOV   R1, 0x3F800000 ; 1
    MOV   [BP - 4], R1 ; Save loop control counter to stack
    ;@var i loc=BP-5 type=lua boxed
    ; --- 'i' is captured by a nested closure: box it ---
    PUSH  R1 ; preserve value across __malloc
    MOV   R0, 1
    PUSH  R0 ; box size = 1 word
    CALL  __malloc
    IADD  SP, 1 ; clean up malloc argument
    POP   R1 ; restore value saved before __malloc
    MOV   [R0], R1 ; store initial value into the box
    MOV   [BP - 5], R0 ; slot now holds the box pointer
    ;@loop kind=for counter=-4 limit=-2 step=-3 stepval=1 from=1 to=3
__p08_for_start_0:
    MOV   R1, [BP - 4]
    MOV   R3, R1 ; preserve control value -- FGT/FLT below is destructive
    MOV   R2, [BP - 2]
    FGT   R1, R2 ; Check if control > limit (exit condition)
    JT    R1, __p08_for_end_0 ; Jump to end if loop condition fails
    ; --- 'i' is captured by a nested closure: box it ---
    PUSH  R3 ; preserve value across __malloc
    MOV   R0, 1
    PUSH  R0 ; box size = 1 word
    CALL  __malloc
    IADD  SP, 1 ; clean up malloc argument
    POP   R3 ; restore value saved before __malloc
    MOV   [R0], R3 ; store initial value into the box
    MOV   [BP - 5], R0 ; slot now holds the box pointer
    JMP   ____anon_0_skip
__function___anon_0:
;@src directed.lua:33
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 14 ; Reserve stack for locals + spills
    ; --- Default implicit return value(s): nil until overwritten ---
    MOV   R0, BOXED_NIL
    ;@var i loc=BP+2 type=lua boxed upvalue
    ; --- 'i' is captured -- boxed access ---
    MOV   R1, [BP + 2] ; load box pointer
    MOV   R1, [R1] ; dereference box
    PUSH  R1 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R2, 0x41200000 ; 10
    POP   R1 ; reload spilled left operand
    MOV   R0, R1
    AND   R0, NAN_VALUE
    IEQ   R0, NAN_VALUE ; not a number?
    JF    R0, ____anon_0_num_0
    ;@cold {
    MOV   R0, R1
    CALL  __arith_coerce ; a numeric string -> its number
    MOV   R1, R0
    ;@cold }
____anon_0_num_0:
    FMUL  R1, R2
    PUSH  R1 ; spill return value 0 (protect across later nested CALLs)
    POP   R0 ; restore return value 0
    MOV   R1, 1
    MOV   [RET_COUNT], R1 ; number of values returned
    JMP   ____anon_0_return
    ; --- Re-establish Nil for IMPLICIT (fall-through) return ---
    MOV   R0, BOXED_NIL ; implicit return: fell off the end without 'return'
    MOV   R1, 0
    MOV   [RET_COUNT], R1 ; no values returned
____anon_0_return:
    MOV   SP, BP
    POP   BP
    MOV   R7, [SP] ; peek return address (don't consume it yet)
    IADD  SP, 2 ; discard return address slot + 1 upvalue word
    JMP   R7 ; manual return, stack now balanced

    ;@fn __function___anon_0 escapes=none
____anon_0_skip:
    ;; Build closure record for '__anon_0' (1 upvalue)
;@src directed.lua:33
    PUSH  R0 ; preserve caller's R0 across this closure's own malloc scratch
    MOV   R0, 4
    PUSH  R0
    CALL  __malloc
    IADD  SP, 1 ; clean up malloc argument
    MOV   R5, R0 ; closure record base
    IADD  R5, 1
    AND   R5, 0xFFFFFFFE ; even address
    MOV   R0, __function___anon_0
    MOV   [R5], R0 ; word 0: code address
    MOV   R0, 1
    MOV   [R5+1], R0 ; word 1: upvalue count
    POP   R0 ; restore caller's R0
    MOV   R6, [BP - 5] ; box pointer for captured 'i'
    MOV   [R5+2], R6
    MOV   R2, R5 ; box the closure record
    SHL   R2, -1 ; address / 2 (see above)
    OR    R2, BOXED_FUNCTION
    OR    R2, BOXED_CLOSURE_FLAG ; mark payload as a closure record, not raw code
    PUSH  R2 ; spill value (protect across possible nested CALLs in table/key exprs)
    MOV   R3, [BP - 1] ; load 'fs'
    PUSH  R3 ; spill table pointer (protect across possible nested CALLs in key expr)
    ; --- 'i' is captured -- boxed access ---
    MOV   R4, [BP - 5] ; load box pointer
    MOV   R4, [R4] ; dereference box
    POP   R3 ; reload spilled table pointer
    POP   R2 ; reload spilled value
    PUSH  R3 ; table pointer
    PUSH  R4 ; key
    PUSH  R2 ; value
    CALL  __builtin_table_set ; store key-value pair in table
    IADD  SP, 3 ; clean up stack arguments
    MOV   R1, [BP - 4]
    MOV   R2, [BP - 3]
    FADD  R1, R2 ; control += step
    MOV   [BP - 4], R1 ; Save incremented control counter
    JMP   __p08_for_start_0 ; Loop back to condition
__p08_for_end_0:
    ;@scope }
    MOV   R3, [BP - 1] ; load 'fs'
    MOV   R4, 0x40000000 ; 2
    MOV   R0, R3 ; inline t[i]
    AND   R0, BOXED_DATA
    IEQ   R0, BOXED_TABLE
    JF    R0, __p08_idx_slow_1
    MOV   R5, 2
    MOV   R0, R3
    AND   R0, BOXED_PAYLOAD
    MOV   R0, [R0] ; flags: array capacity in the low 16 bits
    AND   R0, TABLE_ARRAYSIZE
    ILT   R0, R5 ; capacity < i ?
    JT    R0, __p08_idx_slow_1
    MOV   R0, R3
    AND   R0, BOXED_PAYLOAD
    MOV   R0, [R0+2] ; array data
    IADD  R0, R5
    ISUB  R0, 1
    MOV   R0, [R0]
    MOV   R2, R0
    JMP   __p08_idx_done_1
    ;@cold
__p08_idx_slow_1:
    PUSH  R3
    PUSH  R4
    CALL  __builtin_table_get
    IADD  SP, 2
    MOV   R2, R0
__p08_idx_done_1:
    MOV   [BP -9], R2 ; REGSPILL: Store to spill slot
    ; --- Padding 2 omitted arguments ---
    MOV   R3, BOXED_NIL ; Lua Nil for missing args
    PUSH  R3 ; Pad omitted argument
    PUSH  R3 ; Pad omitted argument
    MOV   R2, [BP -9] ; REGSPILL: Load from spill slot
    MOV   R0, R2 ; Prepare boxed target for validation
    MOV   R13, 0 ; argument count (variadic ABI)
    CALL  __builtin_exec ; Validate and execute
    IADD  SP, 2 ; Clean up call arguments
    MOV   R1, R0 ; Store return value
    PUSH  R1 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R4, [BP - 1] ; load 'fs'
    MOV   R5, 0x40400000 ; 3
    MOV   R0, R4 ; inline t[i]
    AND   R0, BOXED_DATA
    IEQ   R0, BOXED_TABLE
    JF    R0, __p08_idx_slow_2
    MOV   R6, 3
    MOV   R0, R4
    AND   R0, BOXED_PAYLOAD
    MOV   R0, [R0] ; flags: array capacity in the low 16 bits
    AND   R0, TABLE_ARRAYSIZE
    ILT   R0, R6 ; capacity < i ?
    JT    R0, __p08_idx_slow_2
    MOV   R0, R4
    AND   R0, BOXED_PAYLOAD
    MOV   R0, [R0+2] ; array data
    IADD  R0, R6
    ISUB  R0, 1
    MOV   R0, [R0]
    MOV   R3, R0
    JMP   __p08_idx_done_2
    ;@cold
__p08_idx_slow_2:
    PUSH  R4
    PUSH  R5
    CALL  __builtin_table_get
    IADD  SP, 2
    MOV   R3, R0
__p08_idx_done_2:
    MOV   [BP -10], R3 ; REGSPILL: Store to spill slot
    ; --- Padding 2 omitted arguments ---
    MOV   R4, BOXED_NIL ; Lua Nil for missing args
    PUSH  R4 ; Pad omitted argument
    PUSH  R4 ; Pad omitted argument
    MOV   R3, [BP -10] ; REGSPILL: Load from spill slot
    MOV   R0, R3 ; Prepare boxed target for validation
    MOV   R13, 0 ; argument count (variadic ABI)
    CALL  __builtin_exec ; Validate and execute
    IADD  SP, 2 ; Clean up call arguments
    MOV   R2, R0 ; Store return value
    POP   R1 ; reload spilled left operand
    MOV   R0, R1
    AND   R0, NAN_VALUE
    IEQ   R0, NAN_VALUE ; not a number?
    JF    R0, __p08_num_3
    ;@cold {
    MOV   R0, R1
    CALL  __arith_coerce ; a numeric string -> its number
    MOV   R1, R0
    ;@cold }
__p08_num_3:
    MOV   R0, R2
    AND   R0, NAN_VALUE
    IEQ   R0, NAN_VALUE ; not a number?
    JF    R0, __p08_num_4
    ;@cold {
    MOV   R0, R2
    CALL  __arith_coerce ; a numeric string -> its number
    MOV   R2, R0
    ;@cold }
__p08_num_4:
    FADD  R1, R2
    PUSH  R1 ; spill return value 0 (protect across later nested CALLs)
    POP   R0 ; restore return value 0
    MOV   R1, 1
    MOV   [RET_COUNT], R1 ; number of values returned
    JMP   __p08_return
    ; --- Re-establish Nil for IMPLICIT (fall-through) return ---
    MOV   R0, BOXED_NIL ; implicit return: fell off the end without 'return'
    MOV   R1, 0
    MOV   [RET_COUNT], R1 ; no values returned
__p08_return:
    MOV   SP, BP
    POP   BP
    RET  

    ;@fn __function_p08 escapes=none
__p08_skip:
;@src directed.lua:33
    JMP   __p09_skip
__function_p09:
;@src directed.lua:36
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 19 ; Reserve stack for locals + spills
    ; --- Default implicit return value(s): nil until overwritten ---
    MOV   R0, BOXED_NIL
    ;@var t loc=BP+2 type=lua
    MOV   R1, 0x00000000 ; 0
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    ;@var s loc=BP-1 type=lua
    MOV   [BP - 1], R1 ; init local 's'
    ;@scope {
    MOV   R1, 0x40A00000 ; 5
    MOV   [BP - 2], R1 ; Save loop limit to stack
    MOV   R1, 1.000000 ; Default step = 1.0
    MOV   [BP - 3], R1 ; Save loop step to stack
    MOV   R1, 0x3F800000 ; 1
    MOV   [BP - 4], R1 ; Save loop control counter to stack
    ;@var i loc=BP-5 type=lua
    MOV   [BP - 5], R1 ; init local 'i'
    ;@loop kind=for counter=-4 limit=-2 step=-3 stepval=1 from=1 to=5
__p09_for_start_0:
    MOV   R1, [BP - 4]
    MOV   R3, R1 ; preserve control value -- FGT/FLT below is destructive
    MOV   R2, [BP - 2]
    FGT   R1, R2 ; Check if control > limit (exit condition)
    JT    R1, __p09_for_end_0 ; Jump to end if loop condition fails
    MOV   [BP - 5], R3 ; init local 'i'
    MOV   R1, [BP - 5] ; load 'i'
    PUSH  R1 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R2, 0x40000000 ; 2
    POP   R1 ; reload spilled left operand
    FMUL  R1, R2
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [BP - 5], R1 ; store 'i'
    MOV   R2, [BP - 5] ; load 'i'
    PUSH  R2 ; spill value (protect across possible nested CALLs in table/key exprs)
    MOV   R3, [BP + 2] ; load 't'
    PUSH  R3 ; spill table pointer (protect across possible nested CALLs in key expr)
    MOV   R4, [BP - 5] ; load 'i'
    POP   R3 ; reload spilled table pointer
    POP   R2 ; reload spilled value
    PUSH  R3 ; table pointer
    PUSH  R4 ; key
    PUSH  R2 ; value
    CALL  __builtin_table_set ; store key-value pair in table
    IADD  SP, 3 ; clean up stack arguments
    MOV   R1, [BP - 1] ; load 's'
    PUSH  R1 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R2, [BP - 5] ; load 'i'
    POP   R1 ; reload spilled left operand
    FADD  R1, R2
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [BP - 1], R1 ; store 's'
    MOV   R1, [BP - 4]
    MOV   R2, [BP - 3]
    FADD  R1, R2 ; control += step
    MOV   [BP - 4], R1 ; Save incremented control counter
    JMP   __p09_for_start_0 ; Loop back to condition
__p09_for_end_0:
    ;@scope }
    MOV   R1, [BP - 1] ; load 's'
    PUSH  R1 ; spill return value 0 (protect across later nested CALLs)
    POP   R0 ; restore return value 0
    MOV   R1, 1
    MOV   [RET_COUNT], R1 ; number of values returned
    JMP   __p09_return
    ; --- Re-establish Nil for IMPLICIT (fall-through) return ---
    MOV   R0, BOXED_NIL ; implicit return: fell off the end without 'return'
    MOV   R1, 0
    MOV   [RET_COUNT], R1 ; no values returned
__p09_return:
    MOV   SP, BP
    POP   BP
    RET  

    ;@fn __function_p09 escapes=none
__p09_skip:
;@src directed.lua:36
    JMP   __p10_n_skip
__function_p10_n:
;@src directed.lua:39
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 14 ; Reserve stack for locals + spills
    ; --- Default implicit return value(s): nil until overwritten ---
    MOV   R0, BOXED_NIL
    ;@var a loc=BP+2 type=lua
    MOV   R1, [BP + 2] ; load 'a'
    PUSH  R1 ; spill return value 0 (protect across later nested CALLs)
    POP   R0 ; restore return value 0
    MOV   R1, 1
    MOV   [RET_COUNT], R1 ; number of values returned
    JMP   __p10_n_return
    ; --- Re-establish Nil for IMPLICIT (fall-through) return ---
    MOV   R0, BOXED_NIL ; implicit return: fell off the end without 'return'
    MOV   R1, 0
    MOV   [RET_COUNT], R1 ; no values returned
__p10_n_return:
    MOV   SP, BP
    POP   BP
    RET  

    ;@fn __function_p10_n escapes=none
__p10_n_skip:
;@src directed.lua:39
    JMP   __p10_skip
__function_p10:
;@src directed.lua:40
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 20 ; Reserve stack for locals + spills
    MOV   [BP -20], R13 ; save caller's argument count (variadic ABI)
    ; --- Default implicit return value(s): nil until overwritten ---
    MOV   R0, BOXED_NIL
    MOV   R1, 0x00000000 ; 0
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    ;@var s loc=BP-1 type=lua
    MOV   [BP - 1], R1 ; init local 's'
    ;@scope {
    MOV   R1, 0x40800000 ; 4
    MOV   [BP - 2], R1 ; Save loop limit to stack
    MOV   R1, 1.000000 ; Default step = 1.0
    MOV   [BP - 3], R1 ; Save loop step to stack
    MOV   R1, 0x3F800000 ; 1
    MOV   [BP - 4], R1 ; Save loop control counter to stack
    ;@var i loc=BP-5 type=lua
    MOV   [BP - 5], R1 ; init local 'i'
    ;@loop kind=for counter=-4 limit=-2 step=-3 stepval=1 from=1 to=4
__p10_for_start_0:
    MOV   R1, [BP - 4]
    MOV   R3, R1 ; preserve control value -- FGT/FLT below is destructive
    MOV   R2, [BP - 2]
    FGT   R1, R2 ; Check if control > limit (exit condition)
    JT    R1, __p10_for_end_0 ; Jump to end if loop condition fails
    MOV   [BP - 5], R3 ; init local 'i'
    MOV   R1, [BP - 1] ; load 's'
    PUSH  R1 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R2, [BP - 5] ; load 'i'
    PUSH  R2 ; spill left operand (protect across possible nested CALL in right operand)
    ;; Load and box address of the mangled function
    MOV   R4, __function_p10_n
    OR    R4, BOXED_FUNCTION ; Box as Function
    MOV   [BP -11], R4 ; REGSPILL: Store to spill slot
    MOV   R5, SP ; stack before the arguments (multi-value call)
    MOV   [BP -12], R5 ; REGSPILL: Store to spill slot
    MOV   R2, BP
    IADD  R2, -20
    MOV   R2, [R2] ; arguments passed
    ISUB  R2, 0 ; -> number of varargs
    MOV   R1, BP
    IADD  R1, 2 ; the first vararg
    CALL  __mv_from_mem
    MOV   R6, [RET_COUNT] ; values in the list
    MOV   R7, R6
__p10_mvarg_1:
    JF    R7, __p10_mvarg_end_1
    ISUB  R7, 1
    MOV   R8, R7
    IADD  R8, MV_BUF
    MOV   R8, [R8]
    PUSH  R8 ; list value (last first)
    JMP   __p10_mvarg_1
__p10_mvarg_end_1:
    IADD  R6, 0 ; + the other arguments -> argument count
    MOV   [BP -13], R6 ; REGSPILL: Store to spill slot
    ; --- Pushing explicit arguments Right-to-Left ---
    MOV   R4, [BP -11] ; REGSPILL: Load from spill slot
    MOV   R0, R4 ; Prepare boxed target for validation
    MOV   R6, [BP -13] ; REGSPILL: Load from spill slot
    MOV   R13, R6 ; argument count (variadic ABI)
    CALL  __builtin_exec ; Validate and execute
    MOV   R5, [BP -12] ; REGSPILL: Load from spill slot
    MOV   SP, R5 ; drop the arguments (multi-value call)
    MOV   R3, R0 ; Store return value
    POP   R2 ; reload spilled left operand
    MOV   R0, R3
    AND   R0, NAN_VALUE
    IEQ   R0, NAN_VALUE ; not a number?
    JF    R0, __p10_num_2
    ;@cold {
    MOV   R0, R3
    CALL  __arith_coerce ; a numeric string -> its number
    MOV   R3, R0
    ;@cold }
__p10_num_2:
    FMUL  R2, R3
    POP   R1 ; reload spilled left operand
    FADD  R1, R2
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [BP - 1], R1 ; store 's'
    MOV   R1, [BP - 4]
    MOV   R2, [BP - 3]
    FADD  R1, R2 ; control += step
    MOV   [BP - 4], R1 ; Save incremented control counter
    JMP   __p10_for_start_0 ; Loop back to condition
__p10_for_end_0:
    ;@scope }
    MOV   R1, [BP - 1] ; load 's'
    PUSH  R1 ; spill return value 0 (protect across later nested CALLs)
    POP   R0 ; restore return value 0
    MOV   R1, 1
    MOV   [RET_COUNT], R1 ; number of values returned
    JMP   __p10_return
    ; --- Re-establish Nil for IMPLICIT (fall-through) return ---
    MOV   R0, BOXED_NIL ; implicit return: fell off the end without 'return'
    MOV   R1, 0
    MOV   [RET_COUNT], R1 ; no values returned
__p10_return:
    MOV   SP, BP
    POP   BP
    RET  

__p10_skip:
;@src directed.lua:40
    JMP   __p11a_skip
__function_p11a:
;@src directed.lua:43
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 20 ; Reserve stack for locals + spills
    ; --- Default implicit return value(s): nil until overwritten ---
    MOV   R0, BOXED_NIL
    ;@var t loc=BP+2 type=lua
    ;@scope {
    ;@scope {
    MOV   R1, 0x40800000 ; 4
    MOV   [BP - 1], R1 ; Save loop limit to stack
    MOV   R1, 1.000000 ; Default step = 1.0
    MOV   [BP - 2], R1 ; Save loop step to stack
    MOV   R1, 0x3F800000 ; 1
    MOV   [BP - 3], R1 ; Save loop control counter to stack
    ;@var i loc=BP-4 type=lua
    MOV   [BP - 4], R1 ; init local 'i'
    ;@loop kind=for counter=-3 limit=-1 step=-2 var=-4 stepval=1 from=1 to=4
__p11a_for_start_0:
    MOV   R1, [BP - 3]
    MOV   R3, R1 ; preserve control value -- FGT/FLT below is destructive
    MOV   R2, [BP - 1]
    FGT   R1, R2 ; Check if control > limit (exit condition)
    JT    R1, __p11a_for_end_0 ; Jump to end if loop condition fails
    MOV   [BP - 4], R3 ; init local 'i'
    MOV   R2, [BP - 4] ; load 'i'
    PUSH  R2 ; spill value (protect across possible nested CALLs in table/key exprs)
    MOV   R3, [BP + 2] ; load 't'
    PUSH  R3 ; spill table pointer (protect across possible nested CALLs in key expr)
    MOV   R4, [BP - 4] ; load 'i'
    POP   R3 ; reload spilled table pointer
    POP   R2 ; reload spilled value
    PUSH  R3 ; table pointer
    PUSH  R4 ; key
    PUSH  R2 ; value
    CALL  __builtin_table_set ; store key-value pair in table
    IADD  SP, 3 ; clean up stack arguments
    MOV   R1, [BP - 3]
    MOV   R2, [BP - 2]
    FADD  R1, R2 ; control += step
    MOV   [BP - 3], R1 ; Save incremented control counter
    JMP   __p11a_for_start_0 ; Loop back to condition
__p11a_for_end_0:
    ;@scope }
    ;@scope }
    MOV   R2, [BP + 2] ; load 't'
    MOV   R3, 0x40000000 ; 2
    MOV   R0, R2 ; inline t[i]
    AND   R0, BOXED_DATA
    IEQ   R0, BOXED_TABLE
    JF    R0, __p11a_idx_slow_1
    MOV   R4, 2
    MOV   R0, R2
    AND   R0, BOXED_PAYLOAD
    MOV   R0, [R0] ; flags: array capacity in the low 16 bits
    AND   R0, TABLE_ARRAYSIZE
    ILT   R0, R4 ; capacity < i ?
    JT    R0, __p11a_idx_slow_1
    MOV   R0, R2
    AND   R0, BOXED_PAYLOAD
    MOV   R0, [R0+2] ; array data
    IADD  R0, R4
    ISUB  R0, 1
    MOV   R0, [R0]
    MOV   R1, R0
    JMP   __p11a_idx_done_1
    ;@cold
__p11a_idx_slow_1:
    PUSH  R2
    PUSH  R3
    CALL  __builtin_table_get
    IADD  SP, 2
    MOV   R1, R0
__p11a_idx_done_1:
    PUSH  R1 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R2, 0x42200000 ; 40
    POP   R1 ; reload spilled left operand
    MOV   R0, R1
    AND   R0, NAN_VALUE
    IEQ   R0, NAN_VALUE ; not a number?
    JF    R0, __p11a_num_2
    ;@cold {
    MOV   R0, R1
    CALL  __arith_coerce ; a numeric string -> its number
    MOV   R1, R0
    ;@cold }
__p11a_num_2:
    FADD  R1, R2
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    ;@var x loc=BP-1 type=lua
    MOV   [BP - 1], R1 ; init local 'x'
    MOV   R1, [BP - 1] ; load 'x'
    PUSH  R1 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R2, 0x40000000 ; 2
    POP   R1 ; reload spilled left operand
    FMUL  R1, R2
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    ;@var y loc=BP-2 type=lua
    MOV   [BP - 2], R1 ; init local 'y'
    MOV   R1, [BP - 1] ; load 'x'
    PUSH  R1 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R2, [BP - 2] ; load 'y'
    POP   R1 ; reload spilled left operand
    FADD  R1, R2
    PUSH  R1 ; spill return value 0 (protect across later nested CALLs)
    POP   R0 ; restore return value 0
    MOV   R1, 1
    MOV   [RET_COUNT], R1 ; number of values returned
    JMP   __p11a_return
    ; --- Re-establish Nil for IMPLICIT (fall-through) return ---
    MOV   R0, BOXED_NIL ; implicit return: fell off the end without 'return'
    MOV   R1, 0
    MOV   [RET_COUNT], R1 ; no values returned
__p11a_return:
    MOV   SP, BP
    POP   BP
    RET  

    ;@fn __function_p11a escapes=none
__p11a_skip:
;@src directed.lua:43
    JMP   __p11b_skip
__function_p11b:
;@src directed.lua:44
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 20 ; Reserve stack for locals + spills
    ; --- Default implicit return value(s): nil until overwritten ---
    MOV   R0, BOXED_NIL
    ;@var t loc=BP+2 type=lua
    ;@scope {
    MOV   R1, 0x40400000 ; 3
    MOV   [BP - 1], R1 ; Save loop limit to stack
    MOV   R1, 1.000000 ; Default step = 1.0
    MOV   [BP - 2], R1 ; Save loop step to stack
    MOV   R1, 0x3F800000 ; 1
    MOV   [BP - 3], R1 ; Save loop control counter to stack
    ;@var i loc=BP-4 type=lua
    MOV   [BP - 4], R1 ; init local 'i'
    ;@loop kind=for counter=-3 limit=-1 step=-2 var=-4 stepval=1 from=1 to=3
__p11b_for_start_0:
    MOV   R1, [BP - 3]
    MOV   R3, R1 ; preserve control value -- FGT/FLT below is destructive
    MOV   R2, [BP - 1]
    FGT   R1, R2 ; Check if control > limit (exit condition)
    JT    R1, __p11b_for_end_0 ; Jump to end if loop condition fails
    MOV   [BP - 4], R3 ; init local 'i'
    MOV   R2, [BP - 4] ; load 'i'
    PUSH  R2 ; spill value (protect across possible nested CALLs in table/key exprs)
    MOV   R3, [BP + 2] ; load 't'
    PUSH  R3 ; spill table pointer (protect across possible nested CALLs in key expr)
    MOV   R4, [BP - 4] ; load 'i'
    POP   R3 ; reload spilled table pointer
    POP   R2 ; reload spilled value
    PUSH  R3 ; table pointer
    PUSH  R4 ; key
    PUSH  R2 ; value
    CALL  __builtin_table_set ; store key-value pair in table
    IADD  SP, 3 ; clean up stack arguments
    MOV   R1, [BP - 3]
    MOV   R2, [BP - 2]
    FADD  R1, R2 ; control += step
    MOV   [BP - 3], R1 ; Save incremented control counter
    JMP   __p11b_for_start_0 ; Loop back to condition
__p11b_for_end_0:
    ;@scope }
    ;@var z loc=BP-1 type=lua
    MOV   R1, BOXED_NIL
    MOV   [BP - 1], R1 ; init local 'z'
    MOV   R1, 0x41100000 ; 9
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    ;@var w loc=BP-2 type=lua
    MOV   [BP - 2], R1 ; init local 'w'
    MOV   R2, [BP + 2] ; load 't'
    MOV   R3, 0x3F800000 ; 1
    MOV   R0, R2 ; inline t[i]
    AND   R0, BOXED_DATA
    IEQ   R0, BOXED_TABLE
    JF    R0, __p11b_idx_slow_2
    MOV   R4, 1
    MOV   R0, R2
    AND   R0, BOXED_PAYLOAD
    MOV   R0, [R0] ; flags: array capacity in the low 16 bits
    AND   R0, TABLE_ARRAYSIZE
    ILT   R0, R4 ; capacity < i ?
    JT    R0, __p11b_idx_slow_2
    MOV   R0, R2
    AND   R0, BOXED_PAYLOAD
    MOV   R0, [R0+2] ; array data
    IADD  R0, R4
    ISUB  R0, 1
    MOV   R0, [R0]
    MOV   R1, R0
    JMP   __p11b_idx_done_2
    ;@cold
__p11b_idx_slow_2:
    PUSH  R2
    PUSH  R3
    CALL  __builtin_table_get
    IADD  SP, 2
    MOV   R1, R0
__p11b_idx_done_2:
    MOV   R0, 0x3F800000 ; 1
    FEQ   R1, R0 ; a boxed value is a NaN: never equal
    JF    R1, __p11b_else_1
    ;@scope {
    MOV   R1, [BP - 2] ; load 'w'
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [BP - 1], R1 ; store 'z'
    ;@scope }
    JMP   __p11b_end_if_1
__p11b_else_1:
__p11b_end_if_1:
    MOV   R1, [BP - 1] ; load 'z'
    PUSH  R1 ; spill return value 0 (protect across later nested CALLs)
    POP   R0 ; restore return value 0
    MOV   R1, 1
    MOV   [RET_COUNT], R1 ; number of values returned
    JMP   __p11b_return
    ; --- Re-establish Nil for IMPLICIT (fall-through) return ---
    MOV   R0, BOXED_NIL ; implicit return: fell off the end without 'return'
    MOV   R1, 0
    MOV   [RET_COUNT], R1 ; no values returned
__p11b_return:
    MOV   SP, BP
    POP   BP
    RET  

    ;@fn __function_p11b escapes=none
__p11b_skip:
;@src directed.lua:44
    JMP   __p12_skip
__function_p12:
;@src directed.lua:47
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 23 ; Reserve stack for locals + spills
    ; --- Default implicit return value(s): nil until overwritten ---
    MOV   R0, BOXED_NIL
    ;@var t loc=BP+2 type=lua
    MOV   R1, 0x00000000 ; 0
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    ;@var s loc=BP-1 type=lua
    MOV   [BP - 1], R1 ; init local 's'
    ;@scope {
    MOV   R1, 0x40A00000 ; 5
    MOV   [BP - 2], R1 ; Save loop limit to stack
    MOV   R1, 1.000000 ; Default step = 1.0
    MOV   [BP - 3], R1 ; Save loop step to stack
    MOV   R1, 0x3F800000 ; 1
    MOV   [BP - 4], R1 ; Save loop control counter to stack
    ;@var i loc=BP-5 type=lua
    MOV   [BP - 5], R1 ; init local 'i'
    ;@loop kind=for counter=-4 limit=-2 step=-3 var=-5 stepval=1 from=1 to=5
__p12_for_start_0:
    MOV   R1, [BP - 4]
    MOV   R3, R1 ; preserve control value -- FGT/FLT below is destructive
    MOV   R2, [BP - 2]
    FGT   R1, R2 ; Check if control > limit (exit condition)
    JT    R1, __p12_for_end_0 ; Jump to end if loop condition fails
    MOV   [BP - 5], R3 ; init local 'i'
    ;@scope {
    MOV   R1, [BP - 5] ; load 'i'
    MOV   [BP - 6], R1 ; Save loop limit to stack
    MOV   R1, 1.000000 ; Default step = 1.0
    MOV   [BP - 7], R1 ; Save loop step to stack
    MOV   R1, 0x3F800000 ; 1
    MOV   [BP - 8], R1 ; Save loop control counter to stack
    ;@var j loc=BP-9 type=lua
    MOV   [BP - 9], R1 ; init local 'j'
    ;@loop kind=for counter=-8 limit=-6 step=-7 var=-9 stepval=1
__p12_for_start_1:
    MOV   R1, [BP - 8]
    MOV   R3, R1 ; preserve control value -- FGT/FLT below is destructive
    MOV   R2, [BP - 6]
    FGT   R1, R2 ; Check if control > limit (exit condition)
    JT    R1, __p12_for_end_1 ; Jump to end if loop condition fails
    MOV   [BP - 9], R3 ; init local 'j'
    MOV   R2, [BP - 9] ; load 'j'
    PUSH  R2 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R5, [BP - 5] ; load 'i'
    POP   R2 ; reload spilled left operand
    FADD  R2, R5
    PUSH  R2 ; spill value (protect across possible nested CALLs in table/key exprs)
    MOV   R3, [BP + 2] ; load 't'
    PUSH  R3 ; spill table pointer (protect across possible nested CALLs in key expr)
    MOV   R4, [BP - 9] ; load 'j'
    POP   R3 ; reload spilled table pointer
    POP   R2 ; reload spilled value
    PUSH  R3 ; table pointer
    PUSH  R4 ; key
    PUSH  R2 ; value
    CALL  __builtin_table_set ; store key-value pair in table
    IADD  SP, 3 ; clean up stack arguments
    MOV   R1, [BP - 1] ; load 's'
    PUSH  R1 ; spill left operand (protect across possible nested CALL in right operand)
    MOV   R3, [BP + 2] ; load 't'
    MOV   R4, [BP - 9] ; load 'j'
    MOV   R0, R3 ; inline t[i]
    AND   R0, BOXED_DATA
    IEQ   R0, BOXED_TABLE
    JF    R0, __p12_idx_slow_2
    MOV   R0, R4
    AND   R0, NAN_VALUE
    IEQ   R0, NAN_VALUE
    JT    R0, __p12_idx_slow_2 ; not a number
    MOV   R5, R4
    CFI   R5
    MOV   R0, R5
    CIF   R0
    IEQ   R0, R4
    JF    R0, __p12_idx_slow_2 ; not a whole number (or -0)
    MOV   R0, R5
    ILT   R0, 1
    JT    R0, __p12_idx_slow_2
    MOV   R0, R3
    AND   R0, BOXED_PAYLOAD
    MOV   R0, [R0] ; flags: array capacity in the low 16 bits
    AND   R0, TABLE_ARRAYSIZE
    ILT   R0, R5 ; capacity < i ?
    JT    R0, __p12_idx_slow_2
    MOV   R0, R3
    AND   R0, BOXED_PAYLOAD
    MOV   R0, [R0+2] ; array data
    IADD  R0, R5
    ISUB  R0, 1
    MOV   R0, [R0]
    MOV   R2, R0
    JMP   __p12_idx_done_2
    ;@cold
__p12_idx_slow_2:
    PUSH  R3
    PUSH  R4
    CALL  __builtin_table_get
    IADD  SP, 2
    MOV   R2, R0
__p12_idx_done_2:
    POP   R1 ; reload spilled left operand
    MOV   R0, R2
    AND   R0, NAN_VALUE
    IEQ   R0, NAN_VALUE ; not a number?
    JF    R0, __p12_num_3
    ;@cold {
    MOV   R0, R2
    CALL  __arith_coerce ; a numeric string -> its number
    MOV   R2, R0
    ;@cold }
__p12_num_3:
    FADD  R1, R2
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [BP - 1], R1 ; store 's'
    MOV   R1, [BP - 8]
    MOV   R2, [BP - 7]
    FADD  R1, R2 ; control += step
    MOV   [BP - 8], R1 ; Save incremented control counter
    JMP   __p12_for_start_1 ; Loop back to condition
__p12_for_end_1:
    ;@scope }
    MOV   R1, [BP - 4]
    MOV   R2, [BP - 3]
    FADD  R1, R2 ; control += step
    MOV   [BP - 4], R1 ; Save incremented control counter
    JMP   __p12_for_start_0 ; Loop back to condition
__p12_for_end_0:
    ;@scope }
    MOV   R1, [BP - 1] ; load 's'
    PUSH  R1 ; spill return value 0 (protect across later nested CALLs)
    POP   R0 ; restore return value 0
    MOV   R1, 1
    MOV   [RET_COUNT], R1 ; number of values returned
    JMP   __p12_return
    ; --- Re-establish Nil for IMPLICIT (fall-through) return ---
    MOV   R0, BOXED_NIL ; implicit return: fell off the end without 'return'
    MOV   R1, 0
    MOV   [RET_COUNT], R1 ; no values returned
__p12_return:
    MOV   SP, BP
    POP   BP
    RET  

    ;@fn __function_p12 escapes=none
__p12_skip:
;@src directed.lua:47
    JMP   __main_skip
__function_main:
;@src directed.lua:49
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 15 ; Reserve stack for locals + spills
    ; --- Default implicit return value(s): nil until overwritten ---
    MOV   R0, BOXED_NIL
;@src directed.lua:50
    CALL  __builtin_table_new
    MOV   R1, R0
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    ;@var t loc=BP-1 type=lua
    MOV   [BP - 1], R1 ; init local 't'
    ;; Load and box address of the mangled function
;@src directed.lua:51
    MOV   R2, __function_p01
    OR    R2, BOXED_FUNCTION ; Box as Function
    MOV   [BP -5], R2 ; REGSPILL: Store to spill slot
    MOV   R2, [BP -5] ; REGSPILL: Load from spill slot
    MOV   R0, R2 ; Prepare boxed target for validation
    CALL  __builtin_exec ; Validate and execute
    MOV   R1, R0 ; Store return value
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p01], R1 ; store 'r_p01'
    ;; Load and box address of the mangled function
;@src directed.lua:52
    MOV   R2, __function_p02a
    OR    R2, BOXED_FUNCTION ; Box as Function
    MOV   [BP -5], R2 ; REGSPILL: Store to spill slot
    MOV   R2, [BP -5] ; REGSPILL: Load from spill slot
    MOV   R0, R2 ; Prepare boxed target for validation
    CALL  __builtin_exec ; Validate and execute
    MOV   R1, R0 ; Store return value
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p02a], R1 ; store 'r_p02a'
    ;; Load and box address of the mangled function
    MOV   R2, __function_p02b
    OR    R2, BOXED_FUNCTION ; Box as Function
    MOV   [BP -5], R2 ; REGSPILL: Store to spill slot
    ; --- Pushing explicit arguments Right-to-Left ---
    CALL  __builtin_table_new
    MOV   R3, R0
    PUSH  R3
    MOV   R2, [BP -5] ; REGSPILL: Load from spill slot
    MOV   R0, R2 ; Prepare boxed target for validation
    CALL  __builtin_exec ; Validate and execute
    IADD  SP, 1 ; Clean up call arguments
    MOV   R1, R0 ; Store return value
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p02b], R1 ; store 'r_p02b'
    ;; Load and box address of the mangled function
;@src directed.lua:53
    MOV   R2, __function_p03
    OR    R2, BOXED_FUNCTION ; Box as Function
    MOV   [BP -5], R2 ; REGSPILL: Store to spill slot
    ; --- Pushing explicit arguments Right-to-Left ---
    MOV   R3, [BP - 1] ; load 't'
    PUSH  R3
    MOV   R2, [BP -5] ; REGSPILL: Load from spill slot
    MOV   R0, R2 ; Prepare boxed target for validation
    CALL  __builtin_exec ; Validate and execute
    IADD  SP, 1 ; Clean up call arguments
    MOV   R1, R0 ; Store return value
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p03], R1 ; store 'r_p03'
    ;; Load and box address of the mangled function
;@src directed.lua:54
    MOV   R2, __function_p04
    OR    R2, BOXED_FUNCTION ; Box as Function
    MOV   [BP -5], R2 ; REGSPILL: Store to spill slot
    ; --- Pushing explicit arguments Right-to-Left ---
    MOV   R3, 0x40E00000 ; 7
    PUSH  R3
    CALL  __builtin_table_new
    MOV   R3, R0
    PUSH  R3
    MOV   R2, [BP -5] ; REGSPILL: Load from spill slot
    MOV   R0, R2 ; Prepare boxed target for validation
    CALL  __builtin_exec ; Validate and execute
    IADD  SP, 2 ; Clean up call arguments
    MOV   R1, R0 ; Store return value
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p04a], R1 ; store 'r_p04a'
    ;; Load and box address of the mangled function
    MOV   R2, __function_p04
    OR    R2, BOXED_FUNCTION ; Box as Function
    MOV   [BP -5], R2 ; REGSPILL: Store to spill slot
    ; --- Pushing explicit arguments Right-to-Left ---
    MOV   R3, 0x41F00000 ; 30
    PUSH  R3
    CALL  __builtin_table_new
    MOV   R3, R0
    PUSH  R3
    MOV   R2, [BP -5] ; REGSPILL: Load from spill slot
    MOV   R0, R2 ; Prepare boxed target for validation
    CALL  __builtin_exec ; Validate and execute
    IADD  SP, 2 ; Clean up call arguments
    MOV   R1, R0 ; Store return value
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p04b], R1 ; store 'r_p04b'
    ;; Load and box address of the mangled function
;@src directed.lua:55
    MOV   R2, __function_p05
    OR    R2, BOXED_FUNCTION ; Box as Function
    MOV   [BP -5], R2 ; REGSPILL: Store to spill slot
    ; --- Pushing explicit arguments Right-to-Left ---
    CALL  __builtin_table_new
    MOV   R3, R0
    PUSH  R3
    MOV   R2, [BP -5] ; REGSPILL: Load from spill slot
    MOV   R0, R2 ; Prepare boxed target for validation
    CALL  __builtin_exec ; Validate and execute
    IADD  SP, 1 ; Clean up call arguments
    MOV   R1, R0 ; Store return value
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p05], R1 ; store 'r_p05'
    ;; Load and box address of the mangled function
;@src directed.lua:56
    MOV   R2, __function_p06
    OR    R2, BOXED_FUNCTION ; Box as Function
    MOV   [BP -5], R2 ; REGSPILL: Store to spill slot
    ; --- Pushing explicit arguments Right-to-Left ---
    MOV   R3, 0x41A80000 ; 21
    PUSH  R3
    CALL  __builtin_table_new
    MOV   R3, R0
    PUSH  R3 ; spill destination register (protect across possible nested CALL in value expr)
    MOV   R4, 0x40A00000 ; 5
    PUSH  R4 ; spill value (protect across key evaluation)
;@src directed.lua:65
    MOV   R5, 0x3F800000 ; 1
    POP   R4 ; reload spilled value
    POP   R3 ; reload spilled destination register
    PUSH  R3 ; table pointer
    PUSH  R5 ; key
    PUSH  R4 ; value
    CALL  __builtin_table_set
    IADD  SP, 3 ; clean up stack
    PUSH  R3 ; spill destination register (protect across possible nested CALL in value expr)
;@src directed.lua:56
    MOV   R4, 0x41000000 ; 8
    PUSH  R4 ; spill value (protect across key evaluation)
;@src directed.lua:65
    MOV   R5, 0x40000000 ; 2
    POP   R4 ; reload spilled value
    POP   R3 ; reload spilled destination register
    PUSH  R3 ; table pointer
    PUSH  R5 ; key
    PUSH  R4 ; value
    CALL  __builtin_table_set
    IADD  SP, 3 ; clean up stack
    PUSH  R3 ; spill destination register (protect across possible nested CALL in value expr)
;@src directed.lua:56
    MOV   R4, 0x41500000 ; 13
    PUSH  R4 ; spill value (protect across key evaluation)
;@src directed.lua:65
    MOV   R5, 0x40400000 ; 3
    POP   R4 ; reload spilled value
    POP   R3 ; reload spilled destination register
    PUSH  R3 ; table pointer
    PUSH  R5 ; key
    PUSH  R4 ; value
    CALL  __builtin_table_set
    IADD  SP, 3 ; clean up stack
    PUSH  R3 ; spill destination register (protect across possible nested CALL in value expr)
;@src directed.lua:56
    MOV   R4, 0x41A80000 ; 21
    PUSH  R4 ; spill value (protect across key evaluation)
;@src directed.lua:65
    MOV   R5, 0x40800000 ; 4
    POP   R4 ; reload spilled value
    POP   R3 ; reload spilled destination register
    PUSH  R3 ; table pointer
    PUSH  R5 ; key
    PUSH  R4 ; value
    CALL  __builtin_table_set
    IADD  SP, 3 ; clean up stack
    PUSH  R3 ; spill destination register (protect across possible nested CALL in value expr)
;@src directed.lua:56
    MOV   R4, 0x42080000 ; 34
    PUSH  R4 ; spill value (protect across key evaluation)
;@src directed.lua:65
    MOV   R5, 0x40A00000 ; 5
    POP   R4 ; reload spilled value
    POP   R3 ; reload spilled destination register
    PUSH  R3 ; table pointer
    PUSH  R5 ; key
    PUSH  R4 ; value
    CALL  __builtin_table_set
    IADD  SP, 3 ; clean up stack
    PUSH  R3
    MOV   R2, [BP -5] ; REGSPILL: Load from spill slot
    MOV   R0, R2 ; Prepare boxed target for validation
    CALL  __builtin_exec ; Validate and execute
    IADD  SP, 2 ; Clean up call arguments
    MOV   R1, R0 ; Store return value
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p06a], R1 ; store 'r_p06a'
    ;; Load and box address of the mangled function
;@src directed.lua:56
    MOV   R2, __function_p06
    OR    R2, BOXED_FUNCTION ; Box as Function
    MOV   [BP -5], R2 ; REGSPILL: Store to spill slot
    ; --- Pushing explicit arguments Right-to-Left ---
    MOV   R3, 0x40800000 ; 4
    PUSH  R3
    CALL  __builtin_table_new
    MOV   R3, R0
    PUSH  R3 ; spill destination register (protect across possible nested CALL in value expr)
    MOV   R4, 0x40A00000 ; 5
    PUSH  R4 ; spill value (protect across key evaluation)
;@src directed.lua:65
    MOV   R5, 0x3F800000 ; 1
    POP   R4 ; reload spilled value
    POP   R3 ; reload spilled destination register
    PUSH  R3 ; table pointer
    PUSH  R5 ; key
    PUSH  R4 ; value
    CALL  __builtin_table_set
    IADD  SP, 3 ; clean up stack
    PUSH  R3 ; spill destination register (protect across possible nested CALL in value expr)
;@src directed.lua:56
    MOV   R4, 0x41000000 ; 8
    PUSH  R4 ; spill value (protect across key evaluation)
;@src directed.lua:65
    MOV   R5, 0x40000000 ; 2
    POP   R4 ; reload spilled value
    POP   R3 ; reload spilled destination register
    PUSH  R3 ; table pointer
    PUSH  R5 ; key
    PUSH  R4 ; value
    CALL  __builtin_table_set
    IADD  SP, 3 ; clean up stack
    PUSH  R3 ; spill destination register (protect across possible nested CALL in value expr)
;@src directed.lua:56
    MOV   R4, 0x41500000 ; 13
    PUSH  R4 ; spill value (protect across key evaluation)
;@src directed.lua:65
    MOV   R5, 0x40400000 ; 3
    POP   R4 ; reload spilled value
    POP   R3 ; reload spilled destination register
    PUSH  R3 ; table pointer
    PUSH  R5 ; key
    PUSH  R4 ; value
    CALL  __builtin_table_set
    IADD  SP, 3 ; clean up stack
    PUSH  R3 ; spill destination register (protect across possible nested CALL in value expr)
;@src directed.lua:56
    MOV   R4, 0x41A80000 ; 21
    PUSH  R4 ; spill value (protect across key evaluation)
;@src directed.lua:65
    MOV   R5, 0x40800000 ; 4
    POP   R4 ; reload spilled value
    POP   R3 ; reload spilled destination register
    PUSH  R3 ; table pointer
    PUSH  R5 ; key
    PUSH  R4 ; value
    CALL  __builtin_table_set
    IADD  SP, 3 ; clean up stack
    PUSH  R3 ; spill destination register (protect across possible nested CALL in value expr)
;@src directed.lua:56
    MOV   R4, 0x42080000 ; 34
    PUSH  R4 ; spill value (protect across key evaluation)
;@src directed.lua:65
    MOV   R5, 0x40A00000 ; 5
    POP   R4 ; reload spilled value
    POP   R3 ; reload spilled destination register
    PUSH  R3 ; table pointer
    PUSH  R5 ; key
    PUSH  R4 ; value
    CALL  __builtin_table_set
    IADD  SP, 3 ; clean up stack
    PUSH  R3
    MOV   R2, [BP -5] ; REGSPILL: Load from spill slot
    MOV   R0, R2 ; Prepare boxed target for validation
    CALL  __builtin_exec ; Validate and execute
    IADD  SP, 2 ; Clean up call arguments
    MOV   R1, R0 ; Store return value
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p06b], R1 ; store 'r_p06b'
    ;; Load and box address of the mangled function
;@src directed.lua:57
    MOV   R2, __function_p07
    OR    R2, BOXED_FUNCTION ; Box as Function
    MOV   [BP -5], R2 ; REGSPILL: Store to spill slot
    ; --- Pushing explicit arguments Right-to-Left ---
    CALL  __builtin_table_new
    MOV   R3, R0
    PUSH  R3
    MOV   R2, [BP -5] ; REGSPILL: Load from spill slot
    MOV   R0, R2 ; Prepare boxed target for validation
    CALL  __builtin_exec ; Validate and execute
    IADD  SP, 1 ; Clean up call arguments
    MOV   R1, R0 ; Store return value
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p07], R1 ; store 'r_p07'
    ;; Load and box address of the mangled function
;@src directed.lua:58
    MOV   R2, __function_p08
    OR    R2, BOXED_FUNCTION ; Box as Function
    MOV   [BP -5], R2 ; REGSPILL: Store to spill slot
    MOV   R2, [BP -5] ; REGSPILL: Load from spill slot
    MOV   R0, R2 ; Prepare boxed target for validation
    CALL  __builtin_exec ; Validate and execute
    MOV   R1, R0 ; Store return value
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p08], R1 ; store 'r_p08'
    ;; Load and box address of the mangled function
;@src directed.lua:59
    MOV   R2, __function_p09
    OR    R2, BOXED_FUNCTION ; Box as Function
    MOV   [BP -5], R2 ; REGSPILL: Store to spill slot
    ; --- Pushing explicit arguments Right-to-Left ---
    CALL  __builtin_table_new
    MOV   R3, R0
    PUSH  R3
    MOV   R2, [BP -5] ; REGSPILL: Load from spill slot
    MOV   R0, R2 ; Prepare boxed target for validation
    CALL  __builtin_exec ; Validate and execute
    IADD  SP, 1 ; Clean up call arguments
    MOV   R1, R0 ; Store return value
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p09], R1 ; store 'r_p09'
    ;; Load and box address of the mangled function
;@src directed.lua:60
    MOV   R2, __function_p10
    OR    R2, BOXED_FUNCTION ; Box as Function
    MOV   [BP -5], R2 ; REGSPILL: Store to spill slot
    ; --- Pushing explicit arguments Right-to-Left ---
    MOV   R3, 0x40800000 ; 4
    PUSH  R3
    MOV   R3, 0x40400000 ; 3
    PUSH  R3
    MOV   R2, [BP -5] ; REGSPILL: Load from spill slot
    MOV   R0, R2 ; Prepare boxed target for validation
    MOV   R13, 2 ; argument count (variadic ABI)
    CALL  __builtin_exec ; Validate and execute
    IADD  SP, 2 ; Clean up call arguments
    MOV   R1, R0 ; Store return value
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p10], R1 ; store 'r_p10'
    ;; Load and box address of the mangled function
;@src directed.lua:61
    MOV   R2, __function_p11a
    OR    R2, BOXED_FUNCTION ; Box as Function
    MOV   [BP -5], R2 ; REGSPILL: Store to spill slot
    ; --- Pushing explicit arguments Right-to-Left ---
    CALL  __builtin_table_new
    MOV   R3, R0
    PUSH  R3
    MOV   R2, [BP -5] ; REGSPILL: Load from spill slot
    MOV   R0, R2 ; Prepare boxed target for validation
    CALL  __builtin_exec ; Validate and execute
    IADD  SP, 1 ; Clean up call arguments
    MOV   R1, R0 ; Store return value
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p11a], R1 ; store 'r_p11a'
    ;; Load and box address of the mangled function
    MOV   R2, __function_p11b
    OR    R2, BOXED_FUNCTION ; Box as Function
    MOV   [BP -5], R2 ; REGSPILL: Store to spill slot
    ; --- Pushing explicit arguments Right-to-Left ---
    CALL  __builtin_table_new
    MOV   R3, R0
    PUSH  R3
    MOV   R2, [BP -5] ; REGSPILL: Load from spill slot
    MOV   R0, R2 ; Prepare boxed target for validation
    CALL  __builtin_exec ; Validate and execute
    IADD  SP, 1 ; Clean up call arguments
    MOV   R1, R0 ; Store return value
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p11b], R1 ; store 'r_p11b'
    ;; Load and box address of the mangled function
;@src directed.lua:62
    MOV   R2, __function_p12
    OR    R2, BOXED_FUNCTION ; Box as Function
    MOV   [BP -5], R2 ; REGSPILL: Store to spill slot
    ; --- Pushing explicit arguments Right-to-Left ---
    CALL  __builtin_table_new
    MOV   R3, R0
    PUSH  R3
    MOV   R2, [BP -5] ; REGSPILL: Load from spill slot
    MOV   R0, R2 ; Prepare boxed target for validation
    CALL  __builtin_exec ; Validate and execute
    IADD  SP, 1 ; Clean up call arguments
    MOV   R1, R0 ; Store return value
    PUSH  R1 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R1 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p12], R1 ; store 'r_p12'
;@src directed.lua:63
    WAIT 
    ; --- Re-establish Nil for IMPLICIT (fall-through) return ---
    MOV   R0, BOXED_NIL ; implicit return: fell off the end without 'return'
    MOV   R1, 0
    MOV   [RET_COUNT], R1 ; no values returned
__main_return:
    MOV   SP, BP
    POP   BP
    RET  

    ;@fn __function_main escapes=none
__main_skip:
;@src -

; --- Global Variable & Runtime Initialization ---
__global_scope_initialization:
;@src directed.lua:63
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 14 ; Reserve stack for top-level locals + spills
    MOV   R0, HEAP_START ; heap start (see variable map)
    MOV   [HEAP_POINTER], R0
    CALL  __globals_to_nil
    MOV   R0, 0
    MOV   [FTOA_SCRATCH_PTR_A], R0 ; must start at 0 -- see generate_global_setup() comment
    MOV   [FTOA_SCRATCH_PTR_B], R0 ; must start at 0 -- see generate_global_setup() comment
    MOV   R1, VIRCON32_BTN_PREV_STATE ; zero-init btnp() prev-state     table
    MOV   R2, 44
    MOV   R3, 0
__vircon32_btn_prev_state_zero_loop:
    JF    R2, __vircon32_btn_prev_state_zero_done
    MOV   [R1], R3
    IADD  R1, 1
    ISUB  R2, 1
    JMP   __vircon32_btn_prev_state_zero_loop
__vircon32_btn_prev_state_zero_done:
    OUT   INP_SelectedGamepad, 0 ; gamepad 0 selected, and known to be
    MOV   R1, 0
    MOV   [V32IO_GAMEPAD], R1 ; (the port can't be read back: v32io.s)
    MOV   R1, 1 ; sfx.play() round-robin starts at channel 1
    MOV   [VIRCON32_SFX_CURSOR], R1
    MOV   R1, 0 ; no channels claimed by either namespace yet
    MOV   [VIRCON32_MUSIC_CHANNEL_MASK], R1
    MOV   [VIRCON32_SFX_CHANNEL_MASK], R1
    MOV   [META_CALL_DEPTH], R1 ; no __call trampolines pending
    MOV   R0, BOXED_NIL
    MOV   [var_r_p01], R0 ; nil-init global 'r_p01'
    MOV   [var_r_p02a], R0 ; nil-init global 'r_p02a'
    MOV   [var_r_p02b], R0 ; nil-init global 'r_p02b'
    MOV   [var_r_p03], R0 ; nil-init global 'r_p03'
    MOV   [var_r_p04a], R0 ; nil-init global 'r_p04a'
    MOV   [var_r_p04b], R0 ; nil-init global 'r_p04b'
    MOV   [var_r_p05], R0 ; nil-init global 'r_p05'
    MOV   [var_r_p06a], R0 ; nil-init global 'r_p06a'
    MOV   [var_r_p06b], R0 ; nil-init global 'r_p06b'
    MOV   [var_r_p07], R0 ; nil-init global 'r_p07'
    MOV   [var_r_p08], R0 ; nil-init global 'r_p08'
    MOV   [var_r_p09], R0 ; nil-init global 'r_p09'
    MOV   [var_r_p10], R0 ; nil-init global 'r_p10'
    MOV   [var_r_p11a], R0 ; nil-init global 'r_p11a'
    MOV   [var_r_p11b], R0 ; nil-init global 'r_p11b'
    MOV   [var_r_p12], R0 ; nil-init global 'r_p12'
    MOV   [var_s], R0 ; nil-init global 's'
    MOV   [var_i], R0 ; nil-init global 'i'
    MOV   [var_z], R0 ; nil-init global 'z'
;@src directed.lua:5
    MOV   R2, 0x00000000 ; 0
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p01], R2 ; store 'r_p01'
    MOV   R2, 0x00000000 ; 0
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p02a], R2 ; store 'r_p02a'
    MOV   R2, 0x00000000 ; 0
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p02b], R2 ; store 'r_p02b'
    MOV   R2, 0x00000000 ; 0
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p03], R2 ; store 'r_p03'
    MOV   R2, 0x00000000 ; 0
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p04a], R2 ; store 'r_p04a'
    MOV   R2, 0x00000000 ; 0
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p04b], R2 ; store 'r_p04b'
    MOV   R2, 0x00000000 ; 0
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p05], R2 ; store 'r_p05'
    MOV   R2, 0x00000000 ; 0
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p06a], R2 ; store 'r_p06a'
    MOV   R2, 0x00000000 ; 0
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p06b], R2 ; store 'r_p06b'
;@src directed.lua:6
    MOV   R2, 0x00000000 ; 0
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p07], R2 ; store 'r_p07'
    MOV   R2, 0x00000000 ; 0
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p08], R2 ; store 'r_p08'
    MOV   R2, 0x00000000 ; 0
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p09], R2 ; store 'r_p09'
    MOV   R2, 0x00000000 ; 0
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p10], R2 ; store 'r_p10'
    MOV   R2, 0x00000000 ; 0
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p11a], R2 ; store 'r_p11a'
    MOV   R2, 0x00000000 ; 0
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p11b], R2 ; store 'r_p11b'
    MOV   R2, 0x00000000 ; 0
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [var_r_p12], R2 ; store 'r_p12'
    ;; Load and box address of the mangled function
;@src directed.lua:9
    MOV   R2, __function_p01_fill
    OR    R2, BOXED_FUNCTION ; Box as Function
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [func_p01_fill], R2 ; store 'p01_fill'
    ;; Load and box address of the mangled function
;@src directed.lua:14
    MOV   R2, __function_p01
    OR    R2, BOXED_FUNCTION ; Box as Function
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [func_p01], R2 ; store 'p01'
    ;; Load and box address of the mangled function
;@src directed.lua:17
    MOV   R2, __function_p02a
    OR    R2, BOXED_FUNCTION ; Box as Function
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [func_p02a], R2 ; store 'p02a'
    ;; Load and box address of the mangled function
;@src directed.lua:18
    MOV   R2, __function_p02b
    OR    R2, BOXED_FUNCTION ; Box as Function
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [func_p02b], R2 ; store 'p02b'
    ;; Load and box address of the mangled function
;@src directed.lua:21
    MOV   R2, __function_p03
    OR    R2, BOXED_FUNCTION ; Box as Function
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [func_p03], R2 ; store 'p03'
    ;; Load and box address of the mangled function
;@src directed.lua:24
    MOV   R2, __function_p04
    OR    R2, BOXED_FUNCTION ; Box as Function
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [func_p04], R2 ; store 'p04'
    ;; Load and box address of the mangled function
;@src directed.lua:25
    MOV   R2, __function_p05
    OR    R2, BOXED_FUNCTION ; Box as Function
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [func_p05], R2 ; store 'p05'
    ;; Load and box address of the mangled function
;@src directed.lua:26
    MOV   R2, __function_p06
    OR    R2, BOXED_FUNCTION ; Box as Function
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [func_p06], R2 ; store 'p06'
    ;; Load and box address of the mangled function
;@src directed.lua:29
    MOV   R2, __function_p07_sq
    OR    R2, BOXED_FUNCTION ; Box as Function
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [func_p07_sq], R2 ; store 'p07_sq'
    ;; Load and box address of the mangled function
;@src directed.lua:30
    MOV   R2, __function_p07
    OR    R2, BOXED_FUNCTION ; Box as Function
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [func_p07], R2 ; store 'p07'
    ;; Load and box address of the mangled function
;@src directed.lua:33
    MOV   R2, __function_p08
    OR    R2, BOXED_FUNCTION ; Box as Function
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [func_p08], R2 ; store 'p08'
    ;; Load and box address of the mangled function
;@src directed.lua:36
    MOV   R2, __function_p09
    OR    R2, BOXED_FUNCTION ; Box as Function
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [func_p09], R2 ; store 'p09'
    ;; Load and box address of the mangled function
;@src directed.lua:39
    MOV   R2, __function_p10_n
    OR    R2, BOXED_FUNCTION ; Box as Function
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [func_p10_n], R2 ; store 'p10_n'
    ;; Load and box address of the mangled function
;@src directed.lua:40
    MOV   R2, __function_p10
    OR    R2, BOXED_FUNCTION ; Box as Function
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [func_p10], R2 ; store 'p10'
    ;; Load and box address of the mangled function
;@src directed.lua:43
    MOV   R2, __function_p11a
    OR    R2, BOXED_FUNCTION ; Box as Function
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [func_p11a], R2 ; store 'p11a'
    ;; Load and box address of the mangled function
;@src directed.lua:44
    MOV   R2, __function_p11b
    OR    R2, BOXED_FUNCTION ; Box as Function
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [func_p11b], R2 ; store 'p11b'
    ;; Load and box address of the mangled function
;@src directed.lua:47
    MOV   R2, __function_p12
    OR    R2, BOXED_FUNCTION ; Box as Function
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [func_p12], R2 ; store 'p12'
    ;; Load and box address of the mangled function
;@src directed.lua:64
    MOV   R2, __function_main
    OR    R2, BOXED_FUNCTION ; Box as Function
    PUSH  R2 ; Pass 1: hold evaluated RHS value #0 until all targets are known
    POP   R2 ; Pass 2: retrieve evaluated RHS value for target #0
    MOV   [func_main], R2 ; store 'main'
    MOV   SP, BP
    POP   BP
    RET  
;@src -

;; ===========================================================================
;; v32lua Runtime Library Routines
;; ===========================================================================
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Vircon32 NaN-Boxed Routines for Lua Runtime Environment (runtime.s)
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; ===========================================================================
;; SECTION: DEFINES
;; ===========================================================================
;;  V32_CART_PAGE   0x20000000 (0x10000000 when built with -b)
;;  NAN_VALUE       0x7F800000
;;  BOXED_CATEGORY  0x80000000 // sign bit used for RAM (1) vs ROM (0)
;;  BOXED_TYPE      0x00400000 // TABLE/FUNCTION (0) vs STRING (1)
;;  BOXED_DATA      0xFFC00000 // common bitmask to indicate boxed data
;;  BOXED_FUNCTION  0x7F800000 // bitmask for boxed lua function (ROM)
;;  BOXED_ROMSTRING 0x7FC00000 // bitmank for boxed lua string literal (ROM)
;;  BOXED_TABLE     0xFF800000 // bitmask for boxed lua table (RAM)
;;  BOXED_RAMSTRING 0xFFC00000 // starts at offset 4 (includes nil/false/true)
;;  BOXED_NIL       0xFFC00000
;;  BOXED_FALSE     0xFFC00001
;;  BOXED_BOOLEAN   0xFFC00001 // mathing our way to true/false
;;  BOXED_TRUE      0xFFC00002
;;  BOXED_TOMBSTONE 0xFFC00003 // future feature
;;  BOXED_PAYLOAD   0x003FFFFF
;;  TABLE_ARRAYSIZE 0x0000FFFF

;; ===========================================================================
;; SECTION: MEMORY MANAGEMENT & ERROR HANDLING
;; ===========================================================================

;; ---------------------------------------------------------------------------
;; Memory Allocator: Carves out raw word blocks from HEAP_POINTER
;; Incoming Stack: [BP+2] = Number of words requested
;; Returns: R0 = Raw pointer to allocated memory (or 0 if Out-Of-Memory)
;; ---------------------------------------------------------------------------
    ;@fn __malloc clobbers=R0-R3,R6
__malloc:
;@src directed.lua:64
    PUSH  BP
    MOV   BP, SP
         
    MOV   R1, [BP+2]          ; R1 = Requested size in words
    MOV   R0, [HEAP_POINTER]  ; R0 = Address of new allocation block
         
    ;; Calculate potential new heap top
    MOV   R2, R0
    IADD  R2, R1              ; R2 = Potential new HEAP_POINTER
         
    ;; Stack Collision Check: SP grows down, Heap grows up!
    ;; We maintain a 1024-word safety buffer between Heap and Stack.
    MOV   R3, SP
    ISUB  R3, 1024            ; R3 = Lowest safe memory address for stack
    MOV   R6, R2
    IGE   R6, R3              ; Will the new heap top collide with the stack?
    JT    R6, __malloc_oom    ; If Heap >= SafeBoundary, allocation fails!
         
    ;; Success: Commit new heap top and return base address in R0
    MOV   [HEAP_POINTER], R2
    JMP   __malloc_done
         
__malloc_oom:
    MOV   R0, 0               ; Return 0 to signal Out-Of-Memory to caller
         
__malloc_done:
    MOV   SP, BP
    POP   BP
    RET  

;; ---------------------------------------------------------------------------
;; Out-Of-Memory Handler: Safely halts execution when memory is exhausted
;; ---------------------------------------------------------------------------
__oom_handler:
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 1

    MOV   R0, [HEAP_POINTER]
    CIF   R0                        ; word address -> float (Lua number)
    MOV   [BP-1], R0

    MOV   R0, 0xFF000080
    OUT   GPU_ClearColor, R0
    OUT   GPU_Command, GPUCommand_ClearScreen

    MOV   R0, 20
    PUSH  R0
    MOV   R0, 0
    PUSH  R0
    MOV   R0, __const_str_panic_banner
    OR    R0, BOXED_ROMSTRING
    PUSH  R0
    CALL  __builtin_print

    MOV   R0, 20
    PUSH  R0
    MOV   R0, 20
    PUSH  R0
    MOV   R0, __const_str_err_oom
    OR    R0, BOXED_ROMSTRING
    PUSH  R0
    CALL  __builtin_print

    ;; Heap-words-used diagnostic. Deliberately NOT routed through
    ;; print()/tostring() -- see __panic_print_uint's header comment.
    MOV   R0, 20
    PUSH  R0                       ; X
    MOV   R0, 40
    PUSH  R0                       ; Y
    MOV   R0, [HEAP_POINTER]
    PUSH  R0                       ; value
    CALL  __panic_print_uint

    JMP   __panic_halt

;; ---------------------------------------------------------------------------
;; Built-in: Unary Minus (-x) -> Flips IEEE-754 Sign Bit 
;; Incoming Stack: [BP+2] = Tagged Value (Expected Float) 
;; Returns: R0 = Negated Float 
;; ---------------------------------------------------------------------------
    ;@fn __builtin_unm clobbers=R0-R1
__builtin_unm:
    PUSH  BP
    MOV   BP, SP

    MOV   R1, [BP+2]          ; R1 = Value to negate 
    XOR   R1, 0x80000000      ; Instantly changes positive <-> negative 
    MOV   R0, R1
         
    MOV   SP, BP
    POP   BP
    RET  

;; ---------------------------------------------------------------------------
;; Length Operator Dispatch (#): Returns length as IEEE 754 Float in R0
;; Incoming Stack: [BP+2] = Target Value
;; Handles: strings, tables, and returns 0 for other types
;; ---------------------------------------------------------------------------
__builtin_len:
    PUSH  BP
    MOV   BP, SP

    MOV   R0, [BP+2]            ; Load argument

    ;; --- Type check: Is it a STRING (ROM or RAM)? ---
    MOV   R1, R0
    AND   R1, BOXED_DATA        ; Extract tag bits
    IEQ   R1, BOXED_ROMSTRING
    JT    R1, __builtin_len_string

    MOV   R1, R0
    AND   R1, BOXED_DATA        ; Extract tag bits
    IEQ   R1, BOXED_RAMSTRING
    JF    R1, __builtin_len_check_table

    ;; tag matched RAMSTRING -- confirm payload >= 4 before treating it
    ;; as a real string. nil/true/false/tombstone fall through to the
    ;; table check below (and from there to default, since none of them
    ;; carry the TABLE tag either).
    MOV   R1, R0
    AND   R1, BOXED_PAYLOAD
    IGE   R1, 4
    JT    R1, __builtin_len_string

__builtin_len_check_table:
    ;; FIX: this check was missing entirely. __builtin_len_table below
    ;; was unreachable dead code -- nothing in this function ever tested
    ;; for the TABLE tag, so every table (regardless of contents) fell
    ;; through to __builtin_len_default and silently returned 0. Adding
    ;; the actual dispatch here.
    MOV   R1, R0
    AND   R1, BOXED_DATA
    IEQ   R1, BOXED_TABLE
    JT    R1, __builtin_len_table

__builtin_len_default:
    ;; --- Default: Not a string or table (number, boolean, nil, function) ---
    MOV   R0, 0                 ; Return 0 as integer
    CIF   R0                    ; Convert to float
    JMP   __builtin_len_done

__builtin_len_string:
    PUSH  R0                    ; need to repush the boxed pointer
    CALL  __builtin_string_len  ; String length handler
    JMP   __builtin_len_done

__builtin_len_table:
    MOV   R1, R0
    AND   R1, BOXED_PAYLOAD
    MOV   R1, [R1+4]            ; a metatable? its __len answers first
    JF    R1, __builtin_len_raw
    MOV   R1, __mm_str_len
    OR    R1, BOXED_ROMSTRING
    CALL  __table_unary_meta    ; R1 = 1 when it did (R0 = its result)
    JT    R1, __builtin_len_done
__builtin_len_raw:
    PUSH  R0                    ; need to repush the boxed pointer
    CALL  __builtin_table_len   ; Table length handler
    ; R0 already contains float result from __builtin_table_len

__builtin_len_done:
    MOV   SP, BP
    POP   BP
    RET  

    ;@fn __builtin_type clobbers=R0-R1
__builtin_type:
    PUSH  BP
    MOV   BP, SP

    MOV   R0, [BP+2]          ; Load argument into R0

    ;; --- Check exact values for primitives FIRST ---
    MOV   R1, R0
    IEQ   R1, BOXED_NIL
    JT    R1, __type_nil

    MOV   R1, R0
    IEQ   R1, BOXED_FALSE
    JT    R1, __type_boolean

    MOV   R1, R0
    IEQ   R1, BOXED_TRUE
    JT    R1, __type_boolean

    ;; --- Check for number (unboxed float) ---
    MOV   R1, R0
    AND   R1, 0x7F800000      ; NaN/boxed tag mask
    INE   R1, 0x7F800000      ; NOT equal to NaN tag → it's a number
    JT    R1, __type_number

    ;; --- Check for ROM string ---
    MOV   R1, R0
    AND   R1, BOXED_DATA
    IEQ   R1, BOXED_ROMSTRING
    JT    R1, __type_string

    ;; --- Check for RAM string: tag 0xFFC00000 AND payload >= 4 ---
    MOV   R1, R0
    AND   R1, BOXED_DATA
    IEQ   R1, 0xFFC00000      ; Same tag as NIL, but we already ruled out exact NIL
    JF    R1, __type_check_table
    MOV   R1, R0
    AND   R1, BOXED_PAYLOAD   ; Extract lower 22 bits
    IGE   R1, 4               ; RAM strings start at payload 4
    JT    R1, __type_string

__type_check_table:
    ;; --- Check for table ---
    MOV   R1, R0
    AND   R1, BOXED_DATA
    IEQ   R1, BOXED_TABLE
    JT    R1, __type_table

    ;; --- Check for function ---
    MOV   R1, R0
    AND   R1, BOXED_DATA
    IEQ   R1, BOXED_FUNCTION
    JT    R1, __type_function

    ;; --- Fallthrough: unknown ---
    MOV   R0, __const_str_nil
    OR    R0, BOXED_ROMSTRING
    JMP   __type_done

__type_nil:
    MOV   R0, __const_str_nil
    OR    R0, BOXED_ROMSTRING
    JMP   __type_done

__type_boolean:
    MOV   R0, __const_str_boolean
    OR    R0, BOXED_ROMSTRING
    JMP   __type_done

__type_number:
    MOV   R0, __const_str_number
    OR    R0, BOXED_ROMSTRING
    JMP   __type_done

__type_string:
    MOV   R0, __const_str_string
    OR    R0, BOXED_ROMSTRING
    JMP   __type_done

__type_table:
    MOV   R0, __const_str_table
    OR    R0, BOXED_ROMSTRING
    JMP   __type_done

__type_function:
    MOV   R0, __const_str_function
    OR    R0, BOXED_ROMSTRING

__type_done:
    MOV   SP, BP
    POP   BP
    RET  

;; ---------------------------------------------------------------------------
;; Built-in: Unpack a Vircon32 TIM_CurrentDate value into (year, month, day)
;;
;; Incoming Stack: [BP+2] = RAW packed date value, exactly as read from the
;;                 TIM_CurrentDate hardware port via `IN`. This must be the
;;                 UNCONVERTED 32-bit integer -- do NOT run it through CIF
;;                 before pushing it here, since CIF would reinterpret the
;;                 packed bit pattern as a float and destroy it. Format (per
;;                 Vircon32 System Specification Part 7, section 1.2.1):
;;                     bits 31-16: CurrentYear      (0-65535)
;;                     bits 15-0 : ElapsedYearDays  (0 = Jan 1st, counting up;
;;                                 max 364, or 365 in a leap year)
;;
;; Returns (as genuine Lua numbers, already CIF-converted), in the standard
;; 3-value multi-return registers:
;;   R0 = year   (e.g. 2026.0)
;;   R2 = month  (1.0 = January ... 12.0 = December)
;;   R3 = day    (1-based day within the month)
;;
;; Leap years use the standard Gregorian rule (divisible by 4, except
;; centuries unless divisible by 400) -- the spec confirms ElapsedYearDays'
;; upper bound extends to 365 "if the present year is considered a leap
;; year" but doesn't itself define the rule, so this is the one sane,
;; universal reading of it.
;;
;; Registers: R0-R9 used freely as scratch (R14/BP, R15/SP preserved), same
;; convention as __builtin_ftoa.
;; ---------------------------------------------------------------------------
    ;@fn __builtin_unpack_date clobbers=R0-R6,R9
__builtin_unpack_date:
    PUSH  BP
    MOV   BP, SP

    MOV   R0, [BP+2]          ; R0 = raw packed date
    MOV   R1, R0
    IDIV  R1, 65536           ; R1 = CurrentYear
    MOV   R2, R0
    IMOD  R2, 65536           ; R2 = ElapsedYearDays -- remaining days to place

    ;; --- Determine leap year: (year%4==0 && year%100!=0) || (year%400==0) ---
    MOV   R3, R1
    IMOD  R3, 4
    IEQ   R3, 0
    JF    R3, __unpack_date_not_leap     ; not divisible by 4 -> not leap

    MOV   R3, R1
    IMOD  R3, 100
    IEQ   R3, 0
    JF    R3, __unpack_date_is_leap      ; divisible by 4, not by 100 -> leap

    MOV   R3, R1
    IMOD  R3, 400
    IEQ   R3, 0
    JT    R3, __unpack_date_is_leap      ; divisible by 400 -> leap
    JMP   __unpack_date_not_leap         ; divisible by 100, not 400 -> not leap

__unpack_date_is_leap:
    MOV   R9, 1
    JMP   __unpack_date_have_leap_flag
__unpack_date_not_leap:
    MOV   R9, 0
__unpack_date_have_leap_flag:

    MOV   R4, 1                ; R4 = month, starting at January

    ;; --- January (31) ---
    MOV   R5, 31
    MOV   R6, R2
    ILT   R6, R5
    JT    R6, __unpack_date_found
    ISUB  R2, R5
    IADD  R4, 1

    ;; --- February (28, or 29 in a leap year) ---
    MOV   R5, 28
    JF    R9, __unpack_date_feb_len_set
    MOV   R5, 29
__unpack_date_feb_len_set:
    MOV   R6, R2
    ILT   R6, R5
    JT    R6, __unpack_date_found
    ISUB  R2, R5
    IADD  R4, 1

    ;; --- March (31) ---
    MOV   R5, 31
    MOV   R6, R2
    ILT   R6, R5
    JT    R6, __unpack_date_found
    ISUB  R2, R5
    IADD  R4, 1

    ;; --- April (30) ---
    MOV   R5, 30
    MOV   R6, R2
    ILT   R6, R5
    JT    R6, __unpack_date_found
    ISUB  R2, R5
    IADD  R4, 1

    ;; --- May (31) ---
    MOV   R5, 31
    MOV   R6, R2
    ILT   R6, R5
    JT    R6, __unpack_date_found
    ISUB  R2, R5
    IADD  R4, 1

    ;; --- June (30) ---
    MOV   R5, 30
    MOV   R6, R2
    ILT   R6, R5
    JT    R6, __unpack_date_found
    ISUB  R2, R5
    IADD  R4, 1

    ;; --- July (31) ---
    MOV   R5, 31
    MOV   R6, R2
    ILT   R6, R5
    JT    R6, __unpack_date_found
    ISUB  R2, R5
    IADD  R4, 1

    ;; --- August (31) ---
    MOV   R5, 31
    MOV   R6, R2
    ILT   R6, R5
    JT    R6, __unpack_date_found
    ISUB  R2, R5
    IADD  R4, 1

    ;; --- September (30) ---
    MOV   R5, 30
    MOV   R6, R2
    ILT   R6, R5
    JT    R6, __unpack_date_found
    ISUB  R2, R5
    IADD  R4, 1

    ;; --- October (31) ---
    MOV   R5, 31
    MOV   R6, R2
    ILT   R6, R5
    JT    R6, __unpack_date_found
    ISUB  R2, R5
    IADD  R4, 1

    ;; --- November (30) ---
    MOV   R5, 30
    MOV   R6, R2
    ILT   R6, R5
    JT    R6, __unpack_date_found
    ISUB  R2, R5
    IADD  R4, 1

    ;; --- December (31): whatever's left belongs here -- falls straight ---
    ;; --- into __unpack_date_found without needing its own check.       ---

__unpack_date_found:
    ;; R2 = 0-based day-of-month remaining, R4 = month number
    IADD  R2, 1                ; R2 = 1-based day of month

    ;; --- Convert all 3 results to genuine Lua numbers ---
    CIF   R1                    ; year
    CIF   R4                    ; month
    CIF   R2                    ; day

    ;; --- Place into the standard multi-return registers (R0/R2/R3) ---
    ;; ORDER MATTERS: copy day out of R2 BEFORE R2 gets overwritten with
    ;; the month value below.
    MOV   R0, R1                 ; return value 1: year
    MOV   R3, R2                 ; return value 3: day
    MOV   R2, R4                 ; return value 2: month

    MOV   SP, BP
    POP   BP
    RET  

;; ---------------------------------------------------------------------------
;; Built-in: Write a non-negative integer as zero-padded decimal ASCII.
;;
;; Incoming Stack: [BP+4] = value      (raw int, >= 0)
;;                 [BP+3] = min_width  (raw int -- minimum digit count; the
;;                          value's OWN digit count is used instead whenever
;;                          it's larger, exactly like printf's "%0*d" --
;;                          this never truncates)
;;                 [BP+2] = destination (raw address to write the first
;;                          digit at)
;; Returns: R0 = new write pointer, i.e. destination + digits written --
;;          the natural place for a subsequent write to continue from.
;; Registers: R0-R13 scratch (R14/BP, R15/SP preserved).
;; ---------------------------------------------------------------------------
    ;@fn __builtin_write_padded_int clobbers=R0-R1,R5-R13
__builtin_write_padded_int:
    PUSH  BP
    MOV   BP, SP

    MOV   R7, [BP+4]           ; R7 = value (destroyed by the digit loop below)
    MOV   R8, [BP+3]           ; R8 = min_width
    MOV   R9, [BP+2]           ; R9 = write head
    MOV   R6, R9               ; R6 = start of this number's digit span (for reversal)
    MOV   R1, 0                ; R1 = digits written so far

    ;; --- Write digits LSB-first (reversed after) -- always writes at ---
    ;; --- least 1 digit, so value == 0 correctly produces "0".        ---
__padded_digit_loop:
    MOV   R5, R7
    IMOD  R5, 10
    IADD  R5, 48                ; ASCII digit
    MOV   [R9], R5
    IADD  R9, 1
    IADD  R1, 1
    IDIV  R7, 10
    MOV   R5, R7
    INE   R5, 0
    JT    R5, __padded_digit_loop

    ;; --- Pad with zeros (written here, on the right, since ---
    ;; --- everything gets reversed next) until min_width is met. ---
__padded_pad_loop:
    MOV   R5, R1
    IGE   R5, R8
    JT    R5, __padded_reverse
    MOV   R5, 48                ; ASCII '0'
    MOV   [R9], R5
    IADD  R9, 1
    IADD  R1, 1
    JMP   __padded_pad_loop

    ;; --- Reverse the whole span in place: R6 = start, R9-1 = end ---
__padded_reverse:
    MOV   R10, R6
    MOV   R11, R9
    ISUB  R11, 1
__padded_reverse_loop:
    MOV   R5, R10
    IGE   R5, R11
    JT    R5, __padded_done
    MOV   R12, [R10]
    MOV   R13, [R11]
    MOV   [R10], R13
    MOV   [R11], R12
    IADD  R10, 1
    ISUB  R11, 1
    JMP   __padded_reverse_loop

__padded_done:
    MOV   R0, R9                ; return new write head
    MOV   SP, BP
    POP   BP
    RET  

;; ---------------------------------------------------------------------------
;; Built-in: Format (year, month, day) as a "YYYY-MM-DD" string.
;;
;; Incoming Stack: [BP+4] = year  (raw int)
;;                 [BP+3] = month (raw int, 1-12)
;;                 [BP+2] = day   (raw int, 1-31)
;; Returns: R0 = boxed RAM string ("YYYY-MM-DD", BOXED_RAMSTRING-tagged)
;; ---------------------------------------------------------------------------
__builtin_format_date_string:
    PUSH  BP
    MOV   BP, SP

    ;; --- Allocate the buffer: "YYYY-MM-DD" = 10 chars + null = 11 words ---
    MOV   R0, 11
    PUSH  R0
    CALL  __malloc
    IADD  SP, 1

    MOV   R4, R0
    IEQ   R4, 0
    JT    R4, __oom_handler

    PUSH  R0                     ; spill the STRING BASE pointer -- must
                                 ; survive every CALL below

    ;; --- Write year, zero-padded to 4 digits ---
    MOV   R1, [BP+4]              ; year
    PUSH  R1                      ; value       -> callee [BP+4]
    MOV   R3, 4
    PUSH  R3                      ; min_width=4 -> callee [BP+3]
    PUSH  R0                      ; destination -> callee [BP+2] (start of buffer)
    CALL  __builtin_write_padded_int
    IADD  SP, 3
                                  ; R0 = write head after year

    MOV   R5, 45                  ; ASCII '-'
    MOV   [R0], R5
    IADD  R0, 1

    ;; --- Write month, zero-padded to 2 digits ---
    MOV   R1, [BP+3]               ; month
    PUSH  R1
    MOV   R3, 2
    PUSH  R3
    PUSH  R0
    CALL  __builtin_write_padded_int
    IADD  SP, 3

    MOV   R5, 45                   ; ASCII '-'
    MOV   [R0], R5
    IADD  R0, 1

    ;; --- Write day, zero-padded to 2 digits ---
    MOV   R1, [BP+2]                ; day
    PUSH  R1
    MOV   R3, 2
    PUSH  R3
    PUSH  R0
    CALL  __builtin_write_padded_int
    IADD  SP, 3

    MOV   R5, 0                     ; null terminator
    MOV   [R0], R5

    POP   R0                        ; restore STRING BASE pointer
    OR    R0, BOXED_RAMSTRING       ; box the BASE pointer (not the write head)

    MOV   SP, BP
    POP   BP
    RET  

;; ---------------------------------------------------------------------------
;; Built-in: Unpack a Vircon32 TIM_CurrentTime value into (hour, minute, second)
;;
;; Incoming Stack: [BP+2] = RAW elapsed-seconds-within-day value, exactly as
;;                 read from TIM_CurrentTime via `IN` (0-86399, per spec
;;                 Part 7 section 1.2.2). Do NOT CIF it before pushing.
;;
;; Returns (as genuine Lua numbers, already CIF-converted):
;;   R0 = hour   (0-23)
;;   R2 = minute (0-59)
;;   R3 = second (0-59)
;;
;; Registers: R0-R9 scratch (R14/BP, R15/SP preserved).
;; ---------------------------------------------------------------------------
    ;@fn __builtin_unpack_time clobbers=R0-R4
__builtin_unpack_time:
    PUSH  BP
    MOV   BP, SP

    MOV   R0, [BP+2]           ; R0 = raw elapsed seconds within day
    MOV   R1, R0
    IDIV  R1, 3600             ; R1 = hours
    MOV   R2, R0
    IMOD  R2, 3600             ; R2 = remainder within the hour (0-3599)
    MOV   R3, R2               ; R3 = copy of remainder, for minutes
    IDIV  R3, 60               ; R3 = minutes
    IMOD  R2, 60                ; R2 = seconds (remainder % 60)

    CIF   R1                    ; hour
    CIF   R3                    ; minute
    CIF   R2                    ; second

    ;; --- Place into the standard multi-return registers (R0/R2/R3) ---
    MOV   R0, R1                 ; return value 1: hour
    MOV   R4, R2                 ; R4 = seconds, saved before R2 gets overwritten
    MOV   R2, R3                 ; return value 2: minute
    MOV   R3, R4                 ; return value 3: second

    MOV   SP, BP
    POP   BP
    RET  

;; ---------------------------------------------------------------------------
;; Built-in: Format (hour, minute, second) as an "HH:MM:SS" string.
;;
;; Incoming Stack: [BP+4] = hour   (raw int, 0-23)
;;                 [BP+3] = minute (raw int, 0-59)
;;                 [BP+2] = second (raw int, 0-59)
;; Returns: R0 = boxed RAM string ("HH:MM:SS", BOXED_RAMSTRING-tagged)
;; ---------------------------------------------------------------------------
__builtin_format_time_string:
    PUSH  BP
    MOV   BP, SP

    ;; --- Allocate the buffer: "HH:MM:SS" = 8 chars + null = 9 words ---
    MOV   R0, 9
    PUSH  R0
    CALL  __malloc
    IADD  SP, 1

    MOV   R4, R0
    IEQ   R4, 0
    JT    R4, __oom_handler

    PUSH  R0                     ; spill STRING BASE pointer across every CALL below

    ;; --- Write hour, zero-padded to 2 digits ---
    MOV   R1, [BP+4]              ; hour
    PUSH  R1
    MOV   R3, 2
    PUSH  R3
    PUSH  R0
    CALL  __builtin_write_padded_int
    IADD  SP, 3

    MOV   R5, 58                  ; ASCII ':'
    MOV   [R0], R5
    IADD  R0, 1

    ;; --- Write minute, zero-padded to 2 digits ---
    MOV   R1, [BP+3]               ; minute
    PUSH  R1
    MOV   R3, 2
    PUSH  R3
    PUSH  R0
    CALL  __builtin_write_padded_int
    IADD  SP, 3

    MOV   R5, 58                   ; ASCII ':'
    MOV   [R0], R5
    IADD  R0, 1

    ;; --- Write second, zero-padded to 2 digits ---
    MOV   R1, [BP+2]                ; second
    PUSH  R1
    MOV   R3, 2
    PUSH  R3
    PUSH  R0
    CALL  __builtin_write_padded_int
    IADD  SP, 3

    MOV   R5, 0                     ; null terminator
    MOV   [R0], R5

    POP   R0                        ; restore STRING BASE pointer
    OR    R0, BOXED_RAMSTRING

    MOV   SP, BP
    POP   BP
    RET  

;; ===========================================================================
;; SECTION: FUNCTION TRAMPOLINE
;; ===========================================================================

; ============================================================================
; __builtin_exec: Safely validates and executes a boxed function pointer (R0)
; ============================================================================
__builtin_exec:
    ; 1. Isolate and validate the NaN-box tag bits
    MOV   R1, R0
    AND   R1, BOXED_DATA          ; Isolate upper tag bits (adjust if your tag mask differs)
    IEQ   R1, BOXED_FUNCTION          ; Is this tagged as a boxed function pointer?
    JT    R1, __exec_valid            ; If valid, jump to unboxing and execution

    ; 2. Not a function: a table whose metatable has __call is callable --
    ;    __call(t, ...). Anything else is the runtime error.
    MOV   R1, R0
    AND   R1, BOXED_DATA
    IEQ   R1, BOXED_TABLE
    JF    R1, __runtime_error_not_callable
    PUSH  R0
    MOV   R1, __mm_str_call
    OR    R1, BOXED_ROMSTRING
    CALL  __table_metamethod          ; R0 = __call, or nil
    MOV   R1, R0
    AND   R1, BOXED_DATA
    IEQ   R1, BOXED_FUNCTION
    JF    R1, __exec_call_not_callable
    ;; The table becomes the first argument: it takes the return address's
    ;; slot (right above the caller's arguments); the return address goes
    ;; on a small side stack and the callee returns through
    ;; __exec_call_return, which drops the table and goes back -- so the
    ;; caller's own argument clean-up stays balanced.
    MOV   R2, R0                       ; R2 = __call
    POP   R0                           ; R0 = the table
    POP   R1                           ; R1 = caller's return address
    PUSH  R0                          ; the table: argument 1
    MOV   R0, [META_CALL_DEPTH]
    MOV   R3, R0
    IGE   R3, META_CALL_STACK_SIZE
    JT    R3, __runtime_error_not_callable   ; nested too deep
    IADD  R0, META_CALL_STACK
    MOV   [R0], R1
    MOV   R0, [META_CALL_DEPTH]
    IADD  R0, 1
    MOV   [META_CALL_DEPTH], R0
    MOV   R1, __exec_call_return
    PUSH  R1
    IADD  R13, 1                      ; one more argument (variadic ABI)
    MOV   R0, R2
    JMP   __exec_valid
__exec_call_not_callable:
    POP   R0
    JMP   __runtime_error_not_callable

;; A __call handler returns here: SP -> the table argument. Keeps R0, R2,
;; R3 (the return values).
__exec_call_return:
    IADD  SP, 1
    MOV   R1, [META_CALL_DEPTH]
    ISUB  R1, 1
    MOV   [META_CALL_DEPTH], R1
    IADD  R1, META_CALL_STACK
    MOV   R1, [R1]
    JMP   R1

__exec_valid:
    ;; One return value unless the callee says otherwise: a compiled Lua
    ;; function stores its real count on return (node_return()); runtime
    ;; routines reached through here don't, and leave R2/R3 as scratch.
    MOV   R1, 1
    MOV   [RET_COUNT], R1
    MOV   R2, R0
    AND   R2, BOXED_CLOSURE_FLAG
    JT    R2, __exec_closure

    ; --- plain function: unchanged fast path ---
    AND   R0, BOXED_PAYLOAD
    OR    R0, V32_CART_PAGE
    JMP   R0

; ----------------------------------------------------------------------------
; Closure case: R0's payload is a RAM address of a closure record:
;   [0] code address (needs V32_CART_PAGE OR'd back in, same as plain funcs)
;   [1] upvalue count
;   [2..] boxed-upvalue pointers, in the order the callee expects them
;
; We push the upvalues onto the stack right here, in the caller's frame,
; BEFORE the tail-jump -- so from the target function's own prologue
; (PUSH BP; MOV BP,SP) they look exactly like ordinary trailing hidden
; parameters, indistinguishable from what a direct call would have pushed.
; ----------------------------------------------------------------------------
__exec_closure:
    MOV   R1, R0
    AND   R1, CLOSURE_ADDR_MASK
    SHL   R1, 1                        ; R1 = closure record address (boxed / 2)

    ; "CALL __builtin_exec" already pushed a return address before we got
    ; here. Pushing upvalues now would land them ON TOP of it instead of
    ; below it -- the callee's prologue would then see the return address
    ; at [BP+2] (where it expects its first upvalue) and the upvalue at
    ; [BP+1] (where it expects the return address), and the callee's own
    ; RET would pop a box pointer and jump to it as code. Pull the return
    ; address off first, push the upvalues underneath where it was, then
    ; push it back on top -- reproducing exactly the frame a direct call
    ; with pre-pushed upvalue arguments would produce.
    POP   R5                          ; R5 = return address

    MOV   R2, [R1+1]                   ; R2 = upvalue count
    MOV   R3, R1
    IADD  R3, 2                       ; R3 = base of upvalue pointer array
    IADD  R3, R2
    ISUB  R3, 1                       ; R3 = address of the LAST upvalue slot

__exec_push_upvalue_loop:
    JF    R2, __exec_push_done   ; Jump if R2 == 0 (exit when no more upvalues)
    MOV   R4, [R3]
    PUSH  R4
    ISUB  R3, 1
    ISUB  R2, 1
    JMP   __exec_push_upvalue_loop

__exec_push_done:
    PUSH  R5                          ; return address back on top, above the upvalues

    MOV   R0, [R1]                    ; R0 = code address
    OR    R0, V32_CART_PAGE
    JMP   R0

; ==============================================================================
; Runtime Panic Handler
; ==============================================================================
__runtime_error_not_callable:
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 1
    MOV   [BP-1], R0                ; __builtin_exec leaves it here untouched

    MOV   R0, 0xFF000080            ; A=FF, B=00, G=00, R=80 (see hex_to_v32_color)
    OUT   GPU_ClearColor, R0
    OUT   GPU_Command, GPUCommand_ClearScreen

    MOV   R0, 20
    PUSH  R0
    MOV   R0, 0
    PUSH  R0
    MOV   R0, __const_str_panic_banner
    OR    R0, BOXED_ROMSTRING
    PUSH  R0
    CALL  __builtin_print

    MOV   R0, 20
    PUSH  R0
    MOV   R0, 20
    PUSH  R0
    MOV   R0, __const_str_err_call_nil
    OR    R0, BOXED_ROMSTRING
    PUSH  R0
    CALL  __builtin_print

    MOV   R0, 20
    PUSH  R0
    MOV   R0, 40
    PUSH  R0
    MOV   R0, [BP-1]
    PUSH  R0
    CALL  __builtin_print

    JMP   __panic_halt

;; ---------------------------------------------------------------------------
;; A value of a numeric `for` that is not a number (the loop's start, limit or step).
;; R0 = the value; a numeric string becomes its number (as in Lua 5.4), anything
;; else is the runtime error "'for' ... must be a number". Preserves R1-R13.
;; The entry says which value it is (R1 = the message, kept for the panic).
;; ---------------------------------------------------------------------------
__for_number_initial:
    PUSH  R1
    MOV   R1, __const_str_err_for_initial
    JMP   __for_number
__for_number_limit:
    PUSH  R1
    MOV   R1, __const_str_err_for_limit
    JMP   __for_number
__for_number_step:
    PUSH  R1
    MOV   R1, __const_str_err_for_step
__for_number:
    PUSH  R2
    PUSH  R3
    MOV   R2, R0                    ; the value, for the panic screen
    CALL  __arith_coerce
    MOV   R3, R0
    AND   R3, NAN_VALUE
    IEQ   R3, NAN_VALUE
    JT    R3, __runtime_error_for
    POP   R3
    POP   R2
    POP   R1
    RET  

__runtime_error_for:
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 1
    MOV   [BP-1], R2                ; the value that wasn't a number

    MOV   R0, 0xFF000080
    OUT   GPU_ClearColor, R0
    OUT   GPU_Command, GPUCommand_ClearScreen

    MOV   R0, 20
    PUSH  R0
    MOV   R0, 0
    PUSH  R0
    MOV   R0, __const_str_panic_banner
    OR    R0, BOXED_ROMSTRING
    PUSH  R0
    CALL  __builtin_print

    MOV   R0, 20
    PUSH  R0
    MOV   R0, 20
    PUSH  R0
    OR    R1, BOXED_ROMSTRING      ; R1 = the message
    PUSH  R1
    CALL  __builtin_print

    MOV   R0, 20
    PUSH  R0
    MOV   R0, 40
    PUSH  R0
    MOV   R0, [BP-1]
    PUSH  R0
    CALL  __builtin_print

    JMP   __panic_halt

__panic_halt:
    WAIT                             ; Yield CPU frame to prevent runaway execution
    JMP   __panic_halt            ; Trap CPU in an infinite loop

;; ---------------------------------------------------------------------------
;; __panic_print_uint: allocation-free decimal printer for panic diagnostics.
;;
;; Panic handlers can't safely route a diagnostic number through the normal
;; print()/tostring()/ftoa() pipeline: __builtin_ftoa_scratch_a allocates its
;; 48-word scratch buffer lazily, on its very first call in the whole cart's
;; run. If that first call happens to land here -- because the OOM panic IS
;; the first thing that ever needed to print a number -- __malloc fails (the
;; heap is, after all, exactly why we're here), which re-enters
;; __oom_handler and loops forever instead of ever showing the value. This
;; touches no heap at all.
;;
;; Incoming Stack: [BP+4] = X, [BP+3] = Y, [BP+2] = non-negative raw integer
;; Clobbers: R0-R6
;; ---------------------------------------------------------------------------
    ;@fn __panic_print_uint clobbers=R1-R4
__panic_print_uint:
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 12                  ; up to 11 digits + null, built right-to-left

    MOV   R1, [BP+2]
    MOV   R2, BP
    ISUB  R2, 1
    MOV   R3, 0
    MOV   [R2], R3                ; null terminator

    MOV   R3, R1
    IEQ   R3, 0
    JF    R3, __panic_uint_extract
    ISUB  R2, 1
    MOV   R3, 48                  ; '0'
    MOV   [R2], R3
    JMP   __panic_uint_done

__panic_uint_extract:
    MOV   R3, R1
    IEQ   R3, 0
    JT    R3, __panic_uint_done

    MOV   R3, R1
    MOV   R4, 10
    IMOD  R3, R4
    MOV   R4, 10
    IDIV  R1, R4
    IADD  R3, 48                  ; digit -> ASCII

    ISUB  R2, 1
    MOV   [R2], R3
    JMP   __panic_uint_extract

__panic_uint_done:
    MOV   R1, [BP+4]               ; X
    MOV   R3, [BP+3]               ; Y

    PUSH  R2                       ; raw string pointer
    PUSH  R3                       ; Y
    PUSH  R1                       ; X
    CALL  __bios_print_text

    MOV   SP, BP
    POP   BP
    RET  


;; ===========================================================================
;; __builtin_cart_restart: TIC-80 reset() / PICO-8 run() -- start the cart
;; over, as the BIOS hands it over: hardware back to its defaults, a black
;; screen, the next frame, then the cart's first instruction with a fresh
;; stack. Global/heap/layer state is re-created by the cart's own start-up
;; code (which already assumes nothing about RAM -- the BIOS leaves it dirty).
;; Never returns.
;; ===========================================================================
__builtin_cart_restart:
    OUT   GPU_SelectedTexture, -1
    OUT   GPU_SelectedRegion, 0
    OUT   SPU_SelectedSound, -1
    OUT   SPU_SelectedChannel, 0
    OUT   INP_SelectedGamepad, 0      ; (V32IO_GAMEPAD: reset by the start-up code)
    OUT   SPU_Command, SPUCommand_StopAllChannels
    OUT   GPU_MultiplyColor, 0xFFFFFFFF
    OUT   GPU_ActiveBlending, GPUBlendingMode_Alpha
    OUT   GPU_ClearColor, 0xFF000000
    OUT   GPU_Command, GPUCommand_ClearScreen
    WAIT 
    MOV   SP, 0x003FFFFF                ; where the BIOS starts the stack
    MOV   BP, SP
    JMP   V32_CART_PAGE                 ; the cart's first instruction
;; ===========================================================================
;; SECTION: TABLE OPERATIONS
;; ===========================================================================

;; ---------------------------------------------------------------------------
;; Core Memory Allocator: Creates a new Table struct on the heap
;; Returns: R0 = Tagged Table Pointer (0x7F80....)
;; Register Usage: R1 (Callee-Saved for the CALLER's benefit -- separate from
;;                 the R2/R3/R6 save below, which only protects values across
;;                 the internal CALL __malloc)
;; ---------------------------------------------------------------------------
;; __table_key_streq (internal): R4 = stored key, R2 = search key
;;   -> R4 = 1 if BOTH are strings with equal contents, else 0.
;;   Preserves every other register.
;; Table keys used to be compared bitwise only, i.e. strings by ADDRESS:
;; t["ab"] = 1; t["a" .. "b"] read nil, and t["q" .. 1] = 5 twice created two
;; entries. Only literals worked, because equal literals share one ROM label.
;; Called only after the bitwise compare failed; the tag pre-check keeps
;; numeric / table / boolean keys off the slow path.
;; ---------------------------------------------------------------------------
    ;@fn __table_key_streq clobbers=R4
__table_key_streq:
    PUSH  R0
    MOV   R0, R2
    AND   R0, 0x7FC00000
    IEQ   R0, 0x7FC00000          ; search key string-tagged (ROM or RAM)?
    JF    R0, __table_key_streq_no
    MOV   R0, R4
    AND   R0, 0x7FC00000
    IEQ   R0, 0x7FC00000          ; stored key string-tagged?
    JF    R0, __table_key_streq_no
    ;; nil/false/true share the RAM-string tag with payload < 4
    MOV   R0, R2
    AND   R0, BOXED_PAYLOAD
    ILT   R0, 4
    JT    R0, __table_key_streq_no
    MOV   R0, R4
    AND   R0, BOXED_PAYLOAD
    ILT   R0, 4
    JT    R0, __table_key_streq_no

    ;; Inline char-by-char compare (one char per word, NUL-terminated). Most
    ;; mismatching keys differ in the first character, so this exits almost
    ;; immediately -- a full __builtin_eq per non-matching key made field
    ;; lookups in celeste's object tables ~40% slower overall.
    PUSH  R1
    PUSH  R2
    PUSH  R3
    MOV   R0, R2
    CALL  __unbox_string          ; R0 = address (uses R1)
    MOV   R2, R0
    MOV   R0, R4
    CALL  __unbox_string
    MOV   R4, R0                  ; R4 = stored key address, R2 = search key address
__table_key_streq_loop:
    MOV   R0, [R2]
    MOV   R1, [R4]
    MOV   R3, R0
    IEQ   R3, R1
    JF    R3, __table_key_streq_diff
    IEQ   R0, 0                   ; both hit NUL together -> equal
    JT    R0, __table_key_streq_same
    IADD  R2, 1
    IADD  R4, 1
    JMP   __table_key_streq_loop
__table_key_streq_same:
    MOV   R4, 1
    JMP   __table_key_streq_out
__table_key_streq_diff:
    MOV   R4, 0
__table_key_streq_out:
    POP   R3
    POP   R2
    POP   R1
    POP   R0
    RET  
__table_key_streq_no:
    MOV   R4, 0
    POP   R0
    RET  

;; ---------------------------------------------------------------------------
    ;@fn __builtin_table_new clobbers=R0
__builtin_table_new:
    PUSH  BP
    MOV   BP, SP

    ;; --- Callee-Save: preserve R1 for the CALLER ---
    ;; R1 is an ordinary general-purpose register the compiler can hand out
    ;; for any live value, so a caller can easily have something live in R1
    ;; across a `{}` table-constructor call (e.g. an earlier argument already
    ;; evaluated into R1 before a later argument is a table literal). This
    ;; function uses R1 as scratch below for the OOM check / header init
    ;; without ever saving it -- silently destroying that caller value.
    PUSH  R1

    ;; Save registers clobbered by __malloc
    PUSH  R2
    PUSH  R3
    PUSH  R6

    MOV   R0, 5
    PUSH  R0
    CALL  __malloc
    IADD  SP, 1

    ;; Restore registers
    POP   R6
    POP   R3
    POP   R2

    MOV   R1, R0
    IEQ   R1, 0
    JT    R1, __oom_handler   ; Fatal path -- halts the CPU, never returns, so
                              ; no need to restore R1 before jumping here.

    ;; Initialize table header
    MOV   R1, 0
    MOV   [R0], R1            ; Word 0: flags = nil
    MOV   [R0+1], R1          ; Word 1: length = 0
    MOV   [R0+2], R1          ; Word 2: array pointer = null
    MOV   [R0+3], R1          ; Word 3: hash pointer = null
    MOV   [R0+4], R1          ; Word 4: metatable (boxed table) = none

    OR    R0, BOXED_TABLE

    ;; --- Callee-Restore ---
    POP   R1

    MOV   SP, BP
    POP   BP
    RET  

;; ---------------------------------------------------------------------------
;; Table Read Indexer: t[k] -> Returns Value in R0 (or Nil if not found)
;; Incoming Stack: [BP+3] = Tagged Table Pointer, [BP+2] = Key
;; Register Usage: R1-R7 (Audited: reduced from 9 registers down to 7!)
;; ---------------------------------------------------------------------------
    ;@fn __builtin_table_get clobbers=R0,R6-R13
__builtin_table_get:
    PUSH  BP
    MOV   BP, SP

    ;; --- Callee-Save (only R1-R5 are used here; helpers save their own) ---
    PUSH  R1
    PUSH  R2
    PUSH  R3
    PUSH  R4
    PUSH  R5

    MOV   R1, [BP+3]          ; R1 = Tagged Table Pointer
    MOV   R2, [BP+2]          ; R2 = Search Key

    MOV   R4, R1
    AND   R4, BOXED_DATA
    IEQ   R4, BOXED_TABLE
    JF    R4, __table_get_non_table ; strings resolve methods; else trap
    AND   R1, BOXED_PAYLOAD   ; R1 = raw header address

    ;; --- Array part: whole-number key in 1..capacity -> O(1) read ---
    MOV   R3, R2
    AND   R3, NAN_VALUE
    IEQ   R3, NAN_VALUE       ; boxed (not a number)?
    JT    R3, __builtin_table_get_hash
    MOV   R3, R2
    CFI   R3                  ; R3 = (int) key
    MOV   R4, R3
    CIF   R4
    INE   R4, R2              ; fractional?
    JT    R4, __builtin_table_get_hash_num
    MOV   R4, R3
    ILT   R4, 1
    JT    R4, __builtin_table_get_hash_num
    MOV   R5, [R1]
    AND   R5, TABLE_ARRAYSIZE ; capacity
    MOV   R4, R3
    IGT   R4, R5
    JT    R4, __builtin_table_get_hash
    MOV   R5, [R1+2]          ; array data
    IADD  R5, R3
    ISUB  R5, 1
    MOV   R0, [R5]
    MOV   R4, R0
    IEQ   R4, BOXED_NIL       ; a hole: the metatable's __index may answer
    JT    R4, __builtin_table_get_not_found
    JMP   __builtin_table_get_done

__builtin_table_get_hash_num:
    ;; -0 and 0 are the same key
    MOV   R4, R2
    IEQ   R4, 0x80000000
    JF    R4, __builtin_table_get_hash
    MOV   R2, 0
__builtin_table_get_hash:
    MOV   R5, [R1+3]          ; hash block
    MOV   R4, R5
    IEQ   R4, 0
    JT    R4, __builtin_table_get_not_found
    CALL  __table_hash_find   ; R0 = slot address or 0
    MOV   R4, R0
    IEQ   R4, 0
    JT    R4, __builtin_table_get_not_found
    MOV   R0, [R0+1]
    MOV   R4, R0
    IEQ   R4, BOXED_NIL       ; a deleted key: as absent
    JT    R4, __builtin_table_get_not_found
    JMP   __builtin_table_get_done

;; --- Indexing a non-table: Lua strings index the string library --------
;; `s:sub(1, 2)` compiles to get(s, "sub") + call(self = s). Strings have
;; no metatable here, so this resolves the name directly to a wrapper with
;; the normal Lua function ABI (see __strmeth_* in string.s). Anything else
;; (or an unknown name on a string) is still the not-a-table runtime error.
__table_get_non_table:
    MOV   R4, R1
    AND   R4, BOXED_DATA
    IEQ   R4, BOXED_ROMSTRING
    JT    R4, __table_get_string_method
    MOV   R4, R1
    AND   R4, BOXED_DATA
    IEQ   R4, BOXED_RAMSTRING
    JF    R4, __runtime_error_not_table
    MOV   R4, R1
    AND   R4, BOXED_PAYLOAD
    ILT   R4, 4                   ; nil/false/true share the RAM-string tag
    JT    R4, __runtime_error_not_table
__table_get_string_method:
    PUSH  R2
    CALL  __builtin_string_method_lookup
    IADD  SP, 1
    MOV   R4, R0
    IEQ   R4, BOXED_NIL
    JT    R4, __runtime_error_not_table
    JMP   __builtin_table_get_done

__builtin_table_get_not_found:
    MOV   R0, R1              ; absent: nil, or the metatable's __index
    MOV   R0, [R0+4]          ; (R1 = the header here)
    JT    R0, __builtin_table_get_meta
    MOV   R0, BOXED_NIL

__builtin_table_get_done:
    POP   R5
    POP   R4
    POP   R3
    POP   R2
    POP   R1
    MOV   SP, BP
    POP   BP
    RET  
__builtin_table_get_meta:
    MOV   R0, [BP+3]
    MOV   R1, [BP+2]
    CALL  __table_index_miss
    JMP   __builtin_table_get_done

;; ---------------------------------------------------------------------------
;; __builtin_table_getk(t, k): t[k] for a string-LITERAL k (the compiler
;; emits this for t.field / t["field"]). Pushed like table_get (table,
;; then key); no BP frame. A literal's hash sits in the word before it, so
;; the lookup is: read hash, probe. A hit on a bitwise-equal key or an
;; empty slot settles it here; anything unusual (not a table, no hash
;; part, a non-literal string key in the probe path that might be equal by
;; content) defers to the general __builtin_table_get.
;; ---------------------------------------------------------------------------
    ;@fn __builtin_table_getk clobbers=R0,R7-R13
__builtin_table_getk:
    PUSH  R1
    PUSH  R2
    PUSH  R3
    PUSH  R4
    PUSH  R5
    PUSH  R6
    MOV   R1, [SP+8]          ; table (SP: 6 saved regs + return address)
    MOV   R2, R1
    AND   R2, BOXED_DATA
    IEQ   R2, BOXED_TABLE
    JF    R2, __table_getk_slow
    AND   R1, BOXED_PAYLOAD
    MOV   R1, [R1+3]          ; hash block
    MOV   R2, R1
    IEQ   R2, 0
    JT    R2, __table_getk_nil
    MOV   R3, [SP+7]          ; key (pooled literal)
    MOV   R2, R3
    AND   R2, BOXED_PAYLOAD
    OR    R2, V32_CART_PAGE
    ISUB  R2, 1
    MOV   R2, [R2]            ; its precomputed hash
    MOV   R4, [R1]
    ISUB  R4, 1               ; mask
    AND   R2, R4
    ;; R1 = block, R2 = slot index, R3 = key, R4 = mask; R0 = slot - 2,
    ;; R5 = the stored key (loaded once per probe), R6 = scratch
__table_getk_probe:
    MOV   R0, R2
    SHL   R0, 1
    IADD  R0, R1
    MOV   R5, [R0+2]          ; stored key
    MOV   R6, R5
    IEQ   R6, R3
    JT    R6, __table_getk_hit
    MOV   R6, R5
    IEQ   R6, BOXED_NIL
    JT    R6, __table_getk_nil ; empty slot: absent
    MOV   R6, R5
    AND   R6, BOXED_DATA
    IEQ   R6, BOXED_ROMSTRING
    JF    R6, __table_getk_not_rom
    ;; a ROM string: another pooled literal is a different string; a ROM
    ;; string from outside the pool (type() names, ...) may be equal by
    ;; content -> general routine
    AND   R5, BOXED_PAYLOAD
    OR    R5, V32_CART_PAGE
    MOV   R6, __string_pool_start
    IGT   R6, R5
    JT    R6, __table_getk_slow
    MOV   R6, __string_pool_end
    IGT   R6, R5
    JF    R6, __table_getk_slow
__table_getk_next:
    IADD  R2, 1
    AND   R2, R4
    JMP   __table_getk_probe
__table_getk_not_rom:
    ;; tables, functions, numbers, booleans: a different key. A run-time
    ;; string (RAM, payload >= 4) may be equal by content -> general routine
    MOV   R6, R5
    AND   R6, BOXED_DATA
    IEQ   R6, BOXED_RAMSTRING
    JF    R6, __table_getk_next
    AND   R5, BOXED_PAYLOAD
    ILT   R5, 4
    JT    R5, __table_getk_next
    JMP   __table_getk_slow
__table_getk_hit:
    MOV   R0, [R0+3]
    MOV   R5, R0
    IEQ   R5, BOXED_NIL       ; a deleted key: as absent
    JF    R5, __table_getk_done
__table_getk_nil:
    MOV   R0, [SP+8]          ; absent: nil, or the metatable's __index
    AND   R0, BOXED_PAYLOAD
    MOV   R0, [R0+4]
    JT    R0, __table_getk_meta
    MOV   R0, BOXED_NIL
__table_getk_done:
    POP   R6
    POP   R5
    POP   R4
    POP   R3
    POP   R2
    POP   R1
    RET  
__table_getk_meta:
    MOV   R0, [SP+8]
    MOV   R1, [SP+7]
    CALL  __table_index_miss
    JMP   __table_getk_done
__table_getk_slow:
    MOV   R0, [SP+8]
    PUSH  R0
    MOV   R0, [SP+8]          ; key (one more word on the stack now)
    PUSH  R0
    CALL  __builtin_table_get
    IADD  SP, 2
    JMP   __table_getk_done

;; __table_rawget_int (internal): R1 = raw table header, R3 = integer key
;;   -> R0 = t[key]. Array part read directly; otherwise the general get.
;;   Preserves everything else.
    ;@fn __table_rawget_int clobbers=R0,R6-R13
__table_rawget_int:
    PUSH  R2
    MOV   R0, R3
    ILT   R0, 1
    JT    R0, __table_rawget_int_slow
    MOV   R0, [R1]
    AND   R0, TABLE_ARRAYSIZE
    MOV   R2, R3
    IGT   R2, R0
    JT    R2, __table_rawget_int_slow
    MOV   R0, [R1+2]
    IADD  R0, R3
    ISUB  R0, 1
    MOV   R0, [R0]
    POP   R2
    RET  
__table_rawget_int_slow:
    MOV   R0, R1
    OR    R0, BOXED_TABLE
    PUSH  R0
    MOV   R0, R3
    CIF   R0
    PUSH  R0
    CALL  __builtin_table_get
    IADD  SP, 2
    POP   R2
    RET  

;; ===========================================================================
;; TABLE STORAGE (2026-09 rewrite)
;; ---------------------------------------------------------------------------
;; Header (5 words): [0] array capacity (low 16 bits), [1] length (the
;; border # reports), [2] array data pointer, [3] hash block pointer,
;; [4] metatable (a boxed table, 0 = none -- see METATABLES below).
;;
;; ARRAY PART: keys 1..capacity live in a plain word array (nil = absent):
;; O(1) reads/writes. It grows by doubling (min 8) when a key lands at most
;; 2x past the current capacity -- i.e. arrays built by appending, or
;; filled in any order from 1 -- and any of the new range's keys already
;; in the hash part are moved into it, so a key is never in both parts.
;;
;; HASH PART: open addressing, linear probing, power-of-two capacity, kept
;; at most 1/2 full. Block: [0] capacity, [1] used slots, then capacity
;; (key, value) pairs; an empty slot's key is BOXED_NIL (nil is never a
;; key). Deleting stores a nil value and leaves the key in place (keeps
;; probe chains intact); growing drops those.
;;
;; STRING KEYS hash by CONTENT (FNV-1a over the characters), so a string
;; built at runtime finds the same slot as the literal. String literals
;; carry their hash precomputed by the compiler in the word just before
;; them (the __string_pool_start..__string_pool_end block), so the usual
;; `obj.field` lookup never touches the characters at all.
;; ===========================================================================

;; __table_hash: R0 = key -> R0 = 32-bit hash. Preserves every other register.
    ;@fn __table_hash clobbers=R0
__table_hash:
    PUSH  R1
    PUSH  R2
    MOV   R1, R0
    AND   R1, 0x7FC00000
    IEQ   R1, 0x7FC00000      ; string-tagged (or nil/false/true)?
    JF    R1, __table_hash_scalar
    MOV   R1, R0
    AND   R1, BOXED_PAYLOAD
    ILT   R1, 4               ; nil/false/true
    JT    R1, __table_hash_scalar
    MOV   R1, R0
    AND   R1, BOXED_CATEGORY
    JT    R1, __table_hash_content      ; RAM string
    ;; ROM string: a pooled literal has its hash stored just before it
    MOV   R1, R0
    AND   R1, BOXED_PAYLOAD
    OR    R1, V32_CART_PAGE   ; R1 = ROM address
    MOV   R2, __string_pool_start
    IGT   R2, R1
    JT    R2, __table_hash_content
    MOV   R2, __string_pool_end
    IGT   R2, R1
    JF    R2, __table_hash_content
    ISUB  R1, 1
    MOV   R0, [R1]
    POP   R2
    POP   R1
    RET  
__table_hash_content:
    PUSH  R3
    CALL  __unbox_string      ; R0 = address (uses R1)
    MOV   R1, R0
    MOV   R0, 0x811C9DC5      ; FNV-1a offset basis
__table_hash_content_loop:
    MOV   R2, [R1]
    MOV   R3, R2
    IEQ   R3, 0
    JT    R3, __table_hash_content_done
    AND   R2, 0xFF
    XOR   R0, R2
    IMUL  R0, 16777619        ; FNV prime (32-bit wraparound)
    IADD  R1, 1
    JMP   __table_hash_content_loop
__table_hash_content_done:
    POP   R3
    POP   R2
    POP   R1
    RET  
__table_hash_scalar:
    IMUL  R0, 0x9E3779B1      ; Fibonacci hashing of the key bits
    MOV   R1, R0
    SHL   R1, -16             ; (negative count = logical right shift)
    XOR   R0, R1
    POP   R2
    POP   R1
    RET  

;; __table_hash_find: R5 = hash block, R2 = key (not nil)
;;   -> R0 = address of the key's slot, or 0 if absent;
;;      R3 = the empty slot the probe stopped at (insertion point).
;;   Preserves everything else.
    ;@fn __table_hash_find clobbers=R0,R3
__table_hash_find:
    PUSH  R4
    PUSH  R6
    PUSH  R7
    MOV   R0, R2
    CALL  __table_hash
    MOV   R7, [R5]
    ISUB  R7, 1               ; R7 = capacity - 1 (mask)
    AND   R0, R7              ; R0 = slot index
__table_hash_find_loop:
    MOV   R3, R0
    SHL   R3, 1
    IADD  R3, R5
    IADD  R3, 2               ; R3 = slot address
    MOV   R4, [R3]
    MOV   R6, R4
    IEQ   R6, BOXED_NIL
    JT    R6, __table_hash_find_absent
    MOV   R6, R4
    IEQ   R6, R2
    JT    R6, __table_hash_find_hit
    CALL  __table_key_streq   ; equal-content strings at different addresses
    JT    R4, __table_hash_find_hit
    IADD  R0, 1
    AND   R0, R7
    JMP   __table_hash_find_loop
__table_hash_find_hit:
    MOV   R0, R3
    JMP   __table_hash_find_out
__table_hash_find_absent:
    MOV   R0, 0
__table_hash_find_out:
    POP   R7
    POP   R6
    POP   R4
    RET  

;; __table_hash_new: R0 = capacity (power of two) -> R0 = new empty block.
;; Preserves everything else.
    ;@fn __table_hash_new clobbers=R0
__table_hash_new:
    PUSH  R1
    PUSH  R2
    PUSH  R3
    PUSH  R6
    MOV   R1, R0
    MOV   R2, R0
    SHL   R2, 1
    IADD  R2, 2
    PUSH  R1                  ; __malloc clobbers R1-R3, R6
    PUSH  R2
    CALL  __malloc
    IADD  SP, 1
    POP   R1
    MOV   R2, R0
    IEQ   R2, 0
    JT    R2, __oom_handler
    MOV   [R0], R1            ; capacity
    MOV   R2, 0
    MOV   [R0+1], R2          ; used
    MOV   R2, R0
    IADD  R2, 2
    MOV   R3, BOXED_NIL
__table_hash_new_fill:
    MOV   R6, R1
    IEQ   R6, 0
    JT    R6, __table_hash_new_done
    MOV   [R2], R3
    IADD  R2, 2
    ISUB  R1, 1
    JMP   __table_hash_new_fill
__table_hash_new_done:
    POP   R6
    POP   R3
    POP   R2
    POP   R1
    RET  

;; __table_hash_grow: R1 = raw table header. Replaces its hash block with
;; one of twice the capacity, re-inserting every live entry (nil-valued
;; ones are dropped). Preserves everything.
    ;@fn __table_hash_grow clobbers=none
__table_hash_grow:
    PUSH  R0
    PUSH  R2
    PUSH  R3
    PUSH  R4
    PUSH  R5
    PUSH  R6
    PUSH  R7
    PUSH  R8
    MOV   R6, [R1+3]          ; old block
    MOV   R7, [R6]            ; old capacity
    MOV   R0, R7
    SHL   R0, 1
    CALL  __table_hash_new
    MOV   R5, R0              ; new block
    MOV   [R1+3], R5
    IADD  R6, 2               ; old slot pointer
__table_hash_grow_loop:
    MOV   R8, R7
    IEQ   R8, 0
    JT    R8, __table_hash_grow_done
    MOV   R2, [R6]
    MOV   R8, R2
    IEQ   R8, BOXED_NIL
    JT    R8, __table_hash_grow_next
    MOV   R4, [R6+1]
    MOV   R8, R4
    IEQ   R8, BOXED_NIL
    JT    R8, __table_hash_grow_next
    CALL  __table_hash_find   ; R3 = empty slot (keys are unique)
    MOV   [R3], R2
    MOV   [R3+1], R4
    MOV   R8, [R5+1]
    IADD  R8, 1
    MOV   [R5+1], R8
__table_hash_grow_next:
    IADD  R6, 2
    ISUB  R7, 1
    JMP   __table_hash_grow_loop
__table_hash_grow_done:
    POP   R8
    POP   R7
    POP   R6
    POP   R5
    POP   R4
    POP   R3
    POP   R2
    POP   R0
    RET  

;; __table_hash_store: R1 = raw header, R2 = key (not nil), R3 = value.
;; Preserves everything.
    ;@fn __table_hash_store clobbers=none
__table_hash_store:
    PUSH  R0
    PUSH  R3
    PUSH  R4
    PUSH  R5
    PUSH  R6
    MOV   R6, R3              ; R6 = value
    MOV   R5, [R1+3]
    MOV   R4, R5
    INE   R4, 0
    JT    R4, __table_hash_store_have
    MOV   R4, R6
    IEQ   R4, BOXED_NIL       ; deleting from an empty hash: nothing to do
    JT    R4, __table_hash_store_done
    MOV   R0, 8
    CALL  __table_hash_new
    MOV   R5, R0
    MOV   [R1+3], R5
__table_hash_store_have:
    CALL  __table_hash_find
    MOV   R4, R0
    IEQ   R4, 0
    JT    R4, __table_hash_store_insert
    MOV   [R0+1], R6          ; existing key: overwrite (nil = delete)
    JMP   __table_hash_store_done
__table_hash_store_insert:
    MOV   R4, R6
    IEQ   R4, BOXED_NIL       ; absent key set to nil: nothing to do
    JT    R4, __table_hash_store_done
    ;; keep the block at most 1/2 full: (used + 1) * 2 > capacity -> grow.
    ;; (3/4 made linear-probe chains long: a lookup of an absent field,
    ;; e.g. `o.solid_obj` on objects without one, averaged ~8 probes.)
    MOV   R4, [R5+1]
    IADD  R4, 1
    SHL   R4, 1
    MOV   R0, [R5]
    IGT   R4, R0
    JF    R4, __table_hash_store_put
    CALL  __table_hash_grow
    MOV   R5, [R1+3]
    CALL  __table_hash_find   ; new insertion slot in R3
__table_hash_store_put:
    MOV   [R3], R2
    MOV   [R3+1], R6
    MOV   R4, [R5+1]
    IADD  R4, 1
    MOV   [R5+1], R4
__table_hash_store_done:
    POP   R6
    POP   R5
    POP   R4
    POP   R3
    POP   R0
    RET  

;; __table_array_grow: R1 = raw header, R4 = key that must fit (int >= 1).
;; New capacity = max(8, 2 * capacity), doubled until it covers R4. Copies
;; the old array, nil-fills the rest, then moves any of the new range's
;; keys out of the hash part. Preserves everything.
    ;@fn __table_array_grow clobbers=none
__table_array_grow:
    PUSH  R0
    PUSH  R2
    PUSH  R3
    PUSH  R5
    PUSH  R6
    PUSH  R7
    PUSH  R8
    MOV   R5, [R1]
    AND   R5, TABLE_ARRAYSIZE ; R5 = old capacity
    MOV   R6, R5
    SHL   R6, 1
    MOV   R7, R6
    ILT   R7, 8
    JF    R7, __table_array_grow_cap
    MOV   R6, 8
__table_array_grow_cap:
    MOV   R7, R6
    ILT   R7, R4
    JF    R7, __table_array_grow_alloc
    SHL   R6, 1
    JMP   __table_array_grow_cap
__table_array_grow_alloc:
    PUSH  R1                  ; __malloc clobbers R1-R3, R6
    PUSH  R6
    PUSH  R6
    CALL  __malloc
    IADD  SP, 1
    POP   R6
    POP   R1
    MOV   R7, R0
    IEQ   R7, 0
    JT    R7, __oom_handler
    ;; copy old contents, nil-fill the rest
    MOV   R2, [R1+2]          ; old data (may be 0 when capacity 0)
    MOV   R3, 0
__table_array_grow_copy:
    MOV   R7, R3
    IGE   R7, R6
    JT    R7, __table_array_grow_copied
    MOV   R8, BOXED_NIL
    MOV   R7, R3
    ILT   R7, R5
    JF    R7, __table_array_grow_put
    MOV   R8, R2
    IADD  R8, R3
    MOV   R8, [R8]
__table_array_grow_put:
    MOV   R7, R0
    IADD  R7, R3
    MOV   [R7], R8
    IADD  R3, 1
    JMP   __table_array_grow_copy
__table_array_grow_copied:
    MOV   [R1+2], R0
    MOV   R7, [R1]
    AND   R7, 0xFFFF0000
    OR    R7, R6
    MOV   [R1], R7
    ;; migrate keys old_capacity+1 .. new_capacity out of the hash part
    MOV   R8, [R1+3]
    MOV   R7, R8
    IEQ   R7, 0
    JT    R7, __table_array_grow_done
    MOV   R3, R5              ; R3 = index - 1
__table_array_grow_migrate:
    MOV   R7, R3
    IGE   R7, R6
    JT    R7, __table_array_grow_done
    PUSH  R3
    PUSH  R5
    MOV   R2, R3
    IADD  R2, 1
    CIF   R2                  ; key as a Lua number
    MOV   R5, R8
    CALL  __table_hash_find
    MOV   R7, R0
    POP   R5
    POP   R3
    IEQ   R7, 0
    JT    R7, __table_array_grow_migrate_next
    MOV   R2, [R0+1]          ; value
    MOV   R7, BOXED_NIL
    MOV   [R0+1], R7          ; delete from hash
    MOV   R7, [R1+2]
    IADD  R7, R3
    MOV   [R7], R2
__table_array_grow_migrate_next:
    IADD  R3, 1
    JMP   __table_array_grow_migrate
__table_array_grow_done:
    POP   R8
    POP   R7
    POP   R6
    POP   R5
    POP   R3
    POP   R2
    POP   R0
    RET  

;; ---------------------------------------------------------------------------
;; Table Write Indexer: t[k] = v
;; Incoming Stack: [BP+4] = Table Pointer, [BP+3] = Key, [BP+2] = Value
;; Preserves R1-R8.
;; ---------------------------------------------------------------------------
    ;@fn __builtin_table_set clobbers=R0,R9-R13
__builtin_table_set:
    PUSH  BP
    MOV   BP, SP
    PUSH  R1
    PUSH  R2
    PUSH  R3
    PUSH  R4
    PUSH  R5
    PUSH  R6
    PUSH  R7
    PUSH  R8

    MOV   R1, [BP+4]          ; R1 = Tagged Table Pointer
    MOV   R2, [BP+3]          ; R2 = Key
    MOV   R3, [BP+2]          ; R3 = Value
    MOV   R4, R1
    AND   R4, BOXED_DATA
    IEQ   R4, BOXED_TABLE
    JF    R4, __runtime_error_not_table
    AND   R1, BOXED_PAYLOAD

    ;; nil key: nothing can be stored (Lua raises an error; ignored here)
    MOV   R4, R2
    IEQ   R4, BOXED_NIL
    JT    R4, __builtin_table_set_done

    ;; a metatable: __newindex may take a key the table doesn't have
    MOV   R4, [R1+4]
    JT    R4, __builtin_table_set_meta
__builtin_table_set_raw:

    ;; --- whole-number key >= 1? ---
    MOV   R4, R2
    AND   R4, NAN_VALUE
    IEQ   R4, NAN_VALUE
    JT    R4, __builtin_table_set_hash
    MOV   R4, R2
    CFI   R4                  ; R4 = (int) key
    MOV   R5, R4
    CIF   R5
    INE   R5, R2
    JT    R5, __builtin_table_set_hash_num
    MOV   R5, R4
    ILT   R5, 1
    JT    R5, __builtin_table_set_hash_num

    MOV   R5, [R1]
    AND   R5, TABLE_ARRAYSIZE ; capacity
    MOV   R6, R4
    IGT   R6, R5
    JF    R6, __builtin_table_set_array
    ;; beyond capacity: grow the array if the key is within 2x of it
    ;; (min 8) and a real value is being stored; otherwise hash it
    MOV   R6, R3
    IEQ   R6, BOXED_NIL
    JT    R6, __builtin_table_set_hash
    MOV   R6, R5
    SHL   R6, 1
    MOV   R7, R6
    ILT   R7, 8
    JF    R7, __builtin_table_set_grow_limit
    MOV   R6, 8
__builtin_table_set_grow_limit:
    MOV   R7, R4
    IGT   R7, R6
    JT    R7, __builtin_table_set_hash
    CALL  __table_array_grow

__builtin_table_set_array:
    MOV   R6, [R1+2]
    IADD  R6, R4
    ISUB  R6, 1
    MOV   [R6], R3
    ;; --- keep the border (#t) in step ---
    MOV   R7, [R1+1]          ; length
    MOV   R6, R3
    IEQ   R6, BOXED_NIL
    JT    R6, __builtin_table_set_array_nil
    MOV   R6, R7
    IADD  R6, 1
    IEQ   R6, R4              ; appended right after the border?
    JF    R6, __builtin_table_set_done
    ;; extend over any values already stored beyond it
    MOV   R5, [R1]
    AND   R5, TABLE_ARRAYSIZE
    MOV   R8, [R1+2]
__builtin_table_set_extend:
    MOV   R6, R4
    IGE   R6, R5
    JT    R6, __builtin_table_set_extend_hash
    MOV   R6, R8
    IADD  R6, R4
    MOV   R6, [R6]            ; slot for key R4 + 1
    IEQ   R6, BOXED_NIL
    JT    R6, __builtin_table_set_extended
    IADD  R4, 1
    JMP   __builtin_table_set_extend
__builtin_table_set_extend_hash:
    ;; the border reached the end of the array part: if the hash part holds
    ;; key capacity+1, pull that range into the array and keep going
    MOV   R6, [R1+3]
    MOV   R7, R6
    IEQ   R7, 0
    JT    R7, __builtin_table_set_extended
    PUSH  R2
    PUSH  R5
    MOV   R2, R4
    IADD  R2, 1
    CIF   R2
    MOV   R5, R6
    CALL  __table_hash_find
    POP   R5
    POP   R2
    MOV   R7, R0
    IEQ   R7, 0
    JT    R7, __builtin_table_set_extended
    MOV   R7, [R0+1]
    IEQ   R7, BOXED_NIL
    JT    R7, __builtin_table_set_extended
    PUSH  R4
    IADD  R4, 1
    CALL  __table_array_grow   ; migrates capacity+1 .. new capacity
    POP   R4
    MOV   R5, [R1]
    AND   R5, TABLE_ARRAYSIZE
    MOV   R8, [R1+2]
    JMP   __builtin_table_set_extend
__builtin_table_set_extended:
    MOV   [R1+1], R4
    JMP   __builtin_table_set_done
__builtin_table_set_array_nil:
    ;; clearing a slot inside the border moves the border below it
    MOV   R6, R4
    IGT   R6, R7
    JT    R6, __builtin_table_set_done
    ISUB  R4, 1
    MOV   [R1+1], R4
    JMP   __builtin_table_set_done

__builtin_table_set_hash_num:
    MOV   R4, R2
    IEQ   R4, 0x80000000      ; -0 is the same key as 0
    JF    R4, __builtin_table_set_hash
    MOV   R2, 0
__builtin_table_set_hash:
    CALL  __table_hash_store

    JMP   __builtin_table_set_done
__builtin_table_set_meta:
    MOV   R0, [BP+4]
    CALL  __table_newindex    ; R0 = 1 when __newindex took it
    JF    R0, __builtin_table_set_raw

__builtin_table_set_done:
    POP   R8
    POP   R7
    POP   R6
    POP   R5
    POP   R4
    POP   R3
    POP   R2
    POP   R1
    MOV   SP, BP
    POP   BP
    RET  

;; ---------------------------------------------------------------------------
;; Table Set With Shift: Inserts value at position, shifting elements to make
;;                       room.
;;
;; Incoming Stack: [BP+5] = Tagged Table Pointer, [BP+4] = Position (1-based),
;;                 [BP+3] = Value to insert, [BP+2] = Current array length
;; Register Usage: R1-R13
;; Returns: R0 = inserted value
;; ---------------------------------------------------------------------------
__builtin_table_set_with_shift:
    PUSH  BP
    MOV   BP, SP

    ;; --- Callee-Save: Preserve working registers ---
    PUSH  R1
    PUSH  R2
    PUSH  R3
    PUSH  R4
    PUSH  R5
    PUSH  R6
    PUSH  R7
    PUSH  R8
    PUSH  R9
    PUSH  R10
    PUSH  R11
    PUSH  R12
    PUSH  R13

    ;; --- Load arguments ---
    MOV   R1, [BP+5]          ; R1 = Tagged Table Pointer
    MOV   R2, [BP+4]          ; R2 = Insertion position (1-based)
    MOV   R3, [BP+3]          ; R3 = Value to insert (return this)
    MOV   R4, [BP+2]          ; R4 = Current array length

    ;; --- Unbox table to get raw address ---
    MOV   R5, R1
    AND   R5, BOXED_PAYLOAD   ; R5 = Raw table header address

    ;; --- Get array pointer from table header ---
    MOV   R6, [R5+2]          ; R6 = Array data pointer (Word 2)

    ;; --- FIX: Allocate array if it doesn't exist ---
    MOV   R7, R6
    IEQ   R7, 0
    JT    R7, __set_with_shift_allocate_array
    JMP   __set_with_shift_get_capacity

__set_with_shift_allocate_array:
    ;; Allocate initial array (start with capacity of 8)
    PUSH  R1
    PUSH  R2
    PUSH  R3
    PUSH  R4
    PUSH  R5
    PUSH  R6

    MOV   R0, 8
    PUSH  R0
    CALL  __malloc
    IADD  SP, 1

    POP   R6
    POP   R5
    POP   R4
    POP   R3
    POP   R2
    POP   R1

    MOV   R6, R0              ; R6 = New array pointer
    MOV   R7, R0
    IEQ   R7, 0
    JT    R7, __oom_handler

    ;; Store new array pointer in table header
    MOV   [R5+2], R6

    ;; Set initial capacity to 8 in flags
    MOV   R7, [R5]
    MOV   R8, TABLE_ARRAYSIZE
    NOT   R8
    AND   R7, R8              ; Clear old capacity
    OR    R7, 8               ; Set capacity to 8
    MOV   [R5], R7

    ;; Fall through to get capacity

__set_with_shift_get_capacity:
    ;; --- Get capacity from flags ---
    MOV   R7, [R5]            ; R7 = Flags/array capacity (Word 0)
    AND   R7, TABLE_ARRAYSIZE  ; Extract capacity from lower bits

    ;; --- Check if we need to reallocate ---
    IGT   R2, R7
    JT    R2, __set_with_shift_reallocate
    JMP   __set_with_shift_check_shifting

__set_with_shift_reallocate:
    ;; --- Calculate new capacity (double current, or position+1, whichever is larger) ---
    MOV   R8, R7
    IADD  R8, R7              ; R8 = 2 * current capacity

    MOV   R9, R2
    IADD  R9, 1               ; R9 = position + 1 (minimum needed)

    IGT   R8, R9
    JT    R8, __set_with_shift_use_doubled
    MOV   R8, R9              ; Use position+1 if doubling isn't enough

__set_with_shift_use_doubled:
    ;; --- Allocate new array ---
    PUSH  R1
    PUSH  R2
    PUSH  R3
    PUSH  R4
    PUSH  R5
    PUSH  R6
    PUSH  R7

    MOV   R0, R8
    PUSH  R0
    CALL  __malloc
    IADD  SP, 1

    POP   R7
    POP   R6
    POP   R5
    POP   R4
    POP   R3
    POP   R2
    POP   R1

    MOV   R8, R0              ; R8 = New array pointer
    MOV   R9, R0
    IEQ   R9, 0
    JT    R9, __oom_handler

    ;; --- Copy existing elements to new array ---
    MOV   R9, 0               ; R9 = Source index counter
    MOV   R10, 0              ; R10 = Destination index counter

__set_with_shift_copy_loop:
    IGE   R9, R4
    JT    R9, __set_with_shift_copy_done

    MOV   R11, R6
    IADD  R11, R9

    MOV   R12, R8
    IADD  R12, R10

    MOV   R13, [R11]
    MOV   [R12], R13

    IADD  R9, 1
    IADD  R10, 1
    JMP   __set_with_shift_copy_loop

__set_with_shift_copy_done:
    ;; --- Update table header with new array pointer ---
    MOV   [R5+2], R8

    ;; --- Update capacity in flags: clear old capacity, set new ---
    MOV   R6, [R5]            ; Get current flags
    MOV   R7, TABLE_ARRAYSIZE
    NOT   R7                  ; Invert mask to clear capacity bits
    AND   R6, R7              ; Clear old capacity
    OR    R6, R8              ; Set new capacity
    MOV   [R5], R6

    ;; --- Update R6 to point to new array for shifting ---
    MOV   R6, R8

__set_with_shift_check_shifting:
    ;; --- Check if we need to shift elements ---
    MOV   R8, R4
    IADD  R8, 1
    IEQ   R2, R8
    JT    R2, __set_with_shift_no_shift

    ;; --- Shift elements from position to end one slot to the right ---
    MOV   R8, R4              ; R8 = Current length (last valid index)
    MOV   R9, R2              ; R9 = Insertion position

__set_with_shift_loop:
    IGE   R9, R8
    JT    R9, __set_with_shift_store

    MOV   R10, R6
    IADD  R10, R8

    MOV   R11, R6
    IADD  R11, R8
    IADD  R11, 1

    MOV   R12, [R10]
    MOV   [R11], R12

    ISUB  R8, 1
    JMP   __set_with_shift_loop

__set_with_shift_no_shift:
    JMP   __set_with_shift_store

__set_with_shift_store:
    ;; --- Store the new value at the insertion position ---
    MOV   R8, R6
    IADD  R8, R2
    ISUB  R8, 1

    MOV   [R8], R3

    ;; --- Update array length in table header ---
    IADD  R4, 1
    MOV   [R5+1], R4

    ;; --- Return the inserted value ---
    MOV   R0, R3

__set_with_shift_done:
    ;; --- Callee-Restore: Pop all working registers ---
    POP   R13
    POP   R12
    POP   R11
    POP   R10
    POP   R9
    POP   R8
    POP   R7
    POP   R6
    POP   R5
    POP   R4
    POP   R3
    POP   R2
    POP   R1

    MOV   SP, BP
    POP   BP
    RET  

;; ---------------------------------------------------------------------------
;; Table Insert: table.insert(t, [pos], value)
;; Inserts a value at a specific 1-based position, shifting existing
;; elements up by one. With no position (BOXED_NIL), appends at the end.
;; Negative positions count backward from the end: resolved = length + pos + 1
;; (this is a v32lua extension, not standard Lua -- e.g.
;; table.insert(t, -1, x) inserts immediately before the current last
;; element, per this project's test suite).
;;
;; Incoming Stack: [BP+4] = Tagged Table Pointer, [BP+3] = Position (1-based
;;                 float, negative float, or BOXED_NIL for append),
;;                 [BP+2] = Value to insert
;; Register Usage: R1-R8
;; Returns: R0 = inserted value (for pico8 add() compatibility)
;;
;; IMPLEMENTATION NOTE: this is implemented entirely in terms of
;; __builtin_table_get / __builtin_table_set rather than raw array-pointer
;; arithmetic. Real contiguous-array storage doesn't exist yet in this
;; runtime (table capacity is always 0 -- see __builtin_table_set's
;; __builtin_table_set_reallocate TODO), so every table here is actually
;; backed by the hash association list. The previous version of this
;; function assumed a working raw array and wrote directly through the
;; (always-null) array pointer -- on top of never converting the boxed
;; float position to a raw integer, and corrupting the position on the
;; default-append path. Routing every read/write through
;; table_get/table_set fixes all of that at once, and gets negative
;; positions and correct length bookkeeping for free (the shift loop's
;; first table_set naturally lands on key == length + 1, which
;; __builtin_table_set already bumps the tracked length for). It'll also
;; automatically become real O(1) array shifting for free whenever array
;; allocation is implemented, with zero changes needed here.
;; ---------------------------------------------------------------------------
    ;@fn __builtin_table_insert clobbers=R0,R9-R13
__builtin_table_insert:
    PUSH  BP
    MOV   BP, SP

    ;; --- Callee-Save: Preserve all 8 working registers ---
    PUSH  R1
    PUSH  R2
    PUSH  R3
    PUSH  R4
    PUSH  R5
    PUSH  R6
    PUSH  R7
    PUSH  R8

    ;; --- Load arguments from caller's stack frame ---
    MOV   R1, [BP+4]          ; R1 = Tagged Table Pointer (kept BOXED -- table_get/table_set want it boxed)
    MOV   R2, [BP+3]          ; R2 = Position (boxed float, negative float, or BOXED_NIL)
    MOV   R3, [BP+2]          ; R3 = Value to insert (boxed; preserved for the final store + return)

    ;; --- Validate table type, and get the raw header address just to
    ;;     read the current contiguous length directly (Word 1) ---
    MOV   R4, R1
    AND   R4, BOXED_DATA
    IEQ   R4, BOXED_TABLE
    JF    R4, __runtime_error_not_table
    MOV   R5, R1
    AND   R5, BOXED_PAYLOAD   ; R5 = raw table header address

    MOV   R6, [R5+1]          ; R6 = current contiguous length

    ;; --- Resolve the insertion position into R7 (a raw integer) ---
    MOV   R4, R2
    IEQ   R4, BOXED_NIL
    JT    R4, __insert_default_append

    ;; Position given: convert boxed float -> raw integer.
    MOV   R7, R2
    CFI   R7                  ; R7 = integer position (may be negative)

    MOV   R4, R7
    ILT   R4, 0                ; negative position?
    JF    R4, __insert_position_resolved

    ;; Negative position: count backward from the end (v32lua extension).
    ;; resolved = length + position + 1
    MOV   R4, R6
    IADD  R4, R7
    IADD  R4, 1
    MOV   R7, R4
    JMP   __insert_position_resolved

__insert_default_append:
    MOV   R7, R6
    IADD  R7, 1                ; resolved = length + 1

__insert_position_resolved:
    ;; Clamp: never let the resolved position fall below 1. Guards against
    ;; pathological / wildly out-of-range negative input producing a
    ;; degenerate shift loop. (Real Lua would raise an error for an
    ;; out-of-bounds position; this runtime doesn't have error-raising
    ;; infrastructure for library calls yet, so clamping is the safe,
    ;; cheap guard for now.)
    MOV   R4, R7
    ILT   R4, 1
    JF    R4, __insert_shift_init
    MOV   R7, 1

__insert_shift_init:
    MOV   R8, R6                ; R8 = shift cursor, starts at current length

    ;; --- Shift elements from length down to position (descending) ---
    ;; Each step: t[cursor + 1] = t[cursor]. Going from the top down avoids
    ;; overwriting a value before it's been read. The very first shift (if
    ;; any) writes to key == length + 1, which naturally makes
    ;; __builtin_table_set bump the tracked length by one as a side
    ;; effect -- exactly the length update this insert needs, for free.
__insert_shift_loop:
    MOV   R4, R8
    ILT   R4, R7                ; cursor < position? shifting is done
    JT    R4, __insert_store_value

    ;; --- fetch t[cursor] ---
    MOV   R4, R8
    CIF   R4                    ; R4 = boxed float source key
    PUSH  R1                    ; table pointer
    PUSH  R4                    ; key = cursor
    CALL  __builtin_table_get
    IADD  SP, 2
    MOV   R5, R0                ; R5 = fetched value (boxed)

    ;; --- store into t[cursor + 1] ---
    MOV   R6, R8
    IADD  R6, 1
    CIF   R6                    ; R6 = boxed float destination key

    PUSH  R1                    ; table pointer
    PUSH  R6                    ; key = cursor + 1
    PUSH  R5                    ; value = fetched
    CALL  __builtin_table_set
    IADD  SP, 3

    ISUB  R8, 1
    JMP   __insert_shift_loop

__insert_store_value:
    ;; --- Store the new value at the resolved position ---
    MOV   R4, R7
    CIF   R4                    ; R4 = boxed float position key

    PUSH  R1                    ; table pointer
    PUSH  R4                    ; key = position
    PUSH  R3                    ; value to insert
    CALL  __builtin_table_set
    IADD  SP, 3

    ;; --- Return the inserted value (for add() compatibility) ---
    MOV   R0, R3

__insert_done:
    ;; --- Callee-Restore: Pop working registers in reverse order (LIFO) ---
    POP   R8
    POP   R7
    POP   R6
    POP   R5
    POP   R4
    POP   R3
    POP   R2
    POP   R1

    MOV   SP, BP
    POP   BP
    RET  

;; __builtin_table_len: Get length of table array part
    ;@fn __builtin_table_len clobbers=R0-R2
__builtin_table_len:
    PUSH  BP
    MOV   BP, SP
    MOV   R0, [BP+2]    ; Table pointer
    CALL  __unbox_table   ; R0 = raw table header address

    ;; --- VALIDATION: Use R1 and R2 (scratch) to preserve R0 ---
    MOV   R1, R0          ; Copy for null check
    IEQ   R1, 0
    JT    R1, __table_len_invalid

    MOV   R2, R0          ; Copy for ROM check (R0 still intact!)
    IGE   R2, V32_CART_PAGE   ; ROM starts here (0x20000000 cart, 0x10000000 BIOS)
    JT    R2, __table_len_invalid

    MOV   R0, [R0+1]     ; Read array length from header word 1
    CIF   R0             ; Convert to float
    MOV   SP, BP
    POP   BP
    RET  

__table_len_invalid:
    MOV   R0, 0
    CIF   R0
    MOV   SP, BP
    POP   BP
    RET  

;; ---------------------------------------------------------------------------
;; Unbox Table Pointer: Converts a tagged Table pointer to raw RAM address
;;
;; Input:  R0 = Tagged Table Pointer (0xFF8xxxxx)
;; Output: R0 = Raw RAM address (unboxed)
;; Clobbers: R0 only
;; ---------------------------------------------------------------------------
    ;@fn __unbox_table clobbers=R0
__unbox_table:
    ;; Strip the BOXED_TABLE tag (0xFF800000) from the upper bits
    ;; to get the raw heap address
    AND   R0, BOXED_PAYLOAD

    ;; Verify the result is a valid pointer (optional safety check)
    ;; IEQ  R2, R0
    ;; JT   R2, __unbox_table_valid
    ;; MOV  R0, 0  ; Return 0 for invalid (shouldn't happen in practice)
__unbox_table_valid:
    RET  

;; ---------------------------------------------------------------------------
;; Runtime Panic Handlers for Table Errors
;; ---------------------------------------------------------------------------
__runtime_error_not_table:
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 1
    MOV   [BP-1], R1                ; the value that wasn't a table

    MOV   R0, 0xFF000080
    OUT   GPU_ClearColor, R0
    OUT   GPU_Command, GPUCommand_ClearScreen

    MOV   R0, 20
    PUSH  R0
    MOV   R0, 0
    PUSH  R0
    MOV   R0, __const_str_panic_banner
    OR    R0, BOXED_ROMSTRING
    PUSH  R0
    CALL  __builtin_print

    MOV   R0, 20
    PUSH  R0
    MOV   R0, 20
    PUSH  R0
    MOV   R0, __const_str_err_not_table
    OR    R0, BOXED_ROMSTRING
    PUSH  R0
    CALL  __builtin_print

    MOV   R0, 20
    PUSH  R0
    MOV   R0, 40
    PUSH  R0
    MOV   R0, [BP-1]
    PUSH  R0
    CALL  __builtin_print

    JMP   __panic_halt

__runtime_error_hash_overflow:
    ;; Trap CPU if hash part exceeds 7 pairs (until dynamic rehashing
    ;; is implemented)
    HLT  
    JMP   __runtime_error_hash_overflow

;; ===========================================================================
;; LUA TABLE LIBRARY IMPLEMENTATION FOR V32LUA
;; ===========================================================================
;;
;; This file implements the standard Lua table library functions for the v32lua
;; compiler targeting the Vircon32 fantasy console.
;;
;; Implemented functions:
;; - table.insert(t, [pos], value)  - Already exists as __builtin_table_insert
;; - table.remove(t, [pos])          - Remove element at position
;; - table.sort(t, [comp])          - Sort table elements in-place
;; - table.concat(t, [sep], [i], [j]) - Concatenate array elements
;; - table.move(a1, f, e, t, [a2])  - Move elements between tables
;; - table.pack(...)                - Pack arguments into table with .n field
;; - table.unpack(t, [i], [j])      - Unpack table elements as multiple returns
;;
;; ===========================================================================

;; ===========================================================================
;; SECTION: TABLE REMOVE
;; ===========================================================================

;; ---------------------------------------------------------------------------
;; table.remove(t, [pos]) - Removes and returns element at position pos
;; If pos is nil or > length, removes and returns last element
;; Shifts elements left to fill the gap
;;
;; Incoming Stack: [BP+3] = Tagged Table Pointer, [BP+2] = Position (optional)
;; Returns: R0 = removed value
;; Register Usage: R1-R8
;; ---------------------------------------------------------------------------
    ;@fn __builtin_table_remove clobbers=R0,R12-R13
__builtin_table_remove:
    PUSH  BP
    MOV   BP, SP

    ;; --- Callee-Save: Preserve 8 working registers ---
    PUSH  R1
    PUSH  R2
    PUSH  R3
    PUSH  R4
    PUSH  R5
    PUSH  R6
    PUSH  R7
    PUSH  R8
    PUSH  R9
    PUSH  R10
    PUSH  R11

    ;; --- Load arguments ---
    MOV   R1, [BP+3]          ; R1 = Tagged Table Pointer
    MOV   R2, [BP+2]          ; R2 = Position (may be NIL)

    ;; --- Validate table ---
    MOV   R3, R1
    AND   R3, BOXED_DATA
    IEQ   R3, BOXED_TABLE
    JF    R3, __runtime_error_not_table

    ;; --- Unbox table pointer ---
    AND   R1, BOXED_PAYLOAD   ; R1 = raw table header address

    ;; --- Get current length ---
    MOV   R3, [R1+1]          ; R3 = current array length

    ;; --- Handle default position (NIL or > length = remove last) ---
    MOV   R4, R2
    IEQ   R4, BOXED_NIL
    JT    R4, __table_remove_last

    ;; Convert position to integer if it's a float
    MOV   R4, R2
    AND   R4, NAN_VALUE
    IEQ   R4, NAN_VALUE
    JT    R4, __table_remove_check_position

    ;; It's a float, convert to integer
    MOV   R4, R2
    CFI   R4
    MOV   R5, R4
    CIF   R5
    INE   R5, R2
    JT    R5, __table_remove_invalid_position

__table_remove_check_position:
    ;; --- Resolve negative positions: count backward from the end,
    ;; matching table.insert's convention (resolved = length + pos + 1).
    ;; e.g. table.remove(t, -1) removes the last element. Previously any
    ;; position < 1 -- including legitimate negative ones -- fell straight
    ;; into the "invalid position" bailout below without ever being
    ;; resolved, so table.remove(t, -1) always returned nil without
    ;; touching the table.
    MOV   R8, R4
    ILT   R8, 0
    JF    R8, __table_remove_bounds_check
    MOV   R8, R3
    IADD  R8, R4
    IADD  R8, 1
    MOV   R4, R8

__table_remove_bounds_check:
    ;; R4 now contains integer position (use R8 for comparisons)
    MOV   R8, R4
    ILT   R8, 1
    JT    R8, __table_remove_invalid_position
    MOV   R8, R4
    IGT   R8, R3
    JT    R8, __table_remove_last
    JMP   __table_remove_at_position

__table_remove_last:
    MOV   R4, R3              ; Position = length (last element)

__table_remove_at_position:
    MOV   R6, R4
    ILT   R6, 1                ; empty table: nothing to remove
    JT    R6, __table_remove_not_found
    ;; --- Get array data pointer ---
    MOV   R5, [R1+2]          ; R5 = array data pointer

    ;; --- Whole range 1..length in the array part? Then shift in place ---
    MOV   R6, R5
    IEQ   R6, 0
    JT    R6, __table_remove_hash_path
    MOV   R6, [R1]
    AND   R6, TABLE_ARRAYSIZE
    ILT   R6, R3               ; capacity < length: part of it is hashed
    JT    R6, __table_remove_hash_path   ; No array (the common case: __builtin_table_set's
                                     ; reallocation path is currently a stub, so array-
                                     ; backed tables never actually exist) -> use the
                                     ; hash-backed slow path instead of silently no-op'ing.

    ;; --- Calculate address of element to remove ---
    MOV   R6, R5
    IADD  R6, R4
    ISUB  R6, 1               ; Address = array_ptr + (position - 1)

    ;; --- Save the value to return ---
    MOV   R7, [R6]            ; R7 = value to return

    ;; --- Shift elements left to fill the gap ---
    MOV   R8, R4              ; R8 = position to remove

__table_remove_shift_loop:
    MOV   R9, R8
    ILT   R9, R3
    JF    R9, __table_remove_update_length

    MOV   R9, R5
    IADD  R9, R8              ; element R8+1 (0-based R8) ...

    MOV   R10, R5
    IADD  R10, R8
    ISUB  R10, 1              ; ... moves down to element R8

    MOV   R11, [R9]
    MOV   [R10], R11

    IADD  R8, 1
    JMP   __table_remove_shift_loop

__table_remove_update_length:
    ;; clear the vacated last slot, then shrink the border
    MOV   R9, R5
    IADD  R9, R3
    ISUB  R9, 1
    MOV   R10, BOXED_NIL
    MOV   [R9], R10
    ISUB  R3, 1
    MOV   [R1+1], R3

    MOV   R0, R7
    JMP   __table_remove_done

;; ---------------------------------------------------------------------------
;; HASH PATH: element lives in the hash bucket, not the (currently always-
;; empty) contiguous array. Shift down via table_get/table_set instead of
;; touching array memory directly. NOTE: table_get only callee-saves R1-R7,
;; so every value that must survive a sub-CALL here is kept in R1-R7 -- R8+
;; is NOT safe to hold state in across these calls.
;; ---------------------------------------------------------------------------
__table_remove_hash_path:
    ;; value_to_return = get(table, position)
    MOV   R5, R4
    CIF   R5                   ; R5 = float(position)
    MOV   R6, R1
    OR    R6, BOXED_TABLE      ; R6 = re-tagged table pointer
    PUSH  R6
    PUSH  R5
    CALL  __builtin_table_get
    IADD  SP, 2
    MOV   R7, R0               ; R7 = value to return (preserved: within R1-R7)

    MOV   R2, R4                ; R2 = shift index i, starting at position
__table_remove_hash_shift_loop:
    MOV   R5, R2
    ILT   R5, R3                ; i < length ?
    JF    R5, __table_remove_hash_shift_done

    ;; tmp = get(table, i+1)
    MOV   R5, R2
    IADD  R5, 1
    CIF   R5
    MOV   R6, R1
    OR    R6, BOXED_TABLE
    PUSH  R6
    PUSH  R5
    CALL  __builtin_table_get
    IADD  SP, 2
    MOV   R5, R0                ; R5 = fetched value

    ;; set(table, i, tmp)
    MOV   R6, R1
    OR    R6, BOXED_TABLE
    MOV   R0, R2
    CIF   R0                    ; R0 = float(i)
    PUSH  R6
    PUSH  R0
    PUSH  R5
    CALL  __builtin_table_set
    IADD  SP, 3

    IADD  R2, 1
    JMP   __table_remove_hash_shift_loop

__table_remove_hash_shift_done:
    ;; --- FIX: clear the now-vacated slot ---
    ;; The shift loop above only ever OVERWRITES each slot with the NEXT
    ;; slot's value -- it never actually deletes anything -- so the very
    ;; last slot (at the OLD length's key, still held in R3 here) keeps a
    ;; live, non-nil duplicate forever. Any subsequent `t[k]` read past
    ;; the new (shrunk) length incorrectly still finds that duplicate
    ;; instead of nil, since the hash-fallback read path scans for the
    ;; key directly and never consults the tracked length at all. This is
    ;; exactly what makes the extremely common
    ;; `while t[1] ~= nil do table.remove(t, 1) end` idiom loop forever --
    ;; index 1 never actually becomes nil no matter how many elements are
    ;; removed.
    ;;
    ;; Storing BOXED_NIL at the vacated key matches this runtime's
    ;; established "nil-valued hash entry == absent" convention (see
    ;; __builtin_next's own doc comment). This single table_set call also
    ;; replaces the old manual "ISUB R3,1; MOV [R1+1],R3" entirely:
    ;; __builtin_table_set's own nil-handling logic
    ;; (__builtin_table_set_hash_maybe_shrink) already clamps the tracked
    ;; length down to (key - 1) whenever a nil is stored at a key within
    ;; the current length -- which the key we're storing here (the OLD
    ;; length, unchanged since before this loop) always is. Letting it
    ;; do that bookkeeping itself avoids two separate, easy-to-desync
    ;; length writes.
    MOV   R5, R3
    CIF   R5                    ; R5 = float(old length) -- the vacated key
    MOV   R6, R1
    OR    R6, BOXED_TABLE
    MOV   R0, BOXED_NIL
    PUSH  R6
    PUSH  R5
    PUSH  R0
    CALL  __builtin_table_set   ; also shrinks tracked length to (old_length - 1)
    IADD  SP, 3

    MOV   R0, R7
    JMP   __table_remove_done

__table_remove_not_found:
    MOV   R0, BOXED_NIL
    JMP   __table_remove_done

__table_remove_invalid_position:
    MOV   R0, BOXED_NIL

__table_remove_done:
    ;; --- Callee-Restore ---
    POP   R11
    POP   R10
    POP   R9
    POP   R8
    POP   R7
    POP   R6
    POP   R5
    POP   R4
    POP   R3
    POP   R2
    POP   R1

    MOV   SP, BP
    POP   BP
    RET  

;; ---------------------------------------------------------------------------
;; table.sort(t, [comp]) - Sorts table elements in-place (bubble sort).
;; Ascending by default, or via a custom comp(a, b) function that returns
;; true when a should sort before b.
;;
;; Incoming Stack (pushed t, comp in that order):
;;   [BP+3] = t (boxed table pointer)
;;   [BP+2] = comp (boxed function, or BOXED_NIL for default ascending order)
;; Returns: R0 = BOXED_NIL (table.sort returns nothing in real Lua)
;;
;; REPLACES the previous version, which (a) never used its comp argument
;; at all, and (b) walked the table's "array part" directly via raw
;; pointer arithmetic on header word 2, which is NEVER actually allocated
;; by this runtime (same discovery as the table.move/table.concat
;; rewrites) -- so the old bubble sort's very first check, "is the array
;; pointer null?", was always true for every real table, and it silently
;; did nothing, for anything, ever.
;;
;; This version goes entirely through __builtin_len / __builtin_table_get
;; / __builtin_table_set, and invokes a user comparator via __builtin_exec
;; when provided -- the same mechanism generic-for uses for iterator
;; functions. NOTE: Lua function calls push arguments RIGHT-TO-LEFT
;; (param 1 ends up at [BP+2] inside the callee), the OPPOSITE convention
;; from this runtime's own internal builtins (which push left-to-right) --
;; see the comparator-invocation block below.
;;
;; Default comparator uses __builtin_relcmp -- the same three-way compare
;; the < operator is built on -- which already correctly handles both
;; numbers and strings, unlike the old stub's raw FLT-on-NaN-boxed-values
;; approach (undefined/always-false for any two strings).
;;
;; Incomparable-type pairs (e.g. sorting a table with a mix of numbers and
;; non-numbers) make __builtin_relcmp return its error sentinel, which
;; this treats as "not less than" -- the pair is simply never swapped
;; relative to each other, rather than raising an error. Matches this
;; runtime's existing permissive style elsewhere (no hard error on
;; incomparable relational operands), NOT real Lua's behavior (which
;; raises "attempt to compare" errors). Worth knowing if a test ever
;; deliberately sorts mixed types.
;; ---------------------------------------------------------------------------
    ;@fn __builtin_table_sort clobbers=R0,R5-R13
__builtin_table_sort:
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 7   ; [BP-1]=n  [BP-2]=i  [BP-3]=j  [BP-4]=comp (boxed)
                 ; [BP-5]=t[j+1]  [BP-6]=t[j+2]  [BP-7]=should_swap

    PUSH  R1
    PUSH  R2
    PUSH  R3
    PUSH  R4

    ;; --- Validate table ---
    MOV   R1, [BP+3]
    MOV   R2, R1
    AND   R2, BOXED_DATA
    IEQ   R2, BOXED_TABLE
    JF    R2, __runtime_error_not_table

    MOV   R1, [BP+2]
    MOV   [BP-4], R1          ; save comparator (boxed function, or nil)

    ;; --- n = #t ---
    MOV   R1, [BP+3]
    PUSH  R1
    CALL  __builtin_len
    IADD  SP, 1
    MOV   R1, R0
    CFI   R1
    MOV   [BP-1], R1          ; n

    MOV   R1, [BP-1]
    ILT   R1, 2
    JT    R1, __sort_done      ; fewer than 2 elements -- nothing to sort

    MOV   R1, 0
    MOV   [BP-2], R1           ; i = 0

__sort_outer_loop:
    MOV   R1, [BP-2]
    MOV   R2, [BP-1]
    ISUB  R2, 1
    IGE   R1, R2
    JT    R1, __sort_done      ; i >= n-1?

    MOV   R1, 0
    MOV   [BP-3], R1           ; j = 0

__sort_inner_loop:
    MOV   R1, [BP-3]
    MOV   R2, [BP-1]
    ISUB  R2, 1
    MOV   R3, [BP-2]
    ISUB  R2, R3
    IGE   R1, R2
    JT    R1, __sort_inner_done   ; j >= n-1-i?

    ;; --- Fetch t[j+1] ---
    MOV   R1, [BP-3]
    IADD  R1, 1
    CIF   R1
    MOV   R2, [BP+3]
    PUSH  R2
    PUSH  R1
    CALL  __builtin_table_get
    IADD  SP, 2
    MOV   [BP-5], R0            ; t[j+1]

    ;; --- Fetch t[j+2] ---
    MOV   R1, [BP-3]
    IADD  R1, 2
    CIF   R1
    MOV   R2, [BP+3]
    PUSH  R2
    PUSH  R1
    CALL  __builtin_table_get
    IADD  SP, 2
    MOV   [BP-6], R0            ; t[j+2]

    ;; --- should_swap: does t[j+2] belong before t[j+1]? ---
    MOV   R1, [BP-4]
    IEQ   R1, BOXED_NIL
    JF    R1, __sort_use_comp

    ;; --- Default: __builtin_relcmp(t[j+2], t[j+1]) == -1 ---
    ;; (relcmp pushes left-to-right: Left pushed first, Right pushed last)
    MOV   R1, [BP-6]             ; Left = t[j+2]
    MOV   R2, [BP-5]             ; Right = t[j+1]
    PUSH  R1
    PUSH  R2
    CALL  __builtin_relcmp
    IADD  SP, 2
    IEQ   R0, -1
    MOV   [BP-7], R0
    JMP   __sort_check_swap

__sort_use_comp:
    ;; --- Custom: comp(t[j+2], t[j+1]) -- Lua calls push RIGHT-TO-LEFT, ---
    ;; --- so param 2 (b) is pushed first, param 1 (a) pushed last. ---
    MOV   R1, [BP-6]              ; a = t[j+2] (param 1)
    MOV   R2, [BP-5]              ; b = t[j+1] (param 2)
    PUSH  R2                      ; param 2 pushed first
    PUSH  R1                      ; param 1 pushed last
    MOV   R0, [BP-4]
    MOV   R13, 2                  ; argument count (variadic ABI)
    CALL  __builtin_exec
    IADD  SP, 2
    IEQ   R0, BOXED_TRUE
    MOV   [BP-7], R0

__sort_check_swap:
    MOV   R1, [BP-7]
    IEQ   R1, 0
    JT    R1, __sort_no_swap

    ;; --- Swap: t[j+1] = old t[j+2], t[j+2] = old t[j+1] ---
    MOV   R1, [BP-3]
    IADD  R1, 1
    CIF   R1
    MOV   R2, [BP+3]
    MOV   R3, [BP-6]
    PUSH  R2
    PUSH  R1
    PUSH  R3
    CALL  __builtin_table_set
    IADD  SP, 3

    MOV   R1, [BP-3]
    IADD  R1, 2
    CIF   R1
    MOV   R2, [BP+3]
    MOV   R3, [BP-5]
    PUSH  R2
    PUSH  R1
    PUSH  R3
    CALL  __builtin_table_set
    IADD  SP, 3

__sort_no_swap:
    MOV   R1, [BP-3]
    IADD  R1, 1
    MOV   [BP-3], R1
    JMP   __sort_inner_loop

__sort_inner_done:
    MOV   R1, [BP-2]
    IADD  R1, 1
    MOV   [BP-2], R1
    JMP   __sort_outer_loop

__sort_done:
    MOV   R0, BOXED_NIL

    POP   R4
    POP   R3
    POP   R2
    POP   R1

    MOV   SP, BP
    POP   BP
    RET  

;; ---------------------------------------------------------------------------
;; __concat_make_empty_string: allocate a fresh zero-length RAM string.
;; Output: R0 = boxed empty string. Clobbers: R0, R1, R2 only.
;; ---------------------------------------------------------------------------
    ;@fn __concat_make_empty_string clobbers=R0-R2
__concat_make_empty_string:
    MOV   R1, [HEAP_POINTER]   ; R1 = base address for this new string
    MOV   R2, 0
    MOV   [R1], R2             ; write null terminator
    MOV   R0, R1
    IADD  R0, 1
    MOV   [HEAP_POINTER], R0   ; bump heap pointer past the 1 byte used
    MOV   R0, R1
    OR    R0, BOXED_RAMSTRING  ; box the STRING BASE (not the bumped pointer)
    RET  

;; ---------------------------------------------------------------------------
;; table.concat(t, [sep], [i], [j]) - Concatenates array elements into a string
;;
;; Incoming Stack (pushed t, sep, i, j in that order):
;;   [BP+5] = t (boxed table pointer)
;;   [BP+4] = sep (boxed string, or BOXED_NIL for "no separator")
;;   [BP+3] = i (boxed float, or BOXED_NIL for "default 1")
;;   [BP+2] = j (boxed float, or BOXED_NIL for "default #t")
;; Returns: R0 = boxed result string
;;
;; REPLACES the previous stub, which ignored the separator entirely,
;; returned the first element raw/unstringified instead of concatenating,
;; and returned BOXED_NIL instead of "" for an empty range.
;;
;; STACK-OFFSET NOTE: the previous version of this routine documented and
;; read from [BP+6]..[BP+3], one slot higher across the board than every
;; other proven N-argument builtin in this file ([BP+(N+1)]..[BP+2] --
;; see table_get, table_insert). That looks like an authoring bug in the
;; abandoned stub. This rewrite uses the established convention instead:
;; [BP+5]..[BP+2] for 4 arguments, matching emit_table_concat_intrinsic's
;; push order (t, sep, i, j).
;; ---------------------------------------------------------------------------
    ;@fn __builtin_table_concat clobbers=R0,R9-R13
__builtin_table_concat:
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 6   ; [BP-1]=i  [BP-2]=j  [BP-3]=accumulator  [BP-4]=loop idx
                 ; [BP-5]=resolved sep  [BP-6]=first-element flag

    PUSH  R1
    PUSH  R2
    PUSH  R3
    PUSH  R4
    PUSH  R5
    PUSH  R6
    PUSH  R7
    PUSH  R8

    ;; --- Validate table type up front ---
    MOV   R1, [BP+5]
    MOV   R2, R1
    AND   R2, BOXED_DATA
    IEQ   R2, BOXED_TABLE
    JF    R2, __runtime_error_not_table

    ;; --- Resolve j: default to #t if nil ---
    MOV   R1, [BP+2]
    MOV   R2, R1
    IEQ   R2, BOXED_NIL
    JF    R2, __concat_j_given
    MOV   R3, [BP+5]        ; table pointer (still boxed -- __builtin_len wants it boxed)
    PUSH  R3
    CALL  __builtin_len      ; R0 = float length
    IADD  SP, 1
    MOV   R1, R0
__concat_j_given:
    CFI   R1
    MOV   [BP-2], R1

    ;; --- Resolve i: default to 1 if nil ---
    MOV   R1, [BP+3]
    MOV   R2, R1
    IEQ   R2, BOXED_NIL
    JF    R2, __concat_i_given
    MOV   R1, 1.0
__concat_i_given:
    CFI   R1
    MOV   [BP-1], R1

    ;; --- Resolve sep: default to a fresh empty string if nil ---
    MOV   R1, [BP+4]
    MOV   R2, R1
    IEQ   R2, BOXED_NIL
    JF    R2, __concat_sep_given
    CALL  __concat_make_empty_string
    MOV   R1, R0
__concat_sep_given:
    MOV   [BP-5], R1

    ;; --- Seed accumulator with a fresh empty string, first-flag = true ---
    CALL  __concat_make_empty_string
    MOV   [BP-3], R0
    MOV   R1, 1
    MOV   [BP-6], R1

    MOV   R1, [BP-1]
    MOV   [BP-4], R1      ; loop index = i

__concat_loop:
    MOV   R1, [BP-4]
    MOV   R2, [BP-2]
    IGT   R1, R2
    JT    R1, __concat_done

    ;; --- If not the first element, append the separator first ---
    MOV   R1, [BP-6]
    IEQ   R1, 1
    JT    R1, __concat_skip_sep

    MOV   R3, [BP-3]       ; accumulator
    PUSH  R3
    MOV   R4, [BP-5]       ; separator
    PUSH  R4
    CALL  __builtin_strcat
    IADD  SP, 2
    MOV   [BP-3], R0       ; accumulator = accumulator .. sep

__concat_skip_sep:
    ;; --- Fetch element and append it ---
    MOV   R5, [BP-4]        ; raw int loop index
    MOV   R6, R5
    CIF   R6                ; boxed float key
    MOV   R7, [BP+5]        ; table pointer
    PUSH  R7
    PUSH  R6
    CALL  __builtin_table_get
    IADD  SP, 2
    MOV   R8, R0            ; R8 = element value (any type; strcat coerces)

    MOV   R3, [BP-3]        ; accumulator
    PUSH  R3
    PUSH  R8
    CALL  __builtin_strcat
    IADD  SP, 2
    MOV   [BP-3], R0        ; accumulator = accumulator .. tostring(element)

    ;; --- Clear the first-flag, advance loop index ---
    MOV   R1, 0
    MOV   [BP-6], R1
    MOV   R1, [BP-4]
    IADD  R1, 1
    MOV   [BP-4], R1
    JMP   __concat_loop

__concat_done:
    MOV   R0, [BP-3]         ; R0 = final accumulator string

    POP   R8
    POP   R7
    POP   R6
    POP   R5
    POP   R4
    POP   R3
    POP   R2
    POP   R1

    MOV   SP, BP
    POP   BP
    RET  

;; ---------------------------------------------------------------------------
;; table.move(a1, f, e, t, [a2]) - Copies elements a1[f..e] to a2 starting at t
;; If a2 is nil, a1 is used as destination (move within the same table).
;;
;; Incoming Stack (pushed a1, f, e, t, a2 in that order):
;;   [BP+6] = a1 (source table, boxed)
;;   [BP+5] = f  (start index, boxed float)
;;   [BP+4] = e  (end index, boxed float)
;;   [BP+3] = t  (destination start index, boxed float)
;;   [BP+2] = a2 (destination table, boxed, or BOXED_NIL for "same as a1")
;; Returns: R0 = boxed destination table
;;
;; REPLACES the previous version, which read/wrote the table's "array
;; part" directly via raw pointer arithmetic on header word 2. That array
;; part is NEVER actually allocated by this runtime for any table --
;; __builtin_table_set's own comments confirm array capacity is always 0
;; and every write goes through the hash-bucket fallback instead. The old
;; version's destination-resize path was a TODO stub that silently wrote
;; past a pointer that's always null -- a real heap-corruption risk the
;; moment the array part ever did get allocated.
;;
;; This version goes entirely through __builtin_table_get /
;; __builtin_table_set instead -- the same primitives table.concat and
;; every passing tables/ test already rely on. No fixed capacity to
;; overflow, since the hash-bucket fallback grows dynamically.
;;
;; Overlap safety: copies BACKWARD (highest index first) whenever a1 == a2
;; (by boxed-pointer equality, which also naturally covers "a2 omitted")
;; and t > f; forward otherwise. Matches real Lua's overlap handling.
;; ---------------------------------------------------------------------------
    ;@fn __builtin_table_move clobbers=R0,R9-R13
__builtin_table_move:
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 6   ; [BP-1]=f  [BP-2]=e  [BP-3]=t  [BP-4]=count
                 ; [BP-5]=resolved a2 (boxed)  [BP-6]=loop counter

    PUSH  R1
    PUSH  R2
    PUSH  R3
    PUSH  R4
    PUSH  R5
    PUSH  R6
    PUSH  R7
    PUSH  R8

    ;; --- Validate a1 ---
    MOV   R1, [BP+6]
    MOV   R2, R1
    AND   R2, BOXED_DATA
    IEQ   R2, BOXED_TABLE
    JF    R2, __runtime_error_not_table

    ;; --- Resolve a2: default to a1 if nil ---
    MOV   R1, [BP+2]
    MOV   R2, R1
    IEQ   R2, BOXED_NIL
    JF    R2, __move_a2_given
    MOV   R1, [BP+6]           ; a2 = a1
    JMP   __move_a2_resolved
__move_a2_given:
    MOV   R2, R1
    AND   R2, BOXED_DATA
    IEQ   R2, BOXED_TABLE
    JF    R2, __runtime_error_not_table
__move_a2_resolved:
    MOV   [BP-5], R1            ; resolved a2 (boxed)

    ;; --- Convert f, e, t to raw ints ---
    MOV   R1, [BP+5]
    CFI   R1
    MOV   [BP-1], R1            ; f

    MOV   R1, [BP+4]
    CFI   R1
    MOV   [BP-2], R1            ; e

    MOV   R1, [BP+3]
    CFI   R1
    MOV   [BP-3], R1            ; t

    ;; --- count = e - f + 1 ---
    MOV   R1, [BP-2]
    MOV   R2, [BP-1]
    ISUB  R1, R2
    IADD  R1, 1
    MOV   [BP-4], R1            ; count (may be < 1 -- empty range, no-op)

    MOV   R1, [BP-4]
    ILT   R1, 1
    JT    R1, __move_done       ; count < 1 -> nothing to copy

    ;; --- Choose direction: backward only if a1 == a2 (resolved) AND t > f ---
    MOV   R1, [BP+6]
    MOV   R2, [BP-5]
    IEQ   R1, R2
    JF    R1, __move_forward_init

    MOV   R1, [BP-3]
    MOV   R2, [BP-1]
    IGT   R1, R2
    JT    R1, __move_backward_init
    JMP   __move_forward_init

__move_forward_init:
    MOV   R1, 0
    MOV   [BP-6], R1             ; counter = 0
    JMP   __move_forward_loop

__move_forward_loop:
    MOV   R1, [BP-6]
    MOV   R2, [BP-4]
    IGE   R1, R2                  ; counter >= count?
    JT    R1, __move_done

    MOV   R3, [BP-1]              ; source index = f + counter
    MOV   R4, [BP-6]
    IADD  R3, R4
    CIF   R3

    MOV   R5, [BP+6]
    PUSH  R5
    PUSH  R3
    CALL  __builtin_table_get
    IADD  SP, 2
    MOV   R6, R0                  ; R6 = fetched value

    MOV   R3, [BP-3]              ; dest index = t + counter
    MOV   R4, [BP-6]
    IADD  R3, R4
    CIF   R3

    MOV   R5, [BP-5]
    PUSH  R5
    PUSH  R3
    PUSH  R6
    CALL  __builtin_table_set
    IADD  SP, 3

    MOV   R1, [BP-6]
    IADD  R1, 1
    MOV   [BP-6], R1
    JMP   __move_forward_loop

__move_backward_init:
    MOV   R1, [BP-4]
    ISUB  R1, 1
    MOV   [BP-6], R1              ; counter = count - 1
    JMP   __move_backward_loop

__move_backward_loop:
    MOV   R1, [BP-6]
    ILT   R1, 0                    ; counter < 0?
    JT    R1, __move_done

    MOV   R3, [BP-1]
    MOV   R4, [BP-6]
    IADD  R3, R4
    CIF   R3

    MOV   R5, [BP+6]
    PUSH  R5
    PUSH  R3
    CALL  __builtin_table_get
    IADD  SP, 2
    MOV   R6, R0

    MOV   R3, [BP-3]
    MOV   R4, [BP-6]
    IADD  R3, R4
    CIF   R3

    MOV   R5, [BP-5]
    PUSH  R5
    PUSH  R3
    PUSH  R6
    CALL  __builtin_table_set
    IADD  SP, 3

    MOV   R1, [BP-6]
    ISUB  R1, 1
    MOV   [BP-6], R1
    JMP   __move_backward_loop

__move_done:
    MOV   R0, [BP-5]               ; R0 = resolved a2 (already boxed)

    POP   R8
    POP   R7
    POP   R6
    POP   R5
    POP   R4
    POP   R3
    POP   R2
    POP   R1

    MOV   SP, BP
    POP   BP
    RET  

;; ===========================================================================
;; SECTION: TABLE PACK
;; ===========================================================================

;; ---------------------------------------------------------------------------
;; table.pack(...) - Packs all arguments into a new table with .n field
;;
;; This is implemented in C as it needs variable argument handling
;; Assembly version would need stack manipulation
;;
;; For now, this is a placeholder. The actual implementation should be in C
;; to handle the variable arguments properly.
;;
;; Returns: R0 = new table with .n field
;; ---------------------------------------------------------------------------
__builtin_table_pack:
    PUSH  BP
    MOV   BP, SP

    ;; --- Callee-Save: Preserve working registers ---
    PUSH  R1
    PUSH  R2
    PUSH  R3

    ;; --- Create new table ---
    CALL  __builtin_table_new
    MOV   R1, R0              ; R1 = new table

    ;; --- TODO: This needs to be implemented in C to access variable arguments ---
    ;; For now, return empty table

    ;; --- Callee-Restore ---
    POP   R3
    POP   R2
    POP   R1

    MOV   SP, BP
    POP   BP
    RET  

;; ===========================================================================
;; SECTION: TABLE UNPACK
;; ===========================================================================

;; ---------------------------------------------------------------------------
;; table.unpack(t, [i], [j]) - Returns elements from table as multiple values
;;
;; Incoming Stack: [BP+4] = Tagged Table Pointer
;;                 [BP+3] = Start index (optional, may be NIL)
;;                 [BP+2] = End index (optional, may be NIL)
;; Returns: Multiple values on stack (Lua convention)
;; Register Usage: R1-R8
;; ---------------------------------------------------------------------------
__builtin_table_unpack:
    PUSH  BP
    MOV   BP, SP

    ;; --- Callee-Save: Preserve 8 working registers ---
    PUSH  R1
    PUSH  R2
    PUSH  R3
    PUSH  R4
    PUSH  R5
    PUSH  R6
    PUSH  R7
    PUSH  R8

    ;; --- Load arguments ---
    MOV   R1, [BP+4]          ; R1 = Tagged Table Pointer
    MOV   R2, [BP+3]          ; R2 = Start index (may be NIL)
    MOV   R3, [BP+2]          ; R3 = End index (may be NIL)

    ;; --- Validate table ---
    MOV   R4, R1
    AND   R4, BOXED_DATA
    IEQ   R4, BOXED_TABLE
    JF    R4, __runtime_error_not_table

    ;; --- Unbox table pointer ---
    AND   R1, BOXED_PAYLOAD   ; R1 = raw table header address

    ;; --- Get array info ---
    MOV   R4, [R1+1]          ; R4 = array length
    MOV   R5, [R1+2]          ; R5 = array data pointer

    ;; --- Set default start index = 1 ---
    MOV   R6, R2
    IEQ   R6, BOXED_NIL
    JT    R6, __table_unpack_start_default

    CFI   R6
    JMP   __table_unpack_start_set

__table_unpack_start_default:
    MOV   R6, 1

__table_unpack_start_set:
    ;; --- Set default end index = length ---
    MOV   R7, R3
    IEQ   R7, BOXED_NIL
    JT    R7, __table_unpack_end_default

    CFI   R7
    JMP   __table_unpack_end_set

__table_unpack_end_default:
    MOV   R7, R4

__table_unpack_end_set:
    ;; --- Validate indices (use R8 for comparison results) ---
    MOV   R8, R6
    ILT   R8, 1
    JT    R8, __table_unpack_invalid_range
    MOV   R8, R4
    IGT   R8, R6
    JT    R8, __table_unpack_invalid_range
    MOV   R8, R7
    ILT   R8, R6
    JT    R8, __table_unpack_invalid_range
    MOV   R8, R4
    IGT   R8, R7
    JT    R8, __table_unpack_end_adjust
    JMP   __table_unpack_validate_done

__table_unpack_end_adjust:
    MOV   R7, R4

__table_unpack_validate_done:
    ;; --- Calculate number of elements to return ---
    MOV   R8, R7
    ISUB  R8, R6
    IADD  R8, 1              ; R8 = number of elements to return

    ;; --- For now, return first element (simplified) ---
    ;; TODO: Implement proper multiple return values
    MOV   R9, R5
    IADD  R9, R6
    ISUB  R9, 1
    MOV   R0, [R9]
    JMP   __table_unpack_done

__table_unpack_invalid_range:
    MOV   R0, BOXED_NIL

__table_unpack_done:
    ;; --- Callee-Restore ---
    POP   R8
    POP   R7
    POP   R6
    POP   R5
    POP   R4
    POP   R3
    POP   R2
    POP   R1

    MOV   SP, BP
    POP   BP
    RET  

;; ===========================================================================
;; SECTION: UTILITY FUNCTIONS
;; ===========================================================================

;; ---------------------------------------------------------------------------
;; Helper: Validate and unbox table pointer
;; Input: R0 = Tagged table pointer
;; Output: R0 = Raw table pointer, or jump to error (R1 = original tagged
;;         value, for __runtime_error_not_table's second print line)
;; Clobbers: R1, R2
;; ---------------------------------------------------------------------------
__unbox_table_validated:
    MOV   R2, R0
    AND   R2, BOXED_DATA
    IEQ   R2, BOXED_TABLE
    JT    R2, __unbox_table_ok
    MOV   R1, R0                  ; every OTHER not_table call site already
    JMP   __runtime_error_not_table ; leaves the offending value in R1
__unbox_table_ok:
    AND   R0, BOXED_PAYLOAD
    RET  


;; ===========================================================================
;; METATABLES. A table's header word 4 holds its metatable (a boxed table)
;; or 0. Supported: __index and __newindex (a table or a function),
;; __call (exec.s), __tostring (string.s), __len (memory.s), __metatable
;; (getmetatable). Not the arithmetic / comparison / concatenation events:
;; operators compile to bare float instructions, and checking every one of
;; them for a table would slow all arithmetic down.
;;
;; Everything here sits on MISS paths only: a key the table has is read and
;; written exactly as before; the metatable is consulted when a read finds
;; nothing (not found, a nil array slot, a deleted key) or a write adds a
;; key the table doesn't have -- as in Lua. (The inline fast paths the
;; compiler emits for t.k / t[i] return what the table holds without a
;; call; they only differ from Lua for a key that holds nil after being
;; set to nil inside the array part, which reads nil instead of asking
;; __index.)
;; ===========================================================================

__mm_str_index:
    string "__index"
__mm_str_newindex:
    string "__newindex"
__mm_str_call:
    string "__call"
__mm_str_tostring:
    string "__tostring"
__mm_str_len:
    string "__len"
__mm_str_metatable:
    string "__metatable"

;; __table_rawget_rk (internal): R0 = boxed table, R1 = key -> R0 = t[k]
;; without its metatable. Preserves R1-R5; R6-R13 may change.
    ;@fn __table_rawget_rk clobbers=R0,R6-R13
__table_rawget_rk:
    PUSH  R2
    PUSH  R3
    MOV   R2, R0
    AND   R2, BOXED_PAYLOAD
    MOV   R3, [R2+4]
    PUSH  R3                  ; the metatable, set aside for the raw read
    MOV   R3, 0
    MOV   [R2+4], R3
    PUSH  R2
    PUSH  R0
    PUSH  R1
    CALL  __builtin_table_get
    IADD  SP, 2
    POP   R2
    POP   R3
    MOV   [R2+4], R3
    POP   R3
    POP   R2
    RET  

;; __table_metamethod (internal): R0 = boxed table, R1 = event name (boxed
;; ROM string) -> R0 = the table's metatable's handler, or nil.
;; Preserves R1-R5; R6-R13 may change (see __builtin_table_get).
    ;@fn __table_metamethod clobbers=R0,R6-R13
__table_metamethod:
    PUSH  R2
    MOV   R2, R0
    AND   R2, BOXED_PAYLOAD
    MOV   R0, [R2+4]
    JF    R0, __table_metamethod_none
    CALL  __table_rawget_rk
    POP   R2
    RET  
__table_metamethod_none:
    MOV   R0, BOXED_NIL
    POP   R2
    RET  

;; __table_call_handler (internal): calls the Lua function R0 with R1
;; arguments, pushed by the caller before the CALL (last argument first, so
;; the first one sits right above the return address). R1-R13 are saved
;; and restored around the call (a Lua function may use them all); the
;; caller pops its arguments afterwards. -> R0 = the first result.
__table_call_handler:
    PUSH  BP
    MOV   BP, SP              ; [BP+1] return address, [BP+2 ..] arguments
    PUSH  R1
    PUSH  R2
    PUSH  R3
    PUSH  R4
    PUSH  R5
    PUSH  R6
    PUSH  R7
    PUSH  R8
    PUSH  R9
    PUSH  R10
    PUSH  R11
    PUSH  R12
    PUSH  R13
    MOV   R3, R1              ; push them again above the saved registers
__table_call_handler_copy:
    JF    R3, __table_call_handler_go
    ISUB  R3, 1
    MOV   R4, BP
    IADD  R4, 2
    IADD  R4, R3
    MOV   R4, [R4]
    PUSH  R4
    JMP   __table_call_handler_copy
__table_call_handler_go:
    MOV   R13, R1             ; argument count (variadic ABI)
    CALL  __builtin_exec
    MOV   SP, BP
    ISUB  SP, 13
    POP   R13
    POP   R12
    POP   R11
    POP   R10
    POP   R9
    POP   R8
    POP   R7
    POP   R6
    POP   R5
    POP   R4
    POP   R3
    POP   R2
    POP   R1
    POP   BP
    RET  

;; __table_index_miss (internal): R0 = boxed table, R1 = key, absent from
;; it -> R0 = nil, or what the metatable's __index gives: __index is a
;; table -> that table's [key] (its own metatable included, so chains
;; work), a function -> __index(t, key). Preserves R1-R3; R4-R13 may change
;; (the lookups it makes go through __builtin_table_get).
    ;@fn __table_index_miss clobbers=R0,R4-R13
__table_index_miss:
    PUSH  R2
    PUSH  R3
    MOV   R2, R0
    AND   R2, BOXED_PAYLOAD
    MOV   R2, [R2+4]
    JF    R2, __table_index_miss_nil      ; no metatable: nil (the usual case)
    MOV   R3, R0                          ; R3 = the table
    PUSH  R1
    MOV   R1, __mm_str_index
    OR    R1, BOXED_ROMSTRING
    CALL  __table_metamethod              ; R0 = __index
    POP   R1
    MOV   R2, R0
    IEQ   R2, BOXED_NIL
    JT    R2, __table_index_miss_done
    MOV   R2, R0
    AND   R2, BOXED_DATA
    IEQ   R2, BOXED_TABLE
    JF    R2, __table_index_miss_fn
    PUSH  R0
    PUSH  R1
    CALL  __builtin_table_get             ; __index[key]
    IADD  SP, 2
    JMP   __table_index_miss_done
__table_index_miss_fn:
    PUSH  R1                              ; (saved)
    PUSH  R1                              ; __index(t, key): key ...
    PUSH  R3                              ; ... then t, the first argument
    MOV   R1, 2
    CALL  __table_call_handler
    IADD  SP, 2
    POP   R1
    JMP   __table_index_miss_done
__table_index_miss_nil:
    MOV   R0, BOXED_NIL
__table_index_miss_done:
    POP   R3
    POP   R2
    RET  

;; __table_newindex (internal): R0 = boxed table, R2 = key, R3 = value, the
;; table having a metatable -> R0 = 1 if its __newindex took the store (the
;; key is absent from the table and __newindex is set: a table -> stored
;; there, with its own metatable; a function -> __newindex(t, key, value)),
;; 0 for an ordinary store. Preserves R1, R4; R2-R3, R5-R13 may change.
    ;@fn __table_newindex clobbers=R0,R2-R3,R5-R13
__table_newindex:
    PUSH  R1
    PUSH  R4
    MOV   R4, R0                          ; R4 = the table
    MOV   R1, R2
    CALL  __table_rawget_rk
    MOV   R1, R0
    IEQ   R1, BOXED_NIL
    JF    R1, __table_newindex_no         ; present: an ordinary store
    MOV   R0, R4
    MOV   R1, __mm_str_newindex
    OR    R1, BOXED_ROMSTRING
    CALL  __table_metamethod
    MOV   R1, R0
    IEQ   R1, BOXED_NIL
    JT    R1, __table_newindex_no
    MOV   R1, R0
    AND   R1, BOXED_DATA
    IEQ   R1, BOXED_TABLE
    JF    R1, __table_newindex_fn
    PUSH  R0                              ; __newindex[key] = value
    PUSH  R2
    PUSH  R3
    CALL  __builtin_table_set
    IADD  SP, 3
    JMP   __table_newindex_yes
__table_newindex_fn:
    PUSH  R3                              ; __newindex(t, key, value)
    PUSH  R2
    PUSH  R4
    MOV   R1, 3
    CALL  __table_call_handler
    IADD  SP, 3
__table_newindex_yes:
    MOV   R0, 1
    POP   R4
    POP   R1
    RET  
__table_newindex_no:
    MOV   R0, 0
    POP   R4
    POP   R1
    RET  

;; __table_unary_meta (internal): R0 = a value, R1 = event name -> if R0 is
;; a table whose metatable has a function for the event: R0 = handler(t),
;; R1 = 1; else R1 = 0 and R0 unchanged. Preserves R2; R3-R13 may change.
    ;@fn __table_unary_meta clobbers=R0-R1,R3-R13
__table_unary_meta:
    PUSH  R2
    MOV   R2, R0
    AND   R2, BOXED_DATA
    IEQ   R2, BOXED_TABLE
    JF    R2, __table_unary_meta_no
    MOV   R2, R0
    AND   R2, BOXED_PAYLOAD
    MOV   R2, [R2+4]
    JF    R2, __table_unary_meta_no
    MOV   R2, R0                          ; the table
    CALL  __table_metamethod
    MOV   R1, R0
    AND   R1, BOXED_DATA
    IEQ   R1, BOXED_FUNCTION
    JF    R1, __table_unary_meta_restore  ; (a closure has the same tag)
    PUSH  R2
    MOV   R1, 1
    CALL  __table_call_handler
    IADD  SP, 1
    MOV   R1, 1
    POP   R2
    RET  
__table_unary_meta_restore:
    MOV   R0, R2
__table_unary_meta_no:
    MOV   R1, 0
    POP   R2
    RET  

;; ---------------------------------------------------------------------------
;; setmetatable(t, mt) -> t    getmetatable(v) -> mt (or its __metatable)
;; rawget(t, k)  rawset(t, k, v) -> t  rawlen(v)  rawequal(a, b)
;; Arguments pushed in order: the last one at [BP+2]. Preserve R1-R13.
;; ---------------------------------------------------------------------------
__builtin_setmetatable: ; [BP+3] = t, [BP+2] = mt
    PUSH  BP
    MOV   BP, SP
    PUSH  R1
    PUSH  R2
    MOV   R0, [BP+3]
    MOV   R1, R0
    AND   R1, BOXED_DATA
    IEQ   R1, BOXED_TABLE
    JF    R1, __builtin_setmetatable_done ; not a table: nothing to set
    MOV   R1, R0
    AND   R1, BOXED_PAYLOAD
    MOV   R2, [BP+2]
    AND   R2, BOXED_DATA
    IEQ   R2, BOXED_TABLE
    JF    R2, __builtin_setmetatable_set  ; nil (or anything else): removed
    MOV   R2, [BP+2]
__builtin_setmetatable_set:
    MOV   [R1+4], R2
__builtin_setmetatable_done:
    POP   R2
    POP   R1
    MOV   SP, BP
    POP   BP
    RET  

__builtin_getmetatable: ; [BP+2] = v
    PUSH  BP
    MOV   BP, SP
    PUSH  R1
    MOV   R0, [BP+2]
    MOV   R1, R0
    AND   R1, BOXED_DATA
    IEQ   R1, BOXED_TABLE
    JF    R1, __builtin_getmetatable_nil
    MOV   R1, R0
    AND   R1, BOXED_PAYLOAD
    MOV   R0, [R1+4]
    JF    R0, __builtin_getmetatable_nil
    PUSH  R2
    MOV   R2, R0                          ; a __metatable field answers instead
    MOV   R1, __mm_str_metatable
    OR    R1, BOXED_ROMSTRING
    CALL  __table_rawget_rk
    MOV   R1, R0
    IEQ   R1, BOXED_NIL
    JF    R1, __builtin_getmetatable_mf   ; R0 = __metatable
    MOV   R0, R2
__builtin_getmetatable_mf:
    POP   R2
    JMP   __builtin_getmetatable_done
__builtin_getmetatable_nil:
    MOV   R0, BOXED_NIL
__builtin_getmetatable_done:
    POP   R1
    MOV   SP, BP
    POP   BP
    RET  

__builtin_rawget: ; [BP+3] = t, [BP+2] = k
    PUSH  BP
    MOV   BP, SP
    PUSH  R1
    MOV   R0, [BP+3]
    MOV   R1, R0
    AND   R1, BOXED_DATA
    IEQ   R1, BOXED_TABLE
    JF    R1, __builtin_rawget_nil
    MOV   R1, [BP+2]
    CALL  __table_rawget_rk
    JMP   __builtin_rawget_done
__builtin_rawget_nil:
    MOV   R0, BOXED_NIL
__builtin_rawget_done:
    POP   R1
    MOV   SP, BP
    POP   BP
    RET  

__builtin_rawset: ; [BP+4] = t, [BP+3] = k, [BP+2] = v
    PUSH  BP
    MOV   BP, SP
    PUSH  R1
    PUSH  R2
    MOV   R0, [BP+4]
    MOV   R1, R0
    AND   R1, BOXED_DATA
    IEQ   R1, BOXED_TABLE
    JF    R1, __builtin_rawset_done
    MOV   R1, R0
    AND   R1, BOXED_PAYLOAD
    MOV   R2, [R1+4]
    PUSH  R2                              ; metatable set aside
    MOV   R2, 0
    MOV   [R1+4], R2
    PUSH  R0
    MOV   R2, [BP+3]
    PUSH  R2
    MOV   R2, [BP+2]
    PUSH  R2
    CALL  __builtin_table_set
    IADD  SP, 3
    POP   R2
    MOV   [R1+4], R2
    MOV   R0, [BP+4]
__builtin_rawset_done:
    POP   R2
    POP   R1
    MOV   SP, BP
    POP   BP
    RET  

__builtin_rawlen: ; [BP+2] = v
    PUSH  BP
    MOV   BP, SP
    PUSH  R1
    PUSH  R2
    MOV   R0, [BP+2]
    MOV   R1, R0
    AND   R1, BOXED_DATA
    IEQ   R1, BOXED_TABLE
    JF    R1, __builtin_rawlen_plain
    PUSH  R0
    CALL  __builtin_table_len             ; the border, no __len
    IADD  SP, 1
    JMP   __builtin_rawlen_done
__builtin_rawlen_plain:
    PUSH  R0
    CALL  __builtin_len
    IADD  SP, 1
__builtin_rawlen_done:
    POP   R2
    POP   R1
    MOV   SP, BP
    POP   BP
    RET  

;; ===========================================================================
;; SECTION: MULTI-VALUE LISTS (node/multivalue.c)
;; ===========================================================================
;; MV_BUF holds value k of the current multi-value list -- a function's
;; return values, a spread `...`, unpack() -- and [RET_COUNT] says how many
;; (at most MV_MAX; more are dropped). A compiled function returns values
;; 0-2 in R0/R2/R3 and the rest in MV_BUF[3..]; the compiler copies R0/R2/R3
;; into MV_BUF[0..2] when it needs the whole list in memory.

;; __mv_from_mem: R1 = address of the first value, R2 = count (int, may be
;; <= 0). Copies the values into MV_BUF and sets RET_COUNT. Clobbers R0-R3.
    ;@fn __mv_from_mem clobbers=R0-R3
__mv_from_mem:
    MOV   R0, R2
    ILT   R0, 0
    JF    R0, __mv_from_mem_pos
    MOV   R2, 0
__mv_from_mem_pos:
    IMIN  R2, MV_MAX
    MOV   [RET_COUNT], R2
    MOV   R3, MV_BUF
__mv_from_mem_loop:
    JF    R2, __mv_from_mem_done
    MOV   R0, [R1]
    MOV   [R3], R0
    IADD  R1, 1
    IADD  R3, 1
    ISUB  R2, 1
    JMP   __mv_from_mem_loop
__mv_from_mem_done:
    RET  

;; __mv_shift: R1 = k >= 1. Moves the list up by k slots (MV_BUF[i + k] =
;; MV_BUF[i]) so k leading values can be stored in front of it, and adds k
;; to RET_COUNT (capped at MV_MAX). Clobbers R0, R2, R3.
    ;@fn __mv_shift clobbers=R0,R2-R3
__mv_shift:
    MOV   R2, [RET_COUNT]
    IADD  R2, R1
    IMIN  R2, MV_MAX
    MOV   [RET_COUNT], R2         ; R2 = one past the destination slot
__mv_shift_loop:
    MOV   R0, R2
    IGT   R0, R1                  ; destination R2 - 1 >= k?
    JF    R0, __mv_shift_done
    ISUB  R2, 1
    MOV   R3, R2
    ISUB  R3, R1
    IADD  R3, MV_BUF
    MOV   R0, [R3]
    MOV   R3, R2
    IADD  R3, MV_BUF
    MOV   [R3], R0
    JMP   __mv_shift_loop
__mv_shift_done:
    RET  

;; __mv_unpack(t, i, j): [BP+4] = t, [BP+3] = i, [BP+2] = j (nil: 1 and #t).
;; t[i], ..., t[j] into MV_BUF, RET_COUNT = their number. Preserves R1-R5.
    ;@fn __mv_unpack clobbers=R0,R6-R13
__mv_unpack:
    PUSH  BP
    MOV   BP, SP
    PUSH  R1
    PUSH  R2
    PUSH  R3
    PUSH  R4
    PUSH  R5
    MOV   R1, [BP+4]
    MOV   R2, R1
    AND   R2, BOXED_DATA
    IEQ   R2, BOXED_TABLE
    JF    R2, __runtime_error_not_table
    AND   R1, BOXED_PAYLOAD       ; raw header
    MOV   R3, [BP+3]              ; i
    MOV   R2, R3
    IEQ   R2, BOXED_NIL
    JF    R2, __mv_unpack_have_i
    MOV   R3, 1.0
__mv_unpack_have_i:
    FLR   R3
    CFI   R3
    MOV   R4, [BP+2]              ; j
    MOV   R2, R4
    IEQ   R2, BOXED_NIL
    JF    R2, __mv_unpack_have_j
    MOV   R4, [R1+1]              ; #t
    JMP   __mv_unpack_count
__mv_unpack_have_j:
    FLR   R4
    CFI   R4
__mv_unpack_count:
    ISUB  R4, R3
    IADD  R4, 1                   ; count = j - i + 1
    MOV   R2, R4
    ILT   R2, 0
    JF    R2, __mv_unpack_pos
    MOV   R4, 0
__mv_unpack_pos:
    IMIN  R4, MV_MAX
    MOV   [RET_COUNT], R4
    MOV   R5, MV_BUF
__mv_unpack_loop:
    JF    R4, __mv_unpack_done
    CALL  __table_rawget_int      ; R0 = t[R3]
    MOV   [R5], R0
    IADD  R3, 1
    IADD  R5, 1
    ISUB  R4, 1
    JMP   __mv_unpack_loop
__mv_unpack_done:
    POP   R5
    POP   R4
    POP   R3
    POP   R2
    POP   R1
    MOV   SP, BP
    POP   BP
    RET  

;; __mv_to_table(t, base): [BP+3] = t, [BP+2] = first key (raw int).
;; t[base + k] = MV_BUF[k] for every value of the list. Preserves R1-R4.
    ;@fn __mv_to_table clobbers=R0,R9-R13
__mv_to_table:
    PUSH  BP
    MOV   BP, SP
    PUSH  R1
    PUSH  R2
    PUSH  R3
    PUSH  R4
    MOV   R3, [RET_COUNT]
    MOV   R2, 0
__mv_to_table_loop:
    MOV   R1, R2
    ILT   R1, R3
    JF    R1, __mv_to_table_done
    MOV   R1, [BP+3]
    PUSH  R1                      ; table
    MOV   R1, [BP+2]
    IADD  R1, R2
    CIF   R1
    PUSH  R1                      ; key
    MOV   R1, R2
    IADD  R1, MV_BUF
    MOV   R1, [R1]
    PUSH  R1                      ; value
    CALL  __builtin_table_set
    IADD  SP, 3
    IADD  R2, 1
    JMP   __mv_to_table_loop
__mv_to_table_done:
    POP   R4
    POP   R3
    POP   R2
    POP   R1
    MOV   SP, BP
    POP   BP
    RET  
;; ===========================================================================
;; SECTION: STRING OPERATIONS
;; ===========================================================================

;; ===========================================================================
;; Internal Helper: __unbox_string
;; Input:  R0 = NaN-boxed String (RAM or ROM)
;; Output: R0 = Raw Vircon32 Memory Address (Page bit applied if in ROM)
;; Clobbers: R1
;; ===========================================================================
    ;@fn __unbox_string clobbers=R0-R1
__unbox_string:
    MOV   R1, R0
    AND   R1, BOXED_CATEGORY      ; Check Bit 31 (1 = RAM, 0 = ROM) String
    AND   R0, BOXED_PAYLOAD       ; Strip the entire NaN tag (22-bit offset)

    INE   R1, 0                   ; If Bit 31 is non-zero, it is a RAM string
    JT    R1, __unbox_string_end  ; Jump to end (RAM starts at 0x00000000)

    ;; It is a ROM string: Apply Vircon32 Cartridge Page Bit (Bit 29)
    OR    R0, V32_CART_PAGE

__unbox_string_end:
    RET  

;; ---------------------------------------------------------------------------
;; Built-in: Tag-Aware String Concatenation with Auto-Coercion
;; Incoming Stack: [BP+3] = Left_Val (any Lua type), [BP+2] = Right_Val (any Lua type)
;; Returns: R0 = Tagged pointer to newly allocated RAM heap string (0xFFC..)
;; Clobbers: R0-R8
;; ---------------------------------------------------------------------------
__builtin_strcat:
    PUSH  BP
    MOV   BP, SP

    ;; === Type-check and coerce LEFT operand if needed ===
    MOV   R1, [BP+3]          ; Load Left value
    MOV   R3, R1
    AND   R3, BOXED_DATA      ; Extract tag
    IEQ   R3, BOXED_ROMSTRING
    JT    R3, __strcat_left_is_string
    IEQ   R3, BOXED_RAMSTRING
    JF    R3, __strcat_left_coerce

    ;; FIXED: tag matched RAMSTRING -- confirm payload >= 4 before
    ;; accepting it as a real string. nil/true/false fall through to the
    ;; coercion path below, which is where they belong.
    MOV   R3, R1
    AND   R3, BOXED_PAYLOAD
    ILT   R3, 4
    JT    R3, __strcat_left_coerce
    JMP   __strcat_left_is_string

__strcat_left_coerce:
    ;; Not a string - coerce it via the "A" scratch-buffer tostring
    ;; variant (was __builtin_tostring). (comment and code below this
    ;; point unchanged from the original -- this label is new, added
    ;; only so both the "tag never matched" and "tag matched but payload
    ;; < 4" cases can reach the same coercion code.)
    PUSH  R1
    CALL  __builtin_tostring_scratch_a  ; Returns string in R0
    IADD  SP, 1
    MOV   R1, R0              ; R1 = string version
    MOV   [BP+3], R1          ; Replace left operand on stack

__strcat_left_is_string:

    ;; === Type-check and coerce RIGHT operand if needed ===
    MOV   R1, [BP+2]          ; Load Right value
    MOV   R3, R1
    AND   R3, BOXED_DATA      ; Extract tag
    IEQ   R3, BOXED_ROMSTRING
    JT    R3, __strcat_right_is_string
    IEQ   R3, BOXED_RAMSTRING
    JF    R3, __strcat_right_coerce

    ;; FIXED: same payload check for the right operand.
    MOV   R3, R1
    AND   R3, BOXED_PAYLOAD
    ILT   R3, 4
    JT    R3, __strcat_right_coerce
    JMP   __strcat_right_is_string

__strcat_right_coerce:
    ;; Not a string - coerce it via the "B" scratch-buffer tostring
    ;; variant. (comment and code below this point unchanged -- see the
    ;; original's note on why RIGHT specifically uses the independent "B"
    ;; buffer.)
    PUSH  R1
    CALL  __builtin_tostring_scratch_b  ; Returns string in R0
    IADD  SP, 1
    MOV   R1, R0              ; R1 = string version
    MOV   [BP+2], R1          ; Replace right operand on stack

__strcat_right_is_string:

    ;; === Original string concatenation logic continues, unchanged ===
    MOV   R0, [BP+3]          ; Load Left tagged pointer
    CALL  __unbox_string      ; R0 = raw hardware address (ROM or RAM)
    MOV   R7, R0              ; Cache unboxed Left pointer in R7
    MOV   R1, R7              ; R1 = Reading pointer
    MOV   R2, 0               ; R2 = Left length counter

__strcat_len_left:
    MOV   R3, [R1]            ; Read ASCII character
    IEQ   R3, 0
    JT    R3, __strcat_len_right_check
    IADD  R1, 1
    IADD  R2, 1
    JMP   __strcat_len_left

    ;; --- 2. Unbox and Calculate Length of Right String ---
__strcat_len_right_check:
    MOV   R0, [BP+2]          ; Load Right tagged pointer
    CALL  __unbox_string
    MOV   R8, R0              ; Cache unboxed Right pointer in R8
    MOV   R1, R8
    MOV   R4, 0               ; R4 = Right length counter

__strcat_len_right:
    MOV   R3, [R1]
    IEQ   R3, 0
    JT    R3, __strcat_alloc
    IADD  R1, 1
    IADD  R4, 1
    JMP   __strcat_len_right

    ;; --- 3. Allocate Memory on Heap ---
__strcat_alloc:
    MOV   R0, [HEAP_POINTER]  ; R0 = New string base (raw pointer)
    MOV   R5, R0              ; R5 = Write head
    MOV   R6, R0
    IADD  R6, R2
    IADD  R6, R4
    IADD  R6, 1
    MOV   [HEAP_POINTER], R6

    ;; --- 4. Copy Left String to Heap ---
    MOV   R1, R7
__strcat_copy_left:
    MOV   R3, [R1]
    MOV   R6, R3
    IEQ   R6, 0
    JT    R6, __strcat_copy_right_check
    MOV   [R5], R3
    IADD  R1, 1
    IADD  R5, 1
    JMP   __strcat_copy_left

    ;; --- 5. Copy Right String to Heap ---
__strcat_copy_right_check:
    MOV   R1, R8
__strcat_copy_right:
    MOV   R3, [R1]
    MOV   R6, R3
    IEQ   R6, 0
    JT    R6, __strcat_finish
    MOV   [R5], R3
    IADD  R1, 1
    IADD  R5, 1
    JMP   __strcat_copy_right

    ;; --- 6. Null-Terminate, BOX as RAM String, and Return ---
__strcat_finish:
    MOV   R3, 0
    MOV   [R5], R3            ; Write Null terminator
    OR    R0, BOXED_RAMSTRING ; BOX: R0 = new string base
    MOV   SP, BP
    POP   BP
    RET  

;; ---------------------------------------------------------------------------
;; Universal Equality (==): Returns BOXED_TRUE / BOXED_FALSE in R0.
;; (This header used to say "raw integer 1 / 0" -- it never did; both boxed
;; booleans are non-zero, so a caller testing JT R0 always saw "equal".)
;; Incoming Stack: [BP+3] = Left_Val, [BP+2] = Right_Val
;; Handles: bitwise-identical values (fast path), string comparisons, and
;;         numeric comparisons (raw IEEE 754 floats in NaN-boxing scheme)
;; ---------------------------------------------------------------------------
    ;@fn __builtin_eq clobbers=R0-R4
__builtin_eq:
    PUSH  BP
    MOV   BP, SP

    MOV   R1, [BP+3]
    MOV   R2, [BP+2]

    ;; Fast-path: If bitwise identical, they are strictly equal
    IEQ   R1, R2
    JT    R1, __eq_return_true

    ;; --- Check if both operands are raw numbers (not tagged) ---
    ;; In NaN-boxing: numbers are raw floats, other types have BOXED_DATA tag
    MOV   R1, [BP+3]
    MOV   R3, R1
    AND   R3, NAN_VALUE       ; exponent all ones = NaN-boxed (not a number);
    IEQ   R3, NAN_VALUE       ; (BOXED_DATA caught every float >= 2.0 or < 0)
    JF    R3, __eq_check_right_number

    ;; Left is tagged, so not a number - check if it's a string
    JMP   __eq_check_left_string

__eq_check_right_number:
    MOV   R3, R2
    AND   R3, NAN_VALUE       ; exponent all ones = NaN-boxed (not a number);
    IEQ   R3, NAN_VALUE       ; (BOXED_DATA caught every float >= 2.0 or < 0)
    JF    R3, __eq_both_numbers

    ;; Right is tagged, left is not - different types, not equal
    JMP   __eq_return_false

__eq_both_numbers:
    ;; Both operands are raw floats - use float equality
    FEQ   R1, R2
    JT    R1, __eq_return_true
    JMP   __eq_return_false

;; --- String comparison path ---
__eq_check_left_string:
    MOV   R1, [BP+3]
    MOV   R3, R1
    AND   R3, BOXED_DATA
    IEQ   R3, BOXED_ROMSTRING
    JT    R3, __eq_left_valid

    MOV   R3, R1
    AND   R3, BOXED_DATA
    IEQ   R3, BOXED_RAMSTRING
    JF    R3, __eq_return_false

    ;; FIXED: tag matches RAMSTRING, but nil/true/false/tombstone share
    ;; that tag with payload < 4 -- only payload >= 4 is a real RAM string.
    MOV   R3, R1
    AND   R3, BOXED_PAYLOAD
    ILT   R3, 4
    JT    R3, __eq_return_false

__eq_left_valid:
    ;; Validate RIGHT Operand is a String
    MOV   R3, R2
    AND   R3, BOXED_DATA
    IEQ   R3, BOXED_ROMSTRING
    JT    R3, __eq_right_valid

    MOV   R3, R2
    AND   R3, BOXED_DATA
    IEQ   R3, BOXED_RAMSTRING
    JF    R3, __eq_return_false

    ;; FIXED: same payload check for the right operand.
    MOV   R3, R2
    AND   R3, BOXED_PAYLOAD
    ILT   R3, 4
    JT    R3, __eq_return_false

__eq_right_valid:
    ;; Unbox both validated string pointers!
    MOV   R0, R1
    CALL  __unbox_string
    PUSH  R0                  ; Save unboxed Left pointer

    MOV   R0, R2
    CALL  __unbox_string
    MOV   R2, R0              ; R2 = Unboxed Right pointer
    POP   R1                  ; R1 = Unboxed Left pointer

__eq_strcmp_loop:
    MOV   R3, [R1]
    MOV   R4, [R2]

    ;; Compare current characters
    INE   R3, R4
    JT    R3, __eq_return_false   ; Characters differ -> strings not equal

    ;; Check for null terminator (0x00)
    MOV   R3, [R1]
    IEQ   R3, 0
    JT    R3, __eq_return_true    ; Both hit null terminator: strings equal!

    ;; Advance pointers to next character
    IADD  R1, 1
    IADD  R2, 1
    JMP   __eq_strcmp_loop

__eq_return_true:
    MOV   R0, BOXED_TRUE          ; Return boxed Boolean True
    MOV   SP, BP                  ; Stack Restore
    POP   BP
    RET  

__eq_return_false:
    MOV   R0, BOXED_FALSE         ; Return boxed Boolean False
    MOV   SP, BP                  ; Stack Restore
    POP   BP
    RET  

;; ---------------------------------------------------------------------------
;; Universal Relational Comparison (<, <=, >, >=)
;; Incoming Stack: [BP+3] = Left_Val (boxed), [BP+2] = Right_Val (boxed)
;; Returns: R0 = raw integer -1 (Left < Right), 0 (Left == Right), 1 (Left > Right)
;; Handles: numeric comparison (raw IEEE 754 floats) and lexicographic
;;          string comparison. Traps the CPU on mismatched/uncomparable types
;;          (e.g. number vs string, table vs number).
;; Clobbers: R0-R6
;; ---------------------------------------------------------------------------
    ;@fn __builtin_relcmp clobbers=R0-R5
__builtin_relcmp:
    PUSH  BP
    MOV   BP, SP

    MOV   R1, [BP+3]            ; Left
    MOV   R2, [BP+2]            ; Right

    ;; --- Is Left a number? A boxed/tagged value has ALL exponent bits set
    ;; (the quiet-NaN pattern); a finite float never does. Mask out just the
    ;; exponent field (no sign bit!) and compare against that exact pattern -
    ;; same test used by __builtin_type elsewhere in this runtime.
    MOV   R3, R1
    AND   R3, NAN_VALUE          ; 0x7F800000 - isolate exponent bits only
    IEQ   R3, NAN_VALUE          ; R3 = 1 if Left is NaN-tagged (boxed)
    JT    R3, __relcmp_check_left_string

    ;; --- Left is a number: Right must also be a number ---
    MOV   R3, R2
    AND   R3, NAN_VALUE
    IEQ   R3, NAN_VALUE
    JT    R3, __relcmp_error     ; number vs tagged value -> uncomparable

    ;; --- Both operands are raw floats: compare numerically ---
    MOV   R4, R1                 ; preserve Left (FLT overwrites its dest reg)
    FLT   R1, R2                 ; R1 = (Left < Right) ? 1 : 0
    JT    R1, __relcmp_less

    MOV   R1, R4                 ; restore Left
    FGT   R1, R2                 ; R1 = (Left > Right) ? 1 : 0
    JT    R1, __relcmp_greater

    JMP   __relcmp_equal

    ;; --- Left is tagged: it must be a String for a valid comparison ---
__relcmp_check_left_string:
    MOV   R3, R1
    AND   R3, BOXED_DATA
    IEQ   R3, BOXED_ROMSTRING
    JT    R3, __relcmp_check_right_string
         
    MOV   R3, R1
    AND   R3, BOXED_DATA
    IEQ   R3, BOXED_RAMSTRING
    JF    R3, __relcmp_error
         
    ;; FIXED: payload >= 4 check, same rationale as __builtin_eq above.
    MOV   R3, R1
    AND   R3, BOXED_PAYLOAD
    ILT   R3, 4
    JT    R3, __relcmp_error
         
__relcmp_check_right_string:
    MOV   R3, R2
    AND   R3, BOXED_DATA
    IEQ   R3, BOXED_ROMSTRING
    JT    R3, __relcmp_strings
         
    MOV   R3, R2
    AND   R3, BOXED_DATA
    IEQ   R3, BOXED_RAMSTRING
    JF    R3, __relcmp_error
         
    ;; FIXED: same payload check for the right operand.
    MOV   R3, R2
    AND   R3, BOXED_PAYLOAD
    ILT   R3, 4
    JT    R3, __relcmp_error
         
__relcmp_strings:
    ;; --- Both operands are Strings: unbox and compare lexicographically ---
    MOV   R0, R1
    CALL  __unbox_string
    PUSH  R0                     ; Save unboxed Left pointer

    MOV   R0, R2
    CALL  __unbox_string
    MOV   R2, R0                 ; R2 = unboxed Right pointer
    POP   R1                     ; R1 = unboxed Left pointer

__relcmp_strloop:
    MOV   R3, [R1]                ; Left char
    MOV   R4, [R2]                ; Right char

    MOV   R5, R3                  ; scratch: has Left hit NUL?
    IEQ   R5, 0
    JT    R5, __relcmp_check_right_end

    MOV   R5, R4                  ; scratch: has Right hit NUL?
    IEQ   R5, 0
    JT    R5, __relcmp_greater    ; Left longer than Right -> Left > Right

    MOV   R5, R3                  ; scratch: do characters differ?
    INE   R5, R4
    JT    R5, __relcmp_strdiff

    IADD  R1, 1
    IADD  R2, 1
    JMP   __relcmp_strloop

__relcmp_check_right_end:
    MOV   R5, R4
    IEQ   R5, 0
    JT    R5, __relcmp_equal      ; both hit NUL together -> equal strings
    JMP   __relcmp_less           ; Left shorter than Right -> Left < Right

__relcmp_strdiff:
    MOV   R5, R3
    ILT   R5, R4                  ; R5 = (LeftChar < RightChar) ? 1 : 0
    JT    R5, __relcmp_less
    JMP   __relcmp_greater

__relcmp_less:
    MOV   R0, -1
    MOV   SP, BP
    POP   BP
    RET  

__relcmp_greater:
    MOV   R0, 1
    MOV   SP, BP
    POP   BP
    RET  

__relcmp_equal:
    MOV   R0, 0
    MOV   SP, BP
    POP   BP
    RET  

__relcmp_error:
    ;; Operands are of incompatible/non-comparable types (e.g. nil vs
    ;; number, table vs number, nil vs string). Rather than halting the
    ;; whole VM over one bad comparison, ordering operators on
    ;; non-comparable operands simply evaluate to false -- return a
    ;; sentinel that can never equal -1, 0, or 1 so <, <=, >, and >= all
    ;; consistently come out false at the call site.
    MOV   R0, 2
    MOV   SP, BP
    POP   BP
    RET  

;; ---------------------------------------------------------------------------
;; Length Operator Dispatch (#): Returns length as an IEEE 754 Float in R0 
;; Incoming Stack: [BP+2] = Target Value 
;; ---------------------------------------------------------------------------
;; ===========================================================================
;; Runtime Built-in: __builtin_string_len
;; ABI: Arg 1 at [BP+2] (Stack Parameter), Caller cleans up.
;;      Arg 1 MUST be the BOXED string value -- this routine unboxes it
;;      itself. Callers that pre-unboxed their string and passed the RAW
;;      pointer caused a SECOND unbox here: harmless for ROM strings
;;      (OR-ing the cart page bit back on is idempotent) but a RAM/heap
;;      address got misread as a ROM address, yielding a garbage length
;;      and silently wrong results for any string built at runtime.
;; Returns: R0 = Length of string as a Lua Float.
;; ===========================================================================
    ;@fn __builtin_string_len clobbers=R0-R2
__builtin_string_len:
    PUSH  BP
    MOV   BP, SP

    ;; 1. Fetch argument from caller's stack frame
    ;; [BP+0] is old BP, [BP+1] is Return Address, [BP+2] is Arg 1
    MOV   R0, [BP+2]

    ;; 2. Unbox to get raw hardware address
    ;; (handles RAM vs ROM automatically)
    CALL  __unbox_string

    ;; 3. Calculate string length
    MOV   R1, 0                  ; R1 = Character counter
__string_len_loop:
    MOV   R2, [R0]               ; Read character from address
    IEQ   R2, 0                  ; Is it null terminator?
    JT    R2, __string_len_done
    IADD  R0, 1                  ; Advance pointer
    IADD  R1, 1                  ; Increment counter
    JMP   __string_len_loop

__string_len_done:
    ;; 4. Convert integer count in R1 to a Vircon32 Float in R0
    MOV   R0, R1
    CIF   R0                     ; Cast Integer to Float (Standard Lua number)

    MOV   SP, BP
    POP   BP
    RET  

;; ---------------------------------------------------------------------------
;; Built-in: tostring() -- converts any Lua value to a string.
;;
;; This is the normal, persistent variant: a float argument gets a FRESH,
;; permanent heap allocation via __builtin_ftoa -- correct and necessary
;; here, since tostring()'s return value genuinely becomes a Lua string
;; the program may keep, store, or pass around indefinitely.
;;
;; Where the resulting string is instead immediately consumed (copied
;; elsewhere, or displayed) and never needed again, use
;; __builtin_tostring_scratch_a or _scratch_b below instead.
;;
;; Incoming Stack: [BP+2] = value to convert
;; Returns: R0 = Tagged (boxed) string
;; ---------------------------------------------------------------------------
__builtin_tostring:
    PUSH  BP
    MOV   BP, SP
    MOV   R5, 0                ; mode 0: normal malloc'd, persistent buffer
    JMP   __tostring_shared_body

;; ---------------------------------------------------------------------------
;; Built-in: tostring(), SHARED SCRATCH variant "A".
;;
;; Use instead of __builtin_tostring wherever the resulting string is
;; immediately consumed and never needs to survive as an independent Lua
;; value. Non-float results (nil, true/false, pass-through strings,
;; table/function name constants) are already ROM constants or
;; pass-throughs with no allocation at all, so this only actually changes
;; behavior for the float case -- see __builtin_ftoa_scratch_a for the
;; full rationale. Used by print()'s implicit coercion and
;; __builtin_strcat's LEFT-operand coercion.
;; ---------------------------------------------------------------------------
__builtin_tostring_scratch_a:
    PUSH  BP
    MOV   BP, SP
    MOV   R5, 1                ; mode 1: shared scratch buffer A
    JMP   __tostring_shared_body

;; ---------------------------------------------------------------------------
;; Built-in: tostring(), SHARED SCRATCH variant "B" -- second independent
;; buffer, used only by __builtin_strcat's RIGHT-operand coercion. See
;; __builtin_ftoa_scratch_b for why a second buffer is needed.
;; ---------------------------------------------------------------------------
__builtin_tostring_scratch_b:
    PUSH  BP
    MOV   BP, SP
    MOV   R5, 2                ; mode 2: shared scratch buffer B
    ;; falls straight through into the shared body below

;; ===========================================================================
;; Shared dispatch body -- reached from all three entry points above.
;; R5 holds the mode (0/1/2) selected by whichever entry point was called.
;; None of the primitive/string/table/function checks below touch R1, R3,
;; R4, or R6 in a way that matters across this call, and NONE of them
;; touch R5 at all, so the mode flag survives intact all the way down to
;; the float branch at the bottom, where it's finally read.
;; ===========================================================================
__tostring_shared_body:
    MOV   R1, [BP+2]          ; Load argument

    ;; === PRIMITIVE CHECKS FIRST (exact value matches) ===
    MOV   R6, R1
    IEQ   R6, BOXED_NIL
    JT    R6, __tostring_nil
    MOV   R6, R1
    IEQ   R6, BOXED_FALSE
    JT    R6, __tostring_false
    MOV   R6, R1
    IEQ   R6, BOXED_TRUE
    JT    R6, __tostring_true

    ;; === STRING CHECKS ===
    ;; Pass through ROM Strings unchanged
    MOV   R3, R1
    AND   R3, BOXED_DATA
    IEQ   R3, BOXED_ROMSTRING
    JT    R3, __tostring_passthrough

    ;; RAM strings: MUST have tag=BOXED_RAMSTRING AND payload >= 4
    MOV   R3, R1
    AND   R3, BOXED_DATA
    IEQ   R3, BOXED_RAMSTRING
    JF    R3, __tostring_check_other  ; Not RAM string → check other types

    MOV   R4, R1
    AND   R4, BOXED_PAYLOAD
    ILT   R4, 4
    JT    R4, __tostring_check_other  ; Payload < 4 → not a valid RAM string
    JMP   __tostring_passthrough      ; Valid RAM string → pass through

__tostring_check_other:
    ;; Check for Table
    MOV   R3, R1
    AND   R3, BOXED_DATA
    IEQ   R3, BOXED_TABLE
    JT    R3, __tostring_table

    ;; Check for Function
    MOV   R3, R1
    AND   R3, BOXED_DATA
    IEQ   R3, BOXED_FUNCTION
    JT    R3, __tostring_function

    ;; --- Fall through: it's a float. Dispatch to the correct ftoa ---
    ;; --- variant based on the mode this entry point selected.     ---
    MOV   R6, R5
    IEQ   R6, 1
    JT    R6, __tostring_use_scratch_a
    MOV   R6, R5
    IEQ   R6, 2
    JT    R6, __tostring_use_scratch_b

    ;; mode 0: normal malloc'd, persistent buffer
    PUSH  R1
    MOV   R0, -1
    PUSH  R0             ; default precision (6 digits, or omitted if whole)
    CALL  __builtin_ftoa
    IADD  SP, 2
    JMP   __tostring_float_done

__tostring_use_scratch_a:
    PUSH  R1
    MOV   R0, -1
    PUSH  R0
    CALL  __builtin_ftoa_scratch_a
    IADD  SP, 2
    JMP   __tostring_float_done

__tostring_use_scratch_b:
    PUSH  R1
    MOV   R0, -1
    PUSH  R0
    CALL  __builtin_ftoa_scratch_b
    IADD  SP, 2
    ;; falls straight through

__tostring_float_done:
    OR    R0, BOXED_RAMSTRING
    JMP   __tostring_done

__tostring_nil:
    MOV   R0, __const_str_nil
    OR    R0, BOXED_ROMSTRING
    JMP   __tostring_done

__tostring_false:
    MOV   R0, __const_str_false
    OR    R0, BOXED_ROMSTRING
    JMP   __tostring_done

__tostring_true:
    MOV   R0, __const_str_true
    OR    R0, BOXED_ROMSTRING
    JMP   __tostring_done

__tostring_passthrough:
    MOV   R0, R1
    MOV   SP, BP
    POP   BP
    RET  

__tostring_table:
    MOV   R0, R1                 ; a metatable's __tostring answers first
    MOV   R1, __mm_str_tostring
    OR    R1, BOXED_ROMSTRING
    CALL  __table_unary_meta
    JF    R1, __tostring_table_plain
    MOV   SP, BP
    POP   BP
    RET  
__tostring_table_plain:
    MOV   R0, __const_str_table  ; Load result directly
    OR    R0, BOXED_ROMSTRING
    MOV   SP, BP                 ; Restore stack
    POP   BP
    RET                                 ; Return to caller

__tostring_function:
    MOV   R0, __const_str_function  ; Load result directly
    OR    R0, BOXED_ROMSTRING       ; FIXED: was BOXED_FUNCTION
    MOV   SP, BP                 ; Restore stack
    POP   BP
    RET                                 ; Return to caller

__tostring_done:
    MOV   SP, BP
    POP   BP
    RET  

;; ---------------------------------------------------------------------------
;; Built-in: Float to ASCII (Full Floating Point Support)
;; Incoming Stack: [BP+3] = Raw IEEE754 Float (value to convert)
;;                 [BP+2] = Precision (raw hardware integer). Pass -1 for
;;                          "default" behavior (existing tostring() behavior):
;;                          6 fractional digits, but the decimal point and all
;;                          fractional digits are omitted entirely when the
;;                          value is a whole number. Pass 0..N for an explicit
;;                          precision (printf "%.Nf" semantics): the value is
;;                          ROUNDED (not truncated) to N fractional digits,
;;                          the decimal point + digits are always printed
;;                          unless N==0, and rounding correctly carries into
;;                          the integer part (e.g. ftoa(9.999, 2) -> "10.00").
;;                          Precision is clamped to a max of 17.
;; Returns: R0 = Raw Heap Pointer to null-terminated ASCII string
;; Registers: R0-R13 (R14/BP and R15/SP preserved)
;;
;; This allocates a FRESH, permanent 48-word heap block on every call --
;; correct and necessary when the resulting string genuinely becomes a
;; Lua value the program may keep (e.g. tostring()'s return value). Where
;; the result is instead immediately copied elsewhere or displayed and
;; never needed again, use __builtin_ftoa_scratch_a or _scratch_b below
;; instead -- see their own header comments for the rationale.
;; ---------------------------------------------------------------------------
__builtin_ftoa:
    PUSH  BP
    MOV   BP, SP

    ;; --- Allocate buffer ---
    ;; 48 words: sign + integer digits + '.' + up to 17 (clamped) frac digits
    ;; + null, with headroom. (Was 32; bumped since precision can now be large.)
    MOV   R0, 48
    PUSH  R0
    CALL  __malloc
    IADD  SP, 1

    MOV   R4, R0
    IEQ   R4, 0
    JT    R4, __oom_handler

    MOV   R2, R0                 ; R2 = locked-in base pointer
    JMP   __ftoa_shared_body

;; ---------------------------------------------------------------------------
;; Built-in: Float to ASCII, SHARED SCRATCH variant "A".
;;
;; Use instead of __builtin_ftoa wherever the resulting string is
;; immediately consumed (copied elsewhere, or displayed) and never needs
;; to survive as an independent, addressable Lua value in its own right.
;; Currently used by: string.format()'s numeric specifiers, print()'s
;; implicit number-to-string coercion, and __builtin_strcat's LEFT-operand
;; coercion. Each of these only ever needs ONE buffer alive at a time --
;; every result is fully copied out or displayed before the next coercion
;; can happen -- so sharing this one buffer across all three is safe.
;;
;; Costs exactly one 48-word allocation for the ENTIRE run of the cart --
;; made once, on the first call, and reused forever after -- instead of a
;; fresh 48-word block that becomes permanently unreachable garbage on
;; every single call. This runtime has no garbage collector, so that
;; memory could never be reclaimed any other way: a HUD that calls
;; string.format() or print() with a number once per frame (completely
;; ordinary for a TIC-80 cart) would otherwise exhaust available RAM
;; within minutes of continuous play.
;;
;; Incoming Stack / Returns / Registers: identical to __builtin_ftoa,
;; EXCEPT the returned pointer is the SAME shared buffer every time -- the
;; caller must finish using it (copy it out, or display it) before this
;; routine (or its indirect callers, e.g. __builtin_tostring_scratch_a) is
;; called again.
;; ---------------------------------------------------------------------------
__builtin_ftoa_scratch_a:
    PUSH  BP
    MOV   BP, SP

    MOV   R0, [FTOA_SCRATCH_PTR_A]
    IEQ   R0, 0
    JF    R0, __ftoa_scratch_a_have_buffer

    ;; First call ever: allocate the shared buffer once, exactly as
    ;; __builtin_ftoa does, and cache its address for every future call.
    MOV   R0, 48
    PUSH  R0
    CALL  __malloc
    IADD  SP, 1

    MOV   R4, R0
    IEQ   R4, 0
    JT    R4, __oom_handler

    MOV   [FTOA_SCRATCH_PTR_A], R0

__ftoa_scratch_a_have_buffer:
    MOV   R2, [FTOA_SCRATCH_PTR_A]
    JMP   __ftoa_shared_body

;; ---------------------------------------------------------------------------
;; Built-in: Float to ASCII, SHARED SCRATCH variant "B" -- a SECOND,
;; independent scratch buffer.
;;
;; Exists only because __builtin_strcat needs two coerced operands to
;; remain valid simultaneously: strcat measures BOTH operand lengths
;; before allocating its own final buffer, so if both sides need
;; coercing, the left-coerced string (built via scratch buffer "A") must
;; remain valid while the right side is being coerced too -- reusing
;; buffer "A" for the right side as well would silently overwrite the
;; left operand's still-needed content. Used ONLY by __builtin_strcat's
;; RIGHT-operand coercion.
;;
;; Otherwise identical in every respect to __builtin_ftoa_scratch_a.
;; ---------------------------------------------------------------------------
__builtin_ftoa_scratch_b:
    PUSH  BP
    MOV   BP, SP

    MOV   R0, [FTOA_SCRATCH_PTR_B]
    IEQ   R0, 0
    JF    R0, __ftoa_scratch_b_have_buffer

    MOV   R0, 48
    PUSH  R0
    CALL  __malloc
    IADD  SP, 1

    MOV   R4, R0
    IEQ   R4, 0
    JT    R4, __oom_handler

    MOV   [FTOA_SCRATCH_PTR_B], R0

__ftoa_scratch_b_have_buffer:
    MOV   R2, [FTOA_SCRATCH_PTR_B]
    ;; falls straight through into the shared body below

;; ===========================================================================
;; Shared conversion body -- reached from all three entry points above via
;; JMP/fallthrough, each having set R2 to that variant's buffer base
;; pointer first. Everything from here down is UNCHANGED from the
;; original __builtin_ftoa's own conversion algorithm.
;; ===========================================================================
__ftoa_shared_body:
    MOV   R9, R2                 ; R9 = write head

    MOV   R3, [BP+3]             ; R3 = value
    MOV   R8, [BP+2]             ; R8 = requested precision (-1 = default)

    ;; --- Clamp precision to 17 so a pathological caller can't overflow the buffer ---
    MOV   R4, R8
    INE   R4, -1
    JF    R4, __ftoa_precision_clamped   ; precision == -1 -> nothing to clamp
    MOV   R4, R8
    IGT   R4, 17
    JF    R4, __ftoa_precision_clamped   ; precision <= 17 -> nothing to clamp
    MOV   R8, 17
__ftoa_precision_clamped:

    ;; --- Sign ---
    MOV   R4, R3
    FLT   R4, 0.0
    JF    R4, __ftoa_positive
    MOV   R5, 45                 ; ASCII '-'
    MOV   [R9], R5
    IADD  R9, 1
    FABS  R3
    JMP   __ftoa_extract_int
__ftoa_positive:
__ftoa_extract_int:
    MOV   R4, R3
    CFI   R4                     ; truncate toward zero
    MOV   R7, R4                 ; R7 = integer part (may still be bumped by rounding)

    MOV   R6, R3
    CFI   R6
    CIF   R6
    MOV   R5, R3
    FSUB  R5, R6                 ; R5 = fractional part (0 <= R5 < 1)

    ;; --- Decide effective precision ---
    MOV   R12, R8                ; R12 = effective precision digit count
    MOV   R4, R8
    INE   R4, -1
    JT    R4, __ftoa_precision_explicit

    ;; --- Default mode: 6 digits, but omit '.' entirely if negligible ---
    MOV   R12, 6
    MOV   R6, R5
    MOV   R4, 0.000001
    FLT   R6, R4
    JF    R6, __ftoa_scale_and_round     ; fraction not negligible -> format it
    JMP   __ftoa_write_int_only           ; whole number in default mode -> skip '.'

__ftoa_precision_explicit:
    MOV   R4, R12
    IEQ   R4, 0
    JF    R4, __ftoa_scale_and_round
    ;; precision == 0: round to nearest integer, no '.' at all (printf %.0f)
    MOV   R6, R5
    MOV   R4, 0.5
    FLT   R6, R4
    JT    R6, __ftoa_write_int_only       ; frac < 0.5, no rounding needed
    IADD  R7, 1                            ; round the integer part up
    JMP   __ftoa_write_int_only

    ;; ==========================================================
    ;; Scale fraction by 10^precision and round-half-up
    ;; ==========================================================
__ftoa_scale_and_round:
    MOV   R6, 1.0                ; R6 = running power-of-ten (float)
    MOV   R13, 1                  ; R13 = running power-of-ten (integer)
    MOV   R4, R12                 ; R4 = countdown
__ftoa_pow10_loop:
    MOV   R11, R4                 ; compare a COPY -- IEQ is destructive and R4 must survive
    IEQ   R11, 0
    JT    R11, __ftoa_pow10_done
    MOV   R10, 10.0
    FMUL  R6, R10
    MOV   R10, 10
    IMUL  R13, R10
    IADD  R4, -1
    JMP   __ftoa_pow10_loop
__ftoa_pow10_done:

    MOV   R4, R5
    FMUL  R4, R6                  ; R4 = frac * 10^precision
    MOV   R10, 0.5
    FADD  R4, R10                 ; round-half-up
    CFI   R4                      ; R4 = rounded fractional digits (integer)

    ;; --- Carry: rounding pushed the fraction up to a whole unit ---
    MOV   R6, R4
    IGE   R6, R13
    JF    R6, __ftoa_push_frac
    IADD  R7, 1                   ; carry into the integer part
    MOV   R4, 0                   ; fractional digits reset to zero
__ftoa_push_frac:
    PUSH  R4                      ; save rounded fractional digits across int-write
    PUSH  R12                     ; save precision digit count across int-write
    MOV   R4, 1
    PUSH  R4                      ; has_fraction = 1
    JMP   __ftoa_write_int_common

__ftoa_write_int_only:
    MOV   R4, 0
    PUSH  R4                      ; dummy fractional digits (unused)
    PUSH  R4                      ; dummy precision (unused)
    PUSH  R4                      ; has_fraction = 0
    JMP   __ftoa_write_int_common

    ;; ==========================================================
    ;; Shared: write integer digits (destroys R7; R4/R5/R6/R10-R13 free
    ;; to use as scratch here, since the real fraction data is on the stack)
    ;; ==========================================================
__ftoa_write_int_common:
    MOV   R6, R7
    INE   R6, 0
    JT    R6, __ftoa_int_nonzero

    MOV   R6, 48                  ; ASCII '0'
    MOV   [R9], R6
    IADD  R9, 1
    JMP   __ftoa_int_digits_done

__ftoa_int_nonzero:
    MOV   R6, R9                  ; R6 = start of integer digits (for reversal)
__ftoa_int_loop:
    MOV   R5, R7
    INE   R5, 0
    JF    R5, __ftoa_reverse_int

    MOV   R5, R7
    IMOD  R5, 10
    IADD  R5, 48
    MOV   [R9], R5
    IADD  R9, 1
    IDIV  R7, 10
    JMP   __ftoa_int_loop

__ftoa_reverse_int:
    MOV   R10, R6
    MOV   R11, R9
    ISUB  R11, 1
__ftoa_reverse_int_loop:
    MOV   R5, R10
    IGE   R5, R11
    JT    R5, __ftoa_int_digits_done

    MOV   R12, [R10]
    MOV   R13, [R11]
    MOV   [R10], R13
    MOV   [R11], R12

    IADD  R10, 1
    ISUB  R11, 1
    JMP   __ftoa_reverse_int_loop
__ftoa_int_digits_done:

    ;; --- Restore the fraction data, decide whether it's needed ---
    POP   R6                      ; has_fraction flag
    POP   R12                     ; precision digit count
    POP   R4                      ; rounded fractional digits
    INE   R6, 0
    JF    R6, __ftoa_done         ; has_fraction == 0 -> done, no '.' at all

    ;; --- Write decimal point ---
    MOV   R6, 46                  ; ASCII '.'
    MOV   [R9], R6
    IADD  R9, 1

    ;; --- Write exactly R12 fractional digits (LSB-first, then reverse) ---
    MOV   R1, R9                  ; R1 = start of fractional digits (for reversal)
    MOV   R6, R12                 ; R6 = digit countdown
__ftoa_extract_frac:
    MOV   R11, R6                 ; compare a COPY -- IEQ is destructive and R6 must survive
    IEQ   R11, 0
    JT    R11, __ftoa_reverse_frac

    MOV   R10, R4
    IMOD  R10, 10
    IADD  R10, 48
    MOV   [R9], R10
    IADD  R9, 1
    IDIV  R4, 10
    ISUB  R6, 1
    JMP   __ftoa_extract_frac

__ftoa_reverse_frac:
    MOV   R10, R1
    MOV   R11, R9
    ISUB  R11, 1
__ftoa_reverse_frac_loop:
    MOV   R5, R10
    IGE   R5, R11
    JT    R5, __ftoa_done

    MOV   R12, [R10]
    MOV   R13, [R11]
    MOV   [R10], R13
    MOV   [R11], R12

    IADD  R10, 1
    ISUB  R11, 1
    JMP   __ftoa_reverse_frac_loop

__ftoa_done:
    ;; Default mode (precision -1) formats like Lua's %.14g as far as float32
    ;; allows: no trailing zeros ("0.5", not "0.500000"; "2.25", not
    ;; "2.250000"), and no '.' when rounding left nothing after it. Only
    ;; applied when a '.' was actually written, so "100" keeps its zeros.
    MOV   R10, [BP+2]
    IEQ   R10, -1
    JF    R10, __ftoa_terminate
    MOV   R10, R2
__ftoa_find_dot:
    MOV   R11, R10
    IGE   R11, R9
    JT    R11, __ftoa_terminate       ; no '.' -> nothing to trim
    MOV   R11, [R10]
    IEQ   R11, 46
    JT    R11, __ftoa_trim_zeros
    IADD  R10, 1
    JMP   __ftoa_find_dot
__ftoa_trim_zeros:
    MOV   R10, R9
    ISUB  R10, 1                      ; last written char
    MOV   R11, [R10]
    IEQ   R11, 48                     ; '0'?
    JF    R11, __ftoa_trim_dot
    MOV   R9, R10
    JMP   __ftoa_trim_zeros
__ftoa_trim_dot:
    MOV   R11, [R10]
    IEQ   R11, 46                     ; bare '.' left?
    JF    R11, __ftoa_terminate
    MOV   R9, R10
__ftoa_terminate:
    MOV   R10, 0
    MOV   [R9], R10               ; null terminator
    MOV   R0, R2                  ; return base pointer
    MOV   SP, BP
    POP   BP
    RET  

;; ---------------------------------------------------------------------------
;; __builtin_string_to_number(v): Lua tonumber(v), single-argument form.
;; Incoming Stack: [BP+2] = v (any boxed Lua value)
;; Returns: R0 = boxed result -- v unchanged if it was already a number,
;; the parsed number if v was a string that parses cleanly as one of this
;; language's own numeric-literal forms (decimal with optional fraction,
;; leading-dot decimal, 0x/0X hex integer -- the SAME set the lexer
;; itself accepts, nothing more), or BOXED_NIL for anything else
;; (non-string/non-number input, or a string with leftover non-whitespace
;; garbage after the number).
;;
;; SCOPE: no exponent notation, no explicit-base second argument, no
;; fractional/exponent hex ("0x1p4") -- matches the lexer's own grammar.
;; ---------------------------------------------------------------------------
    ;@fn __builtin_string_to_number clobbers=R0-R8
__builtin_string_to_number:
    PUSH  BP
    MOV   BP, SP

    MOV   R0, [BP+2]

    ;; --- Already a number? Return unchanged. A genuine Lua float never
    ;; matches the NaN-tag pattern every non-number value is built from. ---
    MOV   R1, R0
    AND   R1, NAN_VALUE
    IEQ   R1, NAN_VALUE
    JF    R1, __ton_already_number

    ;; --- Not a number. Must be a real STRING to proceed. ---
    MOV   R1, R0
    AND   R1, BOXED_DATA
    IEQ   R1, BOXED_ROMSTRING
    JT    R1, __ton_is_string

    MOV   R1, R0
    AND   R1, BOXED_DATA
    IEQ   R1, BOXED_RAMSTRING
    JF    R1, __ton_not_convertible

    MOV   R1, R0
    AND   R1, BOXED_PAYLOAD
    ILT   R1, 4
    JT    R1, __ton_not_convertible

__ton_is_string:
    CALL  __unbox_string          ; R0 = raw char pointer

__ton_skip_leading_ws:
    MOV   R1, [R0]
    MOV   R2, R1
    IEQ   R2, 32                  ; ' '
    JT    R2, __ton_ws_leading_advance
    MOV   R2, R1
    IEQ   R2, 9                    ; '\t'
    JT    R2, __ton_ws_leading_advance
    MOV   R2, R1
    IEQ   R2, 13                    ; '\r'
    JT    R2, __ton_ws_leading_advance
    MOV   R2, R1
    IEQ   R2, 10                     ; '\n'
    JF    R2, __ton_check_sign
__ton_ws_leading_advance:
    IADD  R0, 1
    JMP   __ton_skip_leading_ws

    ;; --- Optional leading sign ---
__ton_check_sign:
    MOV   R3, 0                   ; R3 = is_negative flag
    MOV   R1, [R0]
    MOV   R2, R1
    IEQ   R2, 45                    ; '-'
    JT    R2, __ton_sign_negative
    MOV   R2, R1
    IEQ   R2, 43                    ; '+'
    JF    R2, __ton_check_hex_prefix
    IADD  R0, 1
    JMP   __ton_check_hex_prefix
__ton_sign_negative:
    MOV   R3, 1
    IADD  R0, 1

    ;; --- Optional "0x"/"0X" prefix -> integer hex path ---
__ton_check_hex_prefix:
    MOV   R1, [R0]
    MOV   R2, R1
    IEQ   R2, 48                    ; '0'
    JF    R2, __ton_decimal
    MOV   R1, [R0+1]
    MOV   R2, R1
    IEQ   R2, 120                    ; 'x'
    JT    R2, __ton_hex_start
    MOV   R2, R1
    IEQ   R2, 88                      ; 'X'
    JF    R2, __ton_decimal
__ton_hex_start:
    IADD  R0, 2
    MOV   R4, 0                       ; R4 = accumulated value (float)
    MOV   R5, 0                        ; R5 = digit-seen count
__ton_hex_digit_loop:
    MOV   R1, [R0]

    MOV   R2, R1
    ISUB  R2, 48
    MOV   R6, R2
    ILT   R6, 0
    JT    R6, __ton_hex_try_upper
    MOV   R6, R2
    IGT   R6, 9
    JT    R6, __ton_hex_try_upper
    JMP   __ton_hex_digit_value_ready

__ton_hex_try_upper:
    MOV   R2, R1
    ISUB  R2, 65                 ; 'A'
    MOV   R6, R2
    ILT   R6, 0
    JT    R6, __ton_hex_try_lower
    MOV   R6, R2
    IGT   R6, 5
    JT    R6, __ton_hex_try_lower
    IADD  R2, 10
    JMP   __ton_hex_digit_value_ready

__ton_hex_try_lower:
    MOV   R2, R1
    ISUB  R2, 97                  ; 'a'
    MOV   R6, R2
    ILT   R6, 0
    JT    R6, __ton_hex_digit_loop_done
    MOV   R6, R2
    IGT   R6, 5
    JT    R6, __ton_hex_digit_loop_done
    IADD  R2, 10

__ton_hex_digit_value_ready:
    CIF   R2
    MOV   R7, 16.0
    FMUL  R4, R7
    FADD  R4, R2
    IADD  R5, 1
    IADD  R0, 1
    JMP   __ton_hex_digit_loop

__ton_hex_digit_loop_done:
    MOV   R6, R5
    IEQ   R6, 0
    JT    R6, __ton_fail            ; "0x" with no digits after -> malformed
    JMP   __ton_finish_check_trailing

    ;; --- Decimal path: optional integer part, optional fractional part ---
__ton_decimal:
    MOV   R4, 0                     ; R4 = accumulated value (float)
    MOV   R5, 0                      ; R5 = digit-seen count (int + frac combined)
__ton_decimal_int_loop:
    MOV   R1, [R0]
    MOV   R2, R1
    ISUB  R2, 48
    MOV   R6, R2
    ILT   R6, 0
    JT    R6, __ton_decimal_int_done
    MOV   R6, R2
    IGT   R6, 9
    JT    R6, __ton_decimal_int_done

    CIF   R2
    MOV   R7, 10.0
    FMUL  R4, R7
    FADD  R4, R2
    IADD  R5, 1
    IADD  R0, 1
    JMP   __ton_decimal_int_loop

__ton_decimal_int_done:
    MOV   R1, [R0]
    MOV   R2, R1
    IEQ   R2, 46                    ; '.'
    JF    R2, __ton_decimal_done

    IADD  R0, 1
    MOV   R8, 10.0                    ; R8 = running divisor
__ton_decimal_frac_loop:
    MOV   R1, [R0]
    MOV   R2, R1
    ISUB  R2, 48
    MOV   R6, R2
    ILT   R6, 0
    JT    R6, __ton_decimal_done
    MOV   R6, R2
    IGT   R6, 9
    JT    R6, __ton_decimal_done

    CIF   R2
    MOV   R7, R2
    FDIV  R7, R8
    FADD  R4, R7
    MOV   R7, 10.0
    FMUL  R8, R7
    IADD  R5, 1
    IADD  R0, 1
    JMP   __ton_decimal_frac_loop

__ton_decimal_done:
    MOV   R6, R5
    IEQ   R6, 0
    JT    R6, __ton_fail             ; no digits at all -- bare sign/whitespace/empty

    ;; --- Shared tail: skip trailing whitespace, confirm clean end-of-string ---
__ton_finish_check_trailing:
__ton_skip_trailing_ws:
    MOV   R1, [R0]
    MOV   R2, R1
    IEQ   R2, 32
    JT    R2, __ton_ws_trailing_advance
    MOV   R2, R1
    IEQ   R2, 9
    JT    R2, __ton_ws_trailing_advance
    MOV   R2, R1
    IEQ   R2, 13
    JT    R2, __ton_ws_trailing_advance
    MOV   R2, R1
    IEQ   R2, 10
    JF    R2, __ton_check_terminator
__ton_ws_trailing_advance:
    IADD  R0, 1
    JMP   __ton_skip_trailing_ws

__ton_check_terminator:
    MOV   R1, [R0]
    MOV   R2, R1
    IEQ   R2, 0
    JF    R2, __ton_fail             ; leftover garbage after the number

    MOV   R6, R3
    IEQ   R6, 0
    JT    R6, __ton_return_positive
    MOV   R7, -1.0
    FMUL  R4, R7
__ton_return_positive:
    MOV   R0, R4
    MOV   SP, BP
    POP   BP
    RET  

__ton_fail:
__ton_not_convertible:
    MOV   R0, BOXED_NIL
    MOV   SP, BP
    POP   BP
    RET  

__ton_already_number:
    MOV   SP, BP
    POP   BP
    RET  

;; ===========================================================================
;; Built-in: string.byte(s [, i [, j]])
;; Returns byte values from string as Lua numbers
;;
;; Incoming Stack: [BP+2] = string, [BP+3] = optional start index (1-based),
;;                 [BP+4] = optional end index
;; Returns: R0 = byte value (as boxed float), or BOXED_NIL on error
;; Clobbers: R0-R5
;; ===========================================================================
    ;@fn __builtin_string_byte clobbers=R0-R5
__builtin_string_byte:
    PUSH  BP
    MOV   BP, SP

    ;; --- Step 1: Load and unbox the string ---
    MOV   R0, [BP+2]          ; Load string argument
    CALL  __unbox_string      ; R0 = raw memory address (ROM or RAM)
    MOV   R1, R0              ; R1 = unboxed string pointer (preserve)

    ;; --- Step 2: Handle optional start index (defaults to 1 = index 0) ---
    MOV   R2, [BP+3]          ; Load start index argument
    IEQ   R2, BOXED_NIL      ; Check if start index is NIL
    JT    R2, __string_byte_default_start

    ;; Start index provided - the IEQ above destructively overwrote R2 with
    ;; its own 0/1 result, so reload the real argument before converting it.
    MOV   R2, [BP+3]
    CFI   R2                 ; Convert float to integer
    ISUB  R2, 1              ; Convert to 0-based index
    JMP   __string_byte_check_end

__string_byte_default_start:
    MOV   R2, 0              ; Default: start at first character (0-based)

    ;; --- Step 3: Handle optional end index (defaults to start index) ---
__string_byte_check_end:
    MOV   R3, [BP+4]          ; Load end index argument
    IEQ   R3, BOXED_NIL
    JT    R3, __string_byte_default_end

    ;; Same fix: reload R3 before converting, IEQ above clobbered it.
    MOV   R3, [BP+4]
    CFI   R3
    ISUB  R3, 1
    JMP   __string_byte_validate

__string_byte_default_end:
    MOV   R3, R2              ; Default: end = start (single byte)

    ;; --- Step 4: Validate indices are within string bounds ---
__string_byte_validate:
    ;; FIXED: __builtin_string_len clobbers R0-R2 internally (its loop
    ;; uses R2 as scratch, and its terminating destructive IEQ always
    ;; leaves R2 == 1 on exit -- see the header comment on this patch).
    ;; R1, R2, AND R3 all need to be preserved across this call -- pushing
    ;; an argument copy does NOT count as preserving it, since it's never
    ;; popped back. R3 happened to survive before this fix only because
    ;; __builtin_string_len's current implementation doesn't happen to
    ;; touch it -- that's an accident of the callee's implementation, not
    ;; a guarantee, so it's preserved explicitly now too rather than left
    ;; implicit.
    PUSH  R1                  ; preserve string pointer across the CALL below
    PUSH  R2                  ; preserve start index across the CALL below
    PUSH  R3                  ; preserve end index across the CALL below
    MOV   R0, [BP+2]          ; Arg 1: the BOXED string (NOT the pre-unboxed
    PUSH  R0                  ; pointer in R1 -- see string_len's header)
    CALL  __builtin_string_len
    IADD  SP, 1               ; Clean up the length-call's own argument
    MOV   R4, R0              ; R4 = length as float
    CFI   R4                  ; Convert float length to integer for comparisons
    POP   R3                  ; restore end index
    POP   R2                  ; restore start index
    POP   R1                  ; restore string pointer

    ;; Validate: start >= 0 AND start < length. Use a scratch register (R5)
    ;; for these comparisons -- R2/R3 are still needed below at Step 5 to
    ;; compute the byte address, and ILT/IGE destructively overwrite their
    ;; left operand with the 0/1 boolean result.
    MOV   R5, R2
    ILT   R5, 0
    JT    R5, __string_byte_error
    MOV   R5, R2
    IGE   R5, R4
    JT    R5, __string_byte_error

    ;; Validate: end >= start AND end < length
    MOV   R5, R3
    ILT   R5, R2
    JT    R5, __string_byte_error
    MOV   R5, R3
    IGE   R5, R4
    JT    R5, __string_byte_error

    ;; --- Step 5: Return byte at validated position ---
    IADD  R1, R2              ; R1 = string pointer + start offset
    MOV   R0, [R1]            ; Load byte from string memory
    CIF   R0                  ; Convert to Lua float
    JMP   __string_byte_done

    ;; --- Error handling ---
__string_byte_error:
    MOV   R0, BOXED_NIL      ; Return nil on any error

__string_byte_done:
    MOV   SP, BP
    POP   BP
    RET  

;; ===========================================================================
;; Built-in: string.char(b1, b2, ..., bn)
;; Creates a new string from byte values
;;
;; Incoming Stack: [BP+2] = first byte, [BP+3] = second byte, ...
;;                 Arguments MUST be terminated by BOXED_NIL
;; Returns: R0 = new string (boxed as RAM string)
;; Clobbers: R0-R6
;; ===========================================================================
__builtin_string_char:
    PUSH  BP
    MOV   BP, SP

    ;; --- Step 1: Count arguments by scanning for BOXED_NIL terminator ---
    MOV   R1, BP               ; R1 = base pointer to stack frame
    IADD  R1, 2               ; R1 = pointer to first argument ([BP+2])
    MOV   R2, 0               ; R2 = argument counter

__string_char_count_loop:
    MOV   R3, [R1]            ; Load current argument from stack
    MOV   R6, R3               ; scratch copy -- IEQ is destructive (this
                                ; loop doesn't reuse R3 afterward, but keep
                                ; the same safe idiom as the copy loop below)
    IEQ   R6, BOXED_NIL       ; Check if this is the terminator
    JT    R6, __string_char_count_done
    IADD  R2, 1               ; Increment argument counter
    IADD  R1, 1               ; Move pointer to next stack word
    JMP   __string_char_count_loop

__string_char_count_done:
    ;; --- Step 2: Allocate memory (R2 bytes + 1 for null terminator) ---
    ;; Note: __malloc expects size in WORDS, but R2 is in bytes.
    ;; For now we assume bytes == words (simplification for single-byte chars)
    IADD  R2, 1               ; +1 for null terminator byte
    PUSH  R2
    CALL  __malloc
    IADD  SP, 1
    MOV   R4, R0              ; R4 = heap pointer (preserve for return)

    ;; FIXED: use a scratch register (R3) for the OOM check instead of
    ;; testing R4 directly. R4 must survive intact -- Step 3 immediately
    ;; below reads it via "MOV R5, R4" to set up the write cursor. R3 is
    ;; free here: its value from the count loop above is no longer
    ;; needed, and the copy loop below reassigns it fresh from [R1] before
    ;; ever reading it again.
    MOV   R3, R4
    IEQ   R3, 0
    JT    R3, __string_char_oom

    ;; --- Step 3: Copy byte values from stack to heap ---
    MOV   R1, BP               ; Reset R1 to base pointer
    IADD  R1, 2               ; R1 = pointer to first argument ([BP+2])
    MOV   R5, R4              ; R5 = current write position in heap

__string_char_copy_loop:
    MOV   R3, [R1]            ; Load current argument
    ;; Use a scratch register (R6) for the nil-check instead of testing R3
    ;; directly -- IEQ is destructive, and CFI below needs R3 to still
    ;; hold the REAL argument value, not the check's own result.
    MOV   R6, R3               ; scratch copy for the nil-check
    IEQ   R6, BOXED_NIL       ; Check for terminator
    JT    R6, __string_char_copy_done

    ;; Convert Lua float byte to integer and store
    CFI   R3                  ; Convert float to integer (byte value) --
                                ; R3 is untouched by the check above, so
                                ; this now converts the real argument.
    MOV   [R5], R3            ; Store byte at current heap position
    IADD  R5, 1               ; Advance write pointer (by 1 word)
    IADD  R1, 1               ; Advance read pointer to next argument
    JMP   __string_char_copy_loop

__string_char_copy_done:
    ;; --- Step 4: Null-terminate the new string ---
    MOV   R3, 0
    MOV   [R5], R3            ; Write null terminator byte

    ;; --- Step 5: Box as RAM string and return ---
    MOV   R0, R4              ; Return heap pointer
    OR    R0, BOXED_RAMSTRING ; Tag as RAM string

    MOV   SP, BP
    POP   BP
    RET  

__string_char_oom:
    MOV   R0, BOXED_NIL      ; Return nil on allocation failure
    MOV   SP, BP
    POP   BP
    RET  

;; ===========================================================================
;; Built-in: string.format(s, ...)
;; Incoming Stack: [BP+2] = format string, [BP+3...] = args, BOXED_NIL terminated
;; Returns: R0 = formatted string (boxed RAM string)
;; ===========================================================================
__builtin_string_format:
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 6                 ; [BP-1]=width accum      [BP-2]=width snapshot
                                 ; [BP-3]=left-justify accum [BP-4]=left-justify snapshot
                                 ; [BP-5]=zero-pad accum   [BP-6]=zero-pad snapshot
    MOV   R0, 0
    MOV   [BP-1], R0
    MOV   [BP-2], R0
    MOV   [BP-3], R0
    MOV   [BP-4], R0
    MOV   [BP-5], R0
    MOV   [BP-6], R0

    ;; Load and unbox format string
    MOV   R0, [BP+2]
    CALL  __unbox_string
    MOV   R1, R0              ; R1 = format string pointer

    ;; Calculate format length for buffer estimation
    MOV   R2, 0
__string_format_calc_len:
    MOV   R4, R1               ; Compute address: R1 + R2
    IADD  R4, R2
    MOV   R3, [R4]            ; Load character at calculated address
    IEQ   R3, 0
    JT    R3, __string_format_len_done
    IADD  R2, 1
    JMP   __string_format_calc_len
__string_format_len_done:
    MOV   R0, R2
    IADD  R0, R2
    IADD  R0, R2            ; *3 for expansion
    IADD  R0, 64           ; + safety margin

    PUSH  R0
    CALL  __malloc
    IADD  SP, 1
    MOV   R4, R0            ; R4 = result buffer
    MOV   R3, R4             ; scratch copy -- IEQ is destructive, R4 must survive
    IEQ   R3, 0
    JT    R3, __string_format_oom

    MOV   R5, R4            ; R5 = write pointer
    MOV   R0, [BP+2]         ; FIX: was "MOV R1, [BP+2]" -- __unbox_string reads R0
    CALL  __unbox_string
    MOV   R1, R0            ; R1 = format string

    MOV   R6, BP
    IADD  R6, 3            ; R6 = arg pointer

__string_format_loop:
    MOV   R3, [R1]
    MOV   R2, R3
    IEQ   R2, 0
    JT    R2, __string_format_done

    MOV   R2, R3
    IEQ   R2, 37           ; '%'
    JF    R2, __string_format_copy_char

    IADD  R1, 1
    MOV   R3, [R1]
    MOV   R2, R3
    IEQ   R2, 37
    JT    R2, __string_format_literal_percent

    ;; --- Flags: '-' (left-justify) and '0' (zero-pad). Loops so either
    ;; can repeat or appear in any order; anything else falls through to
    ;; width parsing. Only these two are recognized -- '+', ' ', '#' still
    ;; hit the same silent-misparse failure mode described previously.
__string_format_check_flags:
    MOV   R2, R3
    IEQ   R2, 45                 ; '-'
    JT    R2, __string_format_flag_left_justify
    MOV   R2, R3
    IEQ   R2, 48                  ; '0'
    JT    R2, __string_format_flag_zero_pad
    JMP   __string_format_skip_width

__string_format_flag_left_justify:
    MOV   R2, 1
    MOV   [BP-3], R2
    IADD  R1, 1
    MOV   R3, [R1]
    JMP   __string_format_check_flags

__string_format_flag_zero_pad:
    MOV   R2, 1
    MOV   [BP-5], R2
    IADD  R1, 1
    MOV   R3, [R1]
    JMP   __string_format_check_flags

    ;; --- Parse width digits (if any) into [BP-1]; 0 = "no width given" ---
    ;; --- Optional '-' flag (left-justify). Only '-' is handled -- other
    ;; C flags ('+', ' ', '0', '#') aren't recognized and fall through to
    ;; the same broken behavior described in the diagnosis above if used.
__string_format_skip_width:
    MOV   R2, R3
    ISUB  R2, 48
    ILT   R2, 0
    JT    R2, __string_format_check_dot
    MOV   R2, R3
    ISUB  R2, 48
    IGT   R2, 9
    JT    R2, __string_format_check_dot

    MOV   R2, [BP-1]
    MOV   R8, 10
    IMUL  R2, R8
    MOV   R8, R3
    ISUB  R8, 48
    IADD  R2, R8
    MOV   [BP-1], R2

    IADD  R1, 1
    MOV   R3, [R1]
    JMP   __string_format_skip_width

    ;; --- Optional ".NN" precision ---
__string_format_check_dot:
    MOV   R12, -1                ; R12 = parsed precision; -1 means "unspecified"
    MOV   R2, R3
    IEQ   R2, 46                 ; '.'
    JF    R2, __string_format_dispatch_specifier

    IADD  R1, 1
    MOV   R3, [R1]
    MOV   R12, 0
__string_format_precision_digits:
    MOV   R2, R3
    ISUB  R2, 48
    ILT   R2, 0
    JT    R2, __string_format_dispatch_specifier
    MOV   R2, R3
    ISUB  R2, 48
    IGT   R2, 9
    JT    R2, __string_format_dispatch_specifier

    MOV   R2, 10
    IMUL  R12, R2
    MOV   R2, R3
    ISUB  R2, 48
    IADD  R12, R2

    IADD  R1, 1
    MOV   R3, [R1]
    JMP   __string_format_precision_digits

    ;; --- Now that we're at a real specifier char, consume the argument ---
__string_format_dispatch_specifier:
    MOV   R2, [BP-1]
    MOV   [BP-2], R2
    MOV   R2, 0
    MOV   [BP-1], R2

    MOV   R2, [BP-3]
    MOV   [BP-4], R2
    MOV   R2, 0
    MOV   [BP-3], R2

    MOV   R2, [BP-5]          ; snapshot zero-pad flag for THIS specifier
    MOV   [BP-6], R2
    MOV   R2, 0
    MOV   [BP-5], R2            ; reset for the NEXT specifier

    MOV   R0, [R6]
    IADD  R6, 1

    MOV   R2, R3
    IEQ   R2, 100          ; 'd'
    JT    R2, __string_format_handle_di
    MOV   R2, R3
    IEQ   R2, 105          ; 'i'
    JT    R2, __string_format_handle_di
    MOV   R2, R3
    IEQ   R2, 117          ; 'u'
    JT    R2, __string_format_handle_u
    MOV   R2, R3
    IEQ   R2, 102          ; 'f'
    JT    R2, __string_format_handle_f
    MOV   R2, R3
    IEQ   R2, 101          ; 'e'
    JT    R2, __string_format_handle_efg
    MOV   R2, R3
    IEQ   R2, 69           ; 'E'
    JT    R2, __string_format_handle_efg
    MOV   R2, R3
    IEQ   R2, 103          ; 'g'
    JT    R2, __string_format_handle_efg
    MOV   R2, R3
    IEQ   R2, 71           ; 'G'
    JT    R2, __string_format_handle_efg
    MOV   R2, R3
    IEQ   R2, 115          ; 's'
    JT    R2, __string_format_handle_s
    MOV   R2, R3
    IEQ   R2, 99           ; 'c'
    JT    R2, __string_format_handle_c
    MOV   R2, R3
    IEQ   R2, 113          ; 'q'
    JT    R2, __string_format_handle_q
    MOV   R2, R3
    IEQ   R2, 120           ; 'x'
    JT    R2, __string_format_handle_x_lower
    MOV   R2, R3
    IEQ   R2, 88             ; 'X'
    JT    R2, __string_format_handle_x_upper
    JMP   __string_format_write_char

__string_format_literal_percent:
    MOV   R3, 37
    JMP   __string_format_copy_char   ; FIX: was "JMP write_char", which skipped
                                       ; the actual write -- "%%" wrote nothing.

__string_format_handle_x_lower:
    MOV   R2, R0
    IEQ   R2, BOXED_NIL
    JT    R2, __string_format_arg_nil
    PUSH  R1
    PUSH  R4
    PUSH  R5
    PUSH  R6
    MOV   R2, R12             ; NEW: pass through the already-parsed precision
    MOV   R1, 0                 ; uppercase = false
    CALL  __string_format_itoa_hex
    POP   R6
    POP   R5
    POP   R4
    POP   R1
    MOV   R7, R0
    JMP   __string_format_copy_string

__string_format_handle_x_upper:
    MOV   R2, R0
    IEQ   R2, BOXED_NIL
    JT    R2, __string_format_arg_nil
    PUSH  R1
    PUSH  R4
    PUSH  R5
    PUSH  R6
    MOV   R2, R12             ; NEW
    MOV   R1, 1                 ; uppercase = true
    CALL  __string_format_itoa_hex
    POP   R6
    POP   R5
    POP   R4
    POP   R1
    MOV   R7, R0
    JMP   __string_format_copy_string

__string_format_handle_di:
    MOV   R2, R0
    IEQ   R2, BOXED_NIL
    JT    R2, __string_format_arg_nil
    PUSH  R1
    PUSH  R4
    PUSH  R5
    PUSH  R6
    CALL  __string_format_trunc_toward_zero  ; NEW: R0 = truncated (was: raw rounding via precision-0 ftoa)
    PUSH  R0
    MOV   R2, 0
    PUSH  R2                     ; precision 0: no '.' (value is already integral now)
    CALL  __builtin_ftoa_scratch_a
    IADD  SP, 2
    POP   R6
    POP   R5
    POP   R4
    POP   R1
    MOV   R7, R0
    JMP   __string_format_copy_string

__string_format_handle_u:
    MOV   R2, R0
    IEQ   R2, BOXED_NIL
    JT    R2, __string_format_arg_nil
    PUSH  R1
    PUSH  R4
    PUSH  R5
    PUSH  R6
    CALL  __string_format_trunc_toward_zero
    PUSH  R0
    MOV   R2, 0
    PUSH  R2
    CALL  __builtin_ftoa_scratch_a  ; was __builtin_ftoa
    IADD  SP, 2
    POP   R6
    POP   R5
    POP   R4
    POP   R1
    MOV   R7, R0
    JMP   __string_format_copy_string

__string_format_handle_f:
    MOV   R2, R0
    IEQ   R2, BOXED_NIL
    JT    R2, __string_format_arg_nil
    PUSH  R1
    PUSH  R4
    PUSH  R5
    PUSH  R6
    PUSH  R0                     ; value
    ;; %f with no precision is C's/Lua's %.6f ("2.500000"). -1 would select
    ;; ftoa's default mode, which since the tostring() change formats like
    ;; %.14g ("2.5") -- right for tostring/.., wrong for %f.
    MOV   R2, R12
    IEQ   R2, -1
    JF    R2, __string_format_f_has_prec
    MOV   R12, 6
__string_format_f_has_prec:
    PUSH  R12                    ; precision (6 if the format string gave none)
    CALL  __builtin_ftoa_scratch_a  ; was __builtin_ftoa
    IADD  SP, 2
    POP   R6
    POP   R5
    POP   R4
    POP   R1
    MOV   R7, R0
    JMP   __string_format_copy_string

__string_format_handle_efg:
    ;; NOTE: still fixed-point via ftoa, same as before this fix -- true
    ;; scientific notation / shortest-representation formatting for %e/%g is
    ;; not implemented. This fix only makes it respect precision like %f now
    ;; does; it does not add exponent formatting.
    MOV   R2, R0
    IEQ   R2, BOXED_NIL
    JT    R2, __string_format_arg_nil
    PUSH  R1
    PUSH  R4
    PUSH  R5
    PUSH  R6
    PUSH  R0
    PUSH  R12
    CALL  __builtin_ftoa_scratch_a  ; was __builtin_ftoa
    IADD  SP, 2
    POP   R6
    POP   R5
    POP   R4
    POP   R1
    MOV   R7, R0
    JMP   __string_format_copy_string

__string_format_handle_s:
    MOV   R2, R0
    IEQ   R2, BOXED_NIL
    JT    R2, __string_format_arg_nil
    ;; %s applies tostring() to its argument, as Lua does: a number, boolean,
    ;; table or function is converted first. Unboxing a number as if it were
    ;; a string pointer read from a wild address ("invalid memory read" on
    ;; tomb_of_the_tic's level-clear screen, format("%s / %s", score, ...)).
    PUSH  R1
    PUSH  R4
    PUSH  R5
    PUSH  R6
    PUSH  R0
    CALL  __builtin_tostring_scratch_a
    IADD  SP, 1
    POP   R6
    POP   R5
    POP   R4
    POP   R1
    PUSH  R1                     ; save loop state: __unbox_string clobbers R1
    CALL  __unbox_string
    POP   R1
    MOV   R7, R0
    JMP   __string_format_copy_string

__string_format_handle_c:
    MOV   R2, R0
    IEQ   R2, BOXED_NIL
    JT    R2, __string_format_arg_nil
    CFI   R0
    AND   R0, 255
    MOV   [R5], R0
    IADD  R5, 1
    IADD  R1, 1
    JMP   __string_format_loop

__string_format_handle_q:
    MOV   R2, R0
    IEQ   R2, BOXED_NIL
    JT    R2, __string_format_arg_nil
    ;; Only strings are quoted; a number/boolean is written as a literal
    ;; (Lua 5.4), which is what %s produces.
    MOV   R2, R0
    AND   R2, BOXED_DATA
    IEQ   R2, BOXED_ROMSTRING
    JT    R2, __string_format_q_is_string
    MOV   R2, R0
    AND   R2, BOXED_DATA
    IEQ   R2, BOXED_RAMSTRING
    JF    R2, __string_format_handle_s
    MOV   R2, R0                 ; false/true share the RAM-string tag
    AND   R2, BOXED_PAYLOAD      ; (payloads 1/2); real strings are >= 4
    ILT   R2, 4
    JT    R2, __string_format_handle_s
__string_format_q_is_string:
    PUSH  R1                     ; save loop state: __unbox_string clobbers R1
    CALL  __unbox_string
    POP   R1
    MOV   R7, R0
    MOV   R3, 34
    MOV   [R5], R3
    IADD  R5, 1
    CALL  __string_format_copy_string_inner
    MOV   R3, 34
    MOV   [R5], R3
    IADD  R5, 1
    IADD  R1, 1
    JMP   __string_format_loop

;; ---------------------------------------------------------------------------
;; __string_format_itoa_hex: converts R0 (boxed Lua float) to a
;; null-terminated hex string. R1 = uppercase flag, R2 = requested
;; precision (-1 = none; minimum digit count, zero-padded -- e.g. "%.8X"
;; on 0xFF gives "000000ff", NOT just "ff").
;;
;; SCOPE: sign+magnitude, not two's-complement (unchanged from before).
;; Precision is clamped to 16 digits -- generous headroom over the
;; natural 8-digit ceiling of a 32-bit value, for deliberate alignment
;; padding, without letting a pathological request overflow the buffer.
;; Output: R0 = raw (unboxed) pointer to the result string.
;; Clobbers R0-R6.
;; ---------------------------------------------------------------------------
__string_format_itoa_hex:
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 3          ; [BP-1]=uppercase  [BP-2]=is_negative  [BP-3]=precision

    MOV   [BP-1], R1
    MOV   [BP-3], R2

    ;; --- Clamp precision to 16 (unchanged) ---
    MOV   R3, [BP-3]
    MOV   R4, R3
    IEQ   R4, -1
    JT    R4, __hex_precision_clamped
    MOV   R4, R3
    IGT   R4, 16
    JF    R4, __hex_precision_clamped
    MOV   R3, 16
    MOV   [BP-3], R3
__hex_precision_clamped:

    CALL  __string_format_trunc_toward_zero

    MOV   R3, R0
    FLT   R3, 0.0
    MOV   [BP-2], R3

    JF    R3, __hex_have_magnitude
    MOV   R4, -1.0
    FMUL  R0, R4
__hex_have_magnitude:
    ;; --- NEW: split magnitude into hi/lo 16-bit halves BEFORE converting
    ;; to int. CFI saturates at INT32_MAX (2147483647); a full 32-bit
    ;; magnitude like 0xDEADBEEF (3735928559) exceeds that and would
    ;; silently clamp to 0x7FFFFFFF otherwise. Each half maxes at 65535,
    ;; safely inside int32 for the entire uint32 span.
    MOV   R6, R0              ; R6 = magnitude (float), full precision kept
    MOV   R5, 65536.0
    FDIV  R0, R5                ; R0 = magnitude / 65536
    FLR   R0                      ; R0 = hi (float)
    MOV   R4, R0                   ; R4 = hi (float), stashed
    FMUL  R0, R5                    ; R0 = hi * 65536
    FSUB  R6, R0                      ; R6 = magnitude - hi*65536 = lo (float)
    CFI   R6                            ; R6 = lo (raw int, 0-65535)
    MOV   R0, R4
    CFI   R0                              ; R0 = hi (raw int, 0-65535)

    PUSH  R0                    ; spill hi across the malloc call below
    PUSH  R6                     ; spill lo across the malloc call below

    MOV   R5, 20          ; sign + up to 16 (clamped) digits + null + margin
    PUSH  R5
    CALL  __malloc
    IADD  SP, 1
    MOV   R6, R0

    POP   R0             ; R0 = lo (reloaded)
    POP   R4             ; R4 = hi (reloaded)

    MOV   R1, R6
    MOV   R3, [BP-2]
    JF    R3, __hex_no_sign
    MOV   R3, 45          ; '-'
    MOV   [R1], R3
    IADD  R1, 1
__hex_no_sign:

    ;; --- Extract lo's digits first (natural stop-on-zero, same as before) ---
    MOV   R5, 0
    MOV   R3, R0
    IEQ   R3, 0
    JF    R3, __hex_lo_nonzero
    MOV   R3, 48          ; '0'
    PUSH  R3
    IADD  R5, 1
    JMP   __hex_lo_done
__hex_lo_nonzero:
__hex_lo_extract_loop:
    MOV   R3, R0
    IEQ   R3, 0
    JT    R3, __hex_lo_done

    MOV   R3, R0
    MOV   R2, 16
    IMOD  R3, R2
    MOV   R2, 16
    IDIV  R0, R2

    MOV   R2, R3
    ILT   R2, 10
    JT    R2, __hex_lo_digit_09
    MOV   R2, [BP-1]
    JF    R2, __hex_lo_digit_lower
    IADD  R3, 55          ; 'A'-10
    JMP   __hex_lo_store
__hex_lo_digit_lower:
    IADD  R3, 87           ; 'a'-10
    JMP   __hex_lo_store
__hex_lo_digit_09:
    IADD  R3, 48            ; '0'
__hex_lo_store:
    PUSH  R3
    IADD  R5, 1
    JMP   __hex_lo_extract_loop
__hex_lo_done:

    ;; --- If hi is nonzero, pad lo up to exactly 4 digits (they're now
    ;; internal digits, not leading padding), then extract hi's digits ---
    MOV   R3, R4
    IEQ   R3, 0
    JT    R3, __hex_extract_done   ; hi==0: lo's natural digits are the whole answer

    MOV   R2, 4
    ISUB  R2, R5             ; R2 = how many zero-pads lo needs to reach 4
__hex_lo_pad_loop:
    MOV   R3, R2
    ILT   R3, 1
    JT    R3, __hex_hi_extract
    MOV   R3, 48
    PUSH  R3
    IADD  R5, 1
    ISUB  R2, 1
    JMP   __hex_lo_pad_loop

__hex_hi_extract:
    MOV   R0, R4                 ; R0 = hi
__hex_hi_extract_loop:
    MOV   R3, R0
    IEQ   R3, 0
    JT    R3, __hex_extract_done

    MOV   R3, R0
    MOV   R2, 16
    IMOD  R3, R2
    MOV   R2, 16
    IDIV  R0, R2

    MOV   R2, R3
    ILT   R2, 10
    JT    R2, __hex_hi_digit_09
    MOV   R2, [BP-1]
    JF    R2, __hex_hi_digit_lower
    IADD  R3, 55
    JMP   __hex_hi_store
__hex_hi_digit_lower:
    IADD  R3, 87
    JMP   __hex_hi_store
__hex_hi_digit_09:
    IADD  R3, 48
__hex_hi_store:
    PUSH  R3
    IADD  R5, 1
    JMP   __hex_hi_extract_loop

__hex_extract_done:
    MOV   R3, [BP-3]           ; precision
    MOV   R2, R3
    IEQ   R2, -1
    JT    R2, __hex_precision_done

    MOV   R2, R3
    ISUB  R2, R5                ; R2 = needed = precision - digit_count -- this
                                  ; stays the REAL counter from here on, never
                                  ; fed into a destructive test directly
    MOV   R3, R2                 ; R3 = disposable copy, just for this one test
    ILT   R3, 1
    JT    R3, __hex_precision_done

__hex_precision_pad_loop:
    MOV   R3, 48                  ; '0'
    PUSH  R3
    IADD  R5, 1
    ISUB  R2, 1                    ; decrement the REAL counter
    MOV   R3, R2                    ; fresh disposable copy for THIS iteration's test
    ILT   R3, 1
    JF    R3, __hex_precision_pad_loop
__hex_precision_done:

    MOV   R4, R5
__hex_write_loop:
    JF    R4, __hex_terminate
    POP   R3
    MOV   [R1], R3
    IADD  R1, 1
    ISUB  R4, 1
    JMP   __hex_write_loop

__hex_terminate:
    MOV   R3, 0
    MOV   [R1], R3

    MOV   R0, R6
    MOV   SP, BP
    POP   BP
    RET  

__string_format_arg_nil:
    MOV   R3, 110
    MOV   [R5], R3
    IADD  R5, 1
    MOV   R3, 105
    MOV   [R5], R3
    IADD  R5, 1
    MOV   R3, 108
    MOV   [R5], R3
    IADD  R5, 1
    IADD  R1, 1
    JMP   __string_format_loop

    ;; --- Entry point used by %d/%i/%u/%f/%e/%g/%s/%x/%X: applies width
    ;;     padding (if any was requested, e.g. "%5d"), then copies the C
    ;;     string from R7, then resumes the outer format loop.
    ;;
    ;; PADDING SCOPE: only these 9 specifiers -- the ones that funnel
    ;; through this shared routine -- are padded. %c and %q write through
    ;; separate dedicated paths (direct inline write / copy_string_inner)
    ;; and do NOT respect a requested width; not extended to cover them
    ;; since nothing currently needs it and it'd mean duplicating this
    ;; same measure-then-pad logic in two more places for an untested case.
__string_format_copy_string:
    MOV   R8, R7
    MOV   R2, 0
__string_format_copy_string_measure:
    MOV   R3, [R8]
    IEQ   R3, 0
    JT    R3, __string_format_copy_string_measured
    IADD  R8, 1
    IADD  R2, 1
    JMP   __string_format_copy_string_measure
__string_format_copy_string_measured:

    MOV   R3, [BP-2]               ; requested width
    ISUB  R3, R2                     ; R3 = pad count (width - length, may be <= 0)

    MOV   R2, [BP-4]                 ; left-justify flag
    JT    R2, __string_format_copy_string_emit_left

    ;; --- Right-justify (default): spaces BEFORE the value ---
__string_format_copy_string_pad_right:
    MOV   R2, R3
    ILT   R2, 1
    JT    R2, __string_format_copy_string_emit_right
    MOV   R2, [BP-6]            ; zero-pad flag
    JF    R2, __string_format_pad_right_space
    MOV   R2, 48                  ; '0'
    JMP   __string_format_pad_right_write
__string_format_pad_right_space:
    MOV   R2, 32                   ; ' '
__string_format_pad_right_write:
    MOV   [R5], R2
    IADD  R5, 1
    ISUB  R3, 1
    JMP   __string_format_copy_string_pad_right

__string_format_copy_string_emit_right:
    MOV   R3, [R7]
    MOV   R2, R3
    IEQ   R2, 0
    JT    R2, __string_format_after_copy
    MOV   [R5], R3
    IADD  R5, 1
    IADD  R7, 1
    JMP   __string_format_copy_string_emit_right

    ;; --- Left-justify: value FIRST, spaces AFTER ---
__string_format_copy_string_emit_left:
    MOV   R9, R3                     ; stash pad count -- R3 becomes a
                                       ; char-scratch register during the copy
__string_format_copy_string_emit_left_loop:
    MOV   R3, [R7]
    MOV   R2, R3
    IEQ   R2, 0
    JT    R2, __string_format_pad_left
    MOV   [R5], R3
    IADD  R5, 1
    IADD  R7, 1
    JMP   __string_format_copy_string_emit_left_loop
__string_format_pad_left:
    MOV   R3, R9
__string_format_copy_string_pad_left_loop:
    MOV   R2, R3
    ILT   R2, 1
    JT    R2, __string_format_after_copy
    MOV   R2, 32
    MOV   [R5], R2
    IADD  R5, 1
    ISUB  R3, 1
    JMP   __string_format_copy_string_pad_left_loop

__string_format_after_copy:
    IADD  R1, 1
    JMP   __string_format_loop

    ;; --- Used by %q only: same copy, but returns to its caller (via RET)
    ;;     instead of resuming the outer loop, since %q still has a closing
    ;;     quote to write first ---
    ;@fn __string_format_copy_string_inner clobbers=R2-R3,R5,R7
__string_format_copy_string_inner:
    MOV   R3, [R7]
    MOV   R2, R3
    IEQ   R2, 0
    JT    R2, __string_format_copy_string_inner_done
    MOV   [R5], R3
    IADD  R5, 1
    IADD  R7, 1
    JMP   __string_format_copy_string_inner
__string_format_copy_string_inner_done:
    RET  

__string_format_copy_char:
    MOV   [R5], R3
    IADD  R5, 1
__string_format_write_char:
    IADD  R1, 1
    JMP   __string_format_loop

__string_format_done:
    MOV   R3, 0
    MOV   [R5], R3
    MOV   R0, R4
    OR    R0, BOXED_RAMSTRING
    MOV   SP, BP
    POP   BP
    RET  

;; ---------------------------------------------------------------------------
;; __string_format_trunc_toward_zero: truncates R0 (a boxed Lua float)
;; toward zero (C/Lua %d semantics) -- NOT round-to-nearest, and NOT floor
;; (floor(-9.9) = -10, but %d on -9.9 must give -9).
;; Output: R0 = truncated boxed float. Clobbers R0, R1 only.
;; ---------------------------------------------------------------------------
    ;@fn __string_format_trunc_toward_zero clobbers=R0-R1
__string_format_trunc_toward_zero:
    MOV   R1, R0
    FLT   R1, 0.0                ; R1 = (value < 0)?
    JT    R1, __trunc_negative

    FLR   R0                     ; non-negative: floor == truncate
    RET  

__trunc_negative:
    MOV   R1, -1.0
    FMUL  R0, R1                 ; negate to positive magnitude
    FLR   R0                     ; floor the magnitude
    FMUL  R0, R1                 ; negate back
    RET  

__string_format_oom:
    MOV   R0, BOXED_NIL
    MOV   SP, BP
    POP   BP
    RET  

;; ===========================================================================
;; Built-in: string.sub(s, i [, j])
;; Incoming Stack: [BP+2] = string, [BP+3] = start index i (1-based Lua
;;                 float, may be negative -- negative counts from the end),
;;                 [BP+4] = end index j (same rules), or BOXED_NIL to
;;                 default to the end of the string (Lua's j = -1).
;; Returns: R0 = new string (boxed RAM string). Out-of-range indices are
;;          clamped, not errors, matching Lua semantics. i > j (after
;;          normalization) returns an empty string.
;; Clobbers: R0-R8
;; ===========================================================================
    ;@fn __builtin_string_sub clobbers=R0-R8
__builtin_string_sub:
    PUSH  BP
    MOV   BP, SP

    ;; --- Step 1: Unbox string, compute length ---
    MOV   R0, [BP+2]
    CALL  __unbox_string
    MOV   R1, R0               ; R1 = unboxed string pointer (persist)

    PUSH  R1
    MOV   R0, [BP+2]           ; Arg 1: the BOXED string (NOT the pre-unboxed
    PUSH  R0                   ; pointer in R1 -- see string_len's header)
    CALL  __builtin_string_len
    IADD  SP, 1
    MOV   R4, R0
    CFI   R4                   ; R4 = length (integer)
    POP   R1                   ; restore string pointer

    ;; --- Step 2: Load & normalize start index i (required, 1-based) ---
    MOV   R6, [BP+3]
    CFI   R6                   ; R6 = i (raw integer, may be negative or 0)

    MOV   R5, R6
    ILT   R5, 0
    JF    R5, __string_sub_i_norm_done
    MOV   R5, R4
    IADD  R5, R6
    IADD  R5, 1
    MOV   R6, R5
__string_sub_i_norm_done:
    MOV   R5, R6
    ILT   R5, 1
    JF    R5, __string_sub_j_load
    MOV   R6, 1

    ;; --- Step 3: Load & normalize end index j (optional, defaults to len) ---
__string_sub_j_load:
    MOV   R7, [BP+4]
    MOV   R5, R7
    IEQ   R5, BOXED_NIL
    JT    R5, __string_sub_j_default

    MOV   R7, [BP+4]            ; reload -- IEQ above only clobbered R5
    CFI   R7

    MOV   R5, R7
    ILT   R5, 0
    JF    R5, __string_sub_j_clamp
    MOV   R5, R4
    IADD  R5, R7
    IADD  R5, 1
    MOV   R7, R5
    JMP   __string_sub_j_clamp

__string_sub_j_default:
    MOV   R7, R4

__string_sub_j_clamp:
    MOV   R5, R7
    IGT   R5, R4
    JF    R5, __string_sub_range_check
    MOV   R7, R4

    ;; --- Step 4: If i > j after normalization, result is empty ---
__string_sub_range_check:
    MOV   R5, R6
    IGT   R5, R7
    JT    R5, __string_sub_empty

    ;; --- Step 5: Allocate result buffer and copy [i-1, j-1] inclusive ---
    MOV   R8, R7
    ISUB  R8, R6
    IADD  R8, 1                 ; R8 = count of chars to copy = j - i + 1
    MOV   R2, R8
    IADD  R2, 1                 ; +1 for null terminator

    ;; __malloc clobbers R0-R3 and R6. R1 (string pointer) and R6 (start
    ;; index) are both read again after this call, so both are preserved.
    PUSH  R1
    PUSH  R6
    PUSH  R2
    CALL  __malloc
    IADD  SP, 1
    MOV   R3, R0                 ; R3 = destination heap pointer
    POP   R6                     ; restore start index
    POP   R1                     ; restore string pointer

    ;; OOM check via scratch register R2 -- R2 is free here (its only
    ;; earlier use was as the malloc size argument, already consumed by
    ;; the call above). R3 must stay untouched: "MOV R5, R3" below needs
    ;; the real heap pointer, not a 0/1 comparison result.
    MOV   R2, R3
    IEQ   R2, 0
    JT    R2, __string_sub_oom

    MOV   R2, R1                 ; R2 = read pointer = string base + (i-1)
    IADD  R2, R6
    ISUB  R2, 1

    MOV   R5, R3                 ; R5 = write pointer

__string_sub_copy_loop:
    MOV   R6, R8                 ; scratch copy -- IEQ is destructive
    IEQ   R6, 0
    JT    R6, __string_sub_copy_done
    MOV   R6, [R2]
    MOV   [R5], R6
    IADD  R2, 1
    IADD  R5, 1
    ISUB  R8, 1
    JMP   __string_sub_copy_loop

__string_sub_copy_done:
    MOV   R6, 0
    MOV   [R5], R6                ; null terminator
    MOV   R0, R3
    OR    R0, BOXED_RAMSTRING
    JMP   __string_sub_done

__string_sub_empty:
    ;; No registers need to survive the CALL itself, but R3 (the heap
    ;; pointer) IS needed after the OOM check below -- same fix applies.
    MOV   R3, 1
    PUSH  R3
    CALL  __malloc
    IADD  SP, 1
    MOV   R3, R0

    MOV   R2, R3
    IEQ   R2, 0
    JT    R2, __string_sub_oom

    MOV   R6, 0
    MOV   [R3], R6
    MOV   R0, R3
    OR    R0, BOXED_RAMSTRING
    JMP   __string_sub_done

__string_sub_oom:
    MOV   R0, BOXED_NIL

__string_sub_done:
    MOV   SP, BP
    POP   BP
    RET  


;; ===========================================================================
;; Built-in: string.upper(s)
;; Incoming Stack: [BP+2] = string
;; Returns: R0 = new string (boxed RAM string), same length as input, with
;;          ASCII 'a'-'z' converted to 'A'-'Z'. Non-letter bytes pass
;;          through unchanged.
;; Clobbers: R0-R6
;; ===========================================================================
    ;@fn __builtin_string_upper clobbers=R0-R6
__builtin_string_upper:
    PUSH  BP
    MOV   BP, SP

    MOV   R0, [BP+2]
    CALL  __unbox_string
    MOV   R1, R0                ; R1 = source string pointer (persist)

    PUSH  R1
    MOV   R0, [BP+2]           ; Arg 1: the BOXED string (NOT the pre-unboxed
    PUSH  R0                   ; pointer in R1 -- see string_len's header)
    CALL  __builtin_string_len
    IADD  SP, 1
    MOV   R4, R0
    CFI   R4
    POP   R1

    MOV   R2, R4
    IADD  R2, 1

    ;; __malloc clobbers R0-R3 and R6. R1 (source pointer) is read again
    ;; after this call, so it's preserved.
    PUSH  R1
    PUSH  R2
    CALL  __malloc
    IADD  SP, 1
    MOV   R3, R0                 ; R3 = destination heap pointer
    POP   R1                     ; restore source pointer

    ;; OOM check via scratch register R6 -- R6 is unused until inside the
    ;; loop below, so it's free here. R3 must stay untouched: "MOV R5, R3"
    ;; below needs the real heap pointer.
    MOV   R6, R3
    IEQ   R6, 0
    JT    R6, __string_upper_oom

    MOV   R2, R1                 ; R2 = read pointer
    MOV   R5, R3                 ; R5 = write pointer

__string_upper_loop:
    MOV   R0, [R2]
    MOV   R6, R0
    IEQ   R6, 0
    JT    R6, __string_upper_done_copy

    MOV   R6, R0
    ILT   R6, 97
    JT    R6, __string_upper_write
    MOV   R6, R0
    IGT   R6, 122
    JT    R6, __string_upper_write

    ISUB  R0, 32

__string_upper_write:
    MOV   [R5], R0
    IADD  R2, 1
    IADD  R5, 1
    JMP   __string_upper_loop

__string_upper_done_copy:
    MOV   R0, 0
    MOV   [R5], R0
    MOV   R0, R3
    OR    R0, BOXED_RAMSTRING
    JMP   __string_upper_finish

__string_upper_oom:
    MOV   R0, BOXED_NIL

__string_upper_finish:
    MOV   SP, BP
    POP   BP
    RET  


;; ===========================================================================
;; Built-in: string.lower(s)
;; Incoming Stack: [BP+2] = string
;; Returns: R0 = new string (boxed RAM string), same length as input, with
;;          ASCII 'A'-'Z' converted to 'a'-'z'. Non-letter bytes pass
;;          through unchanged.
;; Clobbers: R0-R6
;; ===========================================================================
    ;@fn __builtin_string_lower clobbers=R0-R6
__builtin_string_lower:
    PUSH  BP
    MOV   BP, SP

    MOV   R0, [BP+2]
    CALL  __unbox_string
    MOV   R1, R0

    PUSH  R1
    MOV   R0, [BP+2]           ; Arg 1: the BOXED string (NOT the pre-unboxed
    PUSH  R0                   ; pointer in R1 -- see string_len's header)
    CALL  __builtin_string_len
    IADD  SP, 1
    MOV   R4, R0
    CFI   R4
    POP   R1

    MOV   R2, R4
    IADD  R2, 1

    PUSH  R1
    PUSH  R2
    CALL  __malloc
    IADD  SP, 1
    MOV   R3, R0
    POP   R1

    ;; Same fix as string.upper -- scratch register R6 for the OOM check.
    MOV   R6, R3
    IEQ   R6, 0
    JT    R6, __string_lower_oom

    MOV   R2, R1
    MOV   R5, R3

__string_lower_loop:
    MOV   R0, [R2]
    MOV   R6, R0
    IEQ   R6, 0
    JT    R6, __string_lower_done_copy

    MOV   R6, R0
    ILT   R6, 65
    JT    R6, __string_lower_write
    MOV   R6, R0
    IGT   R6, 90
    JT    R6, __string_lower_write

    IADD  R0, 32

__string_lower_write:
    MOV   [R5], R0
    IADD  R2, 1
    IADD  R5, 1
    JMP   __string_lower_loop

__string_lower_done_copy:
    MOV   R0, 0
    MOV   [R5], R0
    MOV   R0, R3
    OR    R0, BOXED_RAMSTRING
    JMP   __string_lower_finish

__string_lower_oom:
    MOV   R0, BOXED_NIL

__string_lower_finish:
    MOV   SP, BP
    POP   BP
    RET  


;; ===========================================================================
;; Built-in: string.rep(s, n)
;; Incoming Stack: [BP+2] = string, [BP+3] = repeat count n (Lua float)
;; Returns: R0 = new string (boxed RAM string) = s repeated n times.
;;          n <= 0, or an empty source string, both yield "".
;; Clobbers: R0-R7
;; ===========================================================================
    ;@fn __builtin_string_rep clobbers=R0-R7
__builtin_string_rep:
    PUSH  BP
    MOV   BP, SP

    MOV   R0, [BP+2]
    CALL  __unbox_string
    MOV   R1, R0                ; R1 = source string pointer (persist)

    PUSH  R1
    MOV   R0, [BP+2]           ; Arg 1: the BOXED string (NOT the pre-unboxed
    PUSH  R0                   ; pointer in R1 -- see string_len's header)
    CALL  __builtin_string_len
    IADD  SP, 1
    MOV   R4, R0
    CFI   R4
    POP   R1

    MOV   R6, [BP+3]
    CFI   R6

    MOV   R5, R6
    IGT   R5, 0
    JF    R5, __string_rep_empty
    MOV   R5, R4
    IEQ   R5, 0
    JT    R5, __string_rep_empty

    MOV   R7, R4
    IMUL  R7, R6

    MOV   R2, R7
    IADD  R2, 1

    ;; __malloc clobbers R0-R3 and R6. Both R1 (source pointer) and R6
    ;; (repeat count n) are read again after this call, so both are
    ;; preserved.
    PUSH  R1
    PUSH  R6
    PUSH  R2
    CALL  __malloc
    IADD  SP, 1
    MOV   R3, R0                 ; R3 = destination heap pointer
    POP   R6                     ; restore repeat count
    POP   R1                     ; restore source pointer

    ;; OOM check via scratch register R2 -- free here (its only earlier
    ;; use was as the malloc size argument, already consumed). R3 must
    ;; stay untouched: "MOV R5, R3" below needs the real heap pointer.
    MOV   R2, R3
    IEQ   R2, 0
    JT    R2, __string_rep_oom

    MOV   R5, R3                 ; R5 = write pointer

__string_rep_outer_loop:
    MOV   R2, R6                 ; scratch copy of remaining repeat count
    IEQ   R2, 0
    JT    R2, __string_rep_done_copy

    MOV   R0, R1                 ; R0 = read pointer, reset to source start

;; Register note: R6 is NOT available as scratch here -- it holds the
;; remaining repeat count (n) and is live across every inner-loop
;; iteration, read again at __string_rep_inner_done ("ISUB R6, 1"). R4 is
;; used instead: it held the source string length earlier in the routine
;; (for the total-length multiply, before the __malloc call) and is never
;; read again after that, so it's genuinely free here.

__string_rep_inner_loop:
    MOV   R2, [R0]
    ;; FIXED: scratch copy (R4) for the nil-check, instead of testing R2
    ;; directly. R4 is free here (only used earlier for source length,
    ;; already consumed by the total-length calculation before the
    ;; __malloc call). R6 is NOT available -- it holds the repeat count,
    ;; live across every iteration of this loop.
    MOV   R4, R2
    IEQ   R4, 0
    JT    R4, __string_rep_inner_done
    MOV   [R5], R2
    IADD  R0, 1
    IADD  R5, 1
    JMP   __string_rep_inner_loop

__string_rep_inner_done:
    ISUB  R6, 1
    JMP   __string_rep_outer_loop

__string_rep_done_copy:
    MOV   R2, 0
    MOV   [R5], R2                ; null terminator
    MOV   R0, R3
    OR    R0, BOXED_RAMSTRING
    JMP   __string_rep_finish

__string_rep_empty:
    ;; R3 (heap pointer) is needed after the OOM check below.
    MOV   R3, 1
    PUSH  R3
    CALL  __malloc
    IADD  SP, 1
    MOV   R3, R0

    MOV   R2, R3
    IEQ   R2, 0
    JT    R2, __string_rep_oom

    MOV   R2, 0
    MOV   [R3], R2
    MOV   R0, R3
    OR    R0, BOXED_RAMSTRING
    JMP   __string_rep_finish

__string_rep_oom:
    MOV   R0, BOXED_NIL

__string_rep_finish:
    MOV   SP, BP
    POP   BP
    RET  


;; ===========================================================================
;; Built-in: string.reverse(s)
;; Incoming Stack: [BP+2] = string
;; Returns: R0 = new string (boxed RAM string) with byte order reversed.
;; Clobbers: R0-R6
;; ===========================================================================
    ;@fn __builtin_string_reverse clobbers=R0-R6
__builtin_string_reverse:
    PUSH  BP
    MOV   BP, SP

    MOV   R0, [BP+2]
    CALL  __unbox_string
    MOV   R1, R0                ; R1 = source string pointer (persist)

    PUSH  R1
    MOV   R0, [BP+2]           ; Arg 1: the BOXED string (NOT the pre-unboxed
    PUSH  R0                   ; pointer in R1 -- see string_len's header)
    CALL  __builtin_string_len
    IADD  SP, 1
    MOV   R4, R0
    CFI   R4
    POP   R1

    MOV   R2, R4
    IADD  R2, 1

    ;; __malloc clobbers R0-R3 and R6. R1 (source pointer) is read again
    ;; after this call, so it's preserved. R4 (length) is also read
    ;; again, but R4 is NOT in __malloc's clobber set, so it needs no
    ;; protection.
    PUSH  R1
    PUSH  R2
    CALL  __malloc
    IADD  SP, 1
    MOV   R3, R0                 ; R3 = destination heap pointer
    POP   R1                     ; restore source pointer

    ;; OOM check via scratch register R2 -- free here (reassigned fresh
    ;; immediately after anyway, on the very next line that matters). R3
    ;; must stay untouched: "MOV R6, R3" below needs the real heap
    ;; pointer.
    MOV   R2, R3
    IEQ   R2, 0
    JT    R2, __string_reverse_oom

    ;; write destination front-to-back from source back-to-front:
    ;; dest[k] = src[len-1-k]
    MOV   R5, R1
    IADD  R5, R4
    ISUB  R5, 1                  ; R5 = pointer to LAST char of source
    MOV   R6, R3                 ; R6 = write pointer (dest, forward)
    MOV   R2, R4                 ; R2 = remaining count

__string_reverse_loop:
    MOV   R0, R2
    IEQ   R0, 0
    JT    R0, __string_reverse_done_copy

    MOV   R0, [R5]
    MOV   [R6], R0
    ISUB  R5, 1
    IADD  R6, 1
    ISUB  R2, 1
    JMP   __string_reverse_loop

__string_reverse_done_copy:
    MOV   R0, 0
    MOV   [R6], R0                ; null terminator
    MOV   R0, R3
    OR    R0, BOXED_RAMSTRING
    JMP   __string_reverse_finish

__string_reverse_oom:
    MOV   R0, BOXED_NIL

__string_reverse_finish:
    MOV   SP, BP
    POP   BP
    RET  


;; ===========================================================================
;; Built-in: string.find(s, pattern) -- PLAIN SUBSTRING SEARCH ONLY
;; No Lua pattern-matching support (no magic characters interpreted; the
;; second argument is always a literal substring). No init/plain-flag
;; arguments. Only the start index is returned (no end index, no captures)
;; -- real Lua's find() returns two values here.
;; Incoming Stack: [BP+2] = string s, [BP+3] = substring to find
;; Returns: R0 = 1-based start index of the first match (as a Lua float),
;;          or BOXED_NIL if not found. An empty needle matches at index 1.
;; Clobbers: R0-R9
;; ===========================================================================
    ;@fn __builtin_string_find clobbers=R0-R9
__builtin_string_find:
    PUSH  BP
    MOV   BP, SP

    ;; Haystack pointer stored directly in R5 (not R1) -- __unbox_string's
    ;; own first instruction is "MOV R1, R0", so it would clobber R1 on
    ;; the second call below if the haystack pointer were stored there.
    ;; R5 is untouched by __unbox_string.
    MOV   R0, [BP+2]
    CALL  __unbox_string
    MOV   R5, R0                 ; R5 = haystack pointer (persist)

    MOV   R0, [BP+3]
    CALL  __unbox_string
    MOV   R2, R0                 ; R2 = needle pointer

    ;; Empty needle matches at position 1. (This IEQ is a fresh
    ;; loop-terminator check on a just-loaded byte, not an OOM check on a
    ;; value that needs to survive -- no bug here.)
    MOV   R3, [R2]
    IEQ   R3, 0
    JT    R3, __string_find_match_at_start

    MOV   R8, 1                  ; R8 = 1-based index of R5 within haystack

__string_find_outer:
    MOV   R3, [R5]
    IEQ   R3, 0
    JT    R3, __string_find_not_found

    MOV   R6, R5                 ; R6 = inner haystack cursor
    MOV   R7, R2                 ; R7 = inner needle cursor

__string_find_inner:
    MOV   R3, [R7]
    MOV   R9, R3
    IEQ   R9, 0
    JT    R9, __string_find_matched

    MOV   R4, [R6]
    MOV   R9, R4
    IEQ   R9, 0
    JT    R9, __string_find_advance_outer

    MOV   R9, R3
    INE   R9, R4
    JT    R9, __string_find_advance_outer

    IADD  R6, 1
    IADD  R7, 1
    JMP   __string_find_inner

__string_find_advance_outer:
    IADD  R5, 1
    IADD  R8, 1
    JMP   __string_find_outer

__string_find_matched:
    MOV   R0, R8
    CIF   R0
    JMP   __string_find_done

__string_find_match_at_start:
    MOV   R0, 1
    CIF   R0
    JMP   __string_find_done

__string_find_not_found:
    MOV   R0, BOXED_NIL

__string_find_done:
    MOV   SP, BP
    POP   BP
    RET  


;; ===========================================================================
;; Built-in: string.gsub(s, pattern, repl) -- PLAIN SUBSTITUTION ONLY
;; No Lua pattern-matching support. No init/n-limit arguments. Only the
;; resulting string is returned (real Lua's gsub also returns a
;; substitution count as a second value). An empty pattern is treated as
;; "no matches" (s returned unchanged) rather than Lua's zero-width
;; match-between-every-character semantics, to avoid an infinite loop.
;; Incoming Stack: [BP+2] = string s, [BP+3] = pattern, [BP+4] = repl
;; Returns: R0 = new string (boxed RAM string)
;; Clobbers: R0-R13
;;
;; Register map (held constant across both passes):
;;   R7  = s pointer (unboxed)          R3 = len(s)
;;   R8  = pattern pointer (unboxed)    R4 = len(pattern)
;;   R9  = repl pointer (unboxed)       R5 = len(repl)
;;   R6  = match count (Pass 1) / free scratch after Pass 1's use is done
;;   R13 = destination heap pointer
;; Everything else (R0-R2, R10-R12) is scratch, reused freely.
;; ===========================================================================
__builtin_string_gsub:
    PUSH  BP
    MOV   BP, SP

    MOV   R0, [BP+2]
    CALL  __unbox_string
    MOV   R7, R0                 ; R7 = s pointer (persist)

    MOV   R0, [BP+3]
    CALL  __unbox_string
    MOV   R8, R0                 ; R8 = pattern pointer (persist)

    MOV   R0, [BP+4]
    CALL  __unbox_string
    MOV   R9, R0                 ; R9 = repl pointer (persist)

    MOV   R0, [BP+2]           ; Arg 1: the BOXED string s (NOT the
    PUSH  R0                   ; pre-unboxed R7 -- see string_len's header)
    CALL  __builtin_string_len
    IADD  SP, 1
    MOV   R3, R0
    CFI   R3                     ; R3 = len(s)

    MOV   R0, [BP+3]           ; Arg 1: the BOXED pattern (NOT R8)
    PUSH  R0
    CALL  __builtin_string_len
    IADD  SP, 1
    MOV   R4, R0
    CFI   R4                     ; R4 = len(pattern)

    MOV   R1, R4
    IEQ   R1, 0
    JT    R1, __string_gsub_no_match_copy

    MOV   R0, [BP+4]           ; Arg 1: the BOXED repl (NOT R9)
    PUSH  R0
    CALL  __builtin_string_len
    IADD  SP, 1
    MOV   R5, R0
    CFI   R5                     ; R5 = len(repl)

    ;; --- Pass 1: count non-overlapping matches ---
    MOV   R0, R7
    MOV   R6, 0                  ; R6 = match count

__string_gsub_count_loop:
    MOV   R1, [R0]
    IEQ   R1, 0
    JT    R1, __string_gsub_count_done

    MOV   R10, R0
    MOV   R11, R8

__string_gsub_count_inner:
    MOV   R1, [R11]
    MOV   R2, R1
    IEQ   R2, 0
    JT    R2, __string_gsub_count_hit

    MOV   R12, [R10]
    MOV   R2, R12
    IEQ   R2, 0
    JT    R2, __string_gsub_count_advance

    MOV   R2, R1
    INE   R2, R12
    JT    R2, __string_gsub_count_advance

    IADD  R10, 1
    IADD  R11, 1
    JMP   __string_gsub_count_inner

__string_gsub_count_hit:
    IADD  R6, 1
    IADD  R0, R4
    JMP   __string_gsub_count_loop

__string_gsub_count_advance:
    IADD  R0, 1
    JMP   __string_gsub_count_loop

__string_gsub_count_done:
    MOV   R1, R6
    IEQ   R1, 0
    JT    R1, __string_gsub_no_match_copy

    MOV   R1, R5
    ISUB  R1, R4
    IMUL  R1, R6
    MOV   R2, R3
    IADD  R2, R1
    IADD  R2, 1
    PUSH  R2
    CALL  __malloc
    IADD  SP, 1
    MOV   R13, R0                ; R13 = destination heap pointer (persist)

    ;; FIXED: OOM check via scratch register R6 -- Pass 1's use of R6 (the
    ;; match count) is over by this point, and nothing in Pass 2 below
    ;; reads it, so it's free. R13 must stay untouched: "MOV R12, R13"
    ;; below needs the real heap pointer.
    MOV   R6, R13
    IEQ   R6, 0
    JT    R6, __string_gsub_oom

    ;; --- Pass 2: copy, substituting each match ---
    MOV   R0, R7
    MOV   R12, R13

__string_gsub_copy_loop:
    MOV   R1, [R0]
    IEQ   R1, 0
    JT    R1, __string_gsub_copy_done

    MOV   R10, R0
    MOV   R11, R8

__string_gsub_copy_inner:
    MOV   R1, [R11]
    MOV   R2, R1
    IEQ   R2, 0
    JT    R2, __string_gsub_copy_hit

    MOV   R1, [R10]
    MOV   R2, R1
    IEQ   R2, 0
    JT    R2, __string_gsub_copy_advance

    MOV   R2, [R11]
    INE   R2, R1
    JT    R2, __string_gsub_copy_advance

    IADD  R10, 1
    IADD  R11, 1
    JMP   __string_gsub_copy_inner

__string_gsub_copy_hit:
    MOV   R10, R9

;; Register note: R2 is free here -- by the time this loop runs, the
;; matching-inner-loop code that also uses R2 as scratch has already
;; exited (this label is only reached via that loop's own JT branch), and
;; nothing in this loop or after it depends on R2's prior value.

__string_gsub_copy_write_repl:
    MOV   R1, [R10]
    ;; FIXED: scratch copy (R2) for the nil-check, instead of testing R1
    ;; directly. R2 is free here -- see note above.
    MOV   R2, R1
    IEQ   R2, 0
    JT    R2, __string_gsub_copy_hit_done
    MOV   [R12], R1
    IADD  R10, 1
    IADD  R12, 1
    JMP   __string_gsub_copy_write_repl
__string_gsub_copy_hit_done:
    IADD  R0, R4
    JMP   __string_gsub_copy_loop

__string_gsub_copy_advance:
    MOV   R1, [R0]
    MOV   [R12], R1
    IADD  R0, 1
    IADD  R12, 1
    JMP   __string_gsub_copy_loop

__string_gsub_copy_done:
    MOV   R1, 0
    MOV   [R12], R1
    MOV   R0, R13
    OR    R0, BOXED_RAMSTRING
    JMP   __string_gsub_finish

__string_gsub_no_match_copy:
    MOV   R0, [BP+2]
    CALL  __unbox_string
    MOV   R7, R0

    MOV   R0, [BP+2]           ; Arg 1: the BOXED string (NOT the
    PUSH  R0                   ; pre-unboxed R7 -- see string_len's header)
    CALL  __builtin_string_len
    IADD  SP, 1
    MOV   R3, R0
    CFI   R3

    MOV   R1, R3
    IADD  R1, 1
    PUSH  R1
    CALL  __malloc
    IADD  SP, 1
    MOV   R13, R0

    ;; FIXED: OOM check via scratch register R6 -- free in this path too
    ;; (nothing here uses R6 at all). R13 must stay untouched: "MOV R12,
    ;; R13" below needs the real heap pointer.
    MOV   R6, R13
    IEQ   R6, 0
    JT    R6, __string_gsub_oom

    MOV   R0, R7
    MOV   R12, R13
__string_gsub_plain_copy_loop:
    MOV   R1, [R0]
    MOV   [R12], R1
    IEQ   R1, 0
    JT    R1, __string_gsub_plain_copy_done
    IADD  R0, 1
    IADD  R12, 1
    JMP   __string_gsub_plain_copy_loop
__string_gsub_plain_copy_done:
    MOV   R0, R13
    OR    R0, BOXED_RAMSTRING
    JMP   __string_gsub_finish

__string_gsub_oom:
    MOV   R0, BOXED_NIL

__string_gsub_finish:
    MOV   SP, BP
    POP   BP
    RET  

;; ===========================================================================
;; String methods: s:name(...) support
;; ---------------------------------------------------------------------------
;; __builtin_string_method_lookup: [BP+2] = key -> R0 = boxed function for
;; a known string-library name, or BOXED_NIL. Called by __builtin_table_get
;; when the "table" is a string. Saves everything but R0.
;;
;; __strmeth_*: Lua-ABI wrappers ([BP+2] = self string, [BP+3].. = args;
;; the compiler NIL-pads method calls to these names, see
;; string_method_arity() in function.c) that forward to the existing
;; __builtin_string_* routines, which take the same [BP+2].. layout.
;; format() is variadic and is not offered as a method.
;; ===========================================================================
__strmeth_name_len:
    string "len"
__strmeth_name_sub:
    string "sub"
__strmeth_name_upper:
    string "upper"
__strmeth_name_lower:
    string "lower"
__strmeth_name_rep:
    string "rep"
__strmeth_name_byte:
    string "byte"
__strmeth_name_find:
    string "find"
__strmeth_name_reverse:
    string "reverse"
__strmeth_name_gsub:
    string "gsub"

    ;@fn __builtin_string_method_lookup clobbers=R0,R3-R4
__builtin_string_method_lookup:
    PUSH  BP
    MOV   BP, SP
    PUSH  R1
    PUSH  R2
    MOV   R1, __strmeth_name_len
    CALL  __strmeth_key_is
    JF    R0, __strmeth_try_after_len
    MOV   R0, __strmeth_len
    OR    R0, BOXED_FUNCTION
    JMP   __strmeth_lookup_done
__strmeth_try_after_len:
    MOV   R1, __strmeth_name_sub
    CALL  __strmeth_key_is
    JF    R0, __strmeth_try_after_sub
    MOV   R0, __strmeth_sub
    OR    R0, BOXED_FUNCTION
    JMP   __strmeth_lookup_done
__strmeth_try_after_sub:
    MOV   R1, __strmeth_name_upper
    CALL  __strmeth_key_is
    JF    R0, __strmeth_try_after_upper
    MOV   R0, __strmeth_upper
    OR    R0, BOXED_FUNCTION
    JMP   __strmeth_lookup_done
__strmeth_try_after_upper:
    MOV   R1, __strmeth_name_lower
    CALL  __strmeth_key_is
    JF    R0, __strmeth_try_after_lower
    MOV   R0, __strmeth_lower
    OR    R0, BOXED_FUNCTION
    JMP   __strmeth_lookup_done
__strmeth_try_after_lower:
    MOV   R1, __strmeth_name_rep
    CALL  __strmeth_key_is
    JF    R0, __strmeth_try_after_rep
    MOV   R0, __strmeth_rep
    OR    R0, BOXED_FUNCTION
    JMP   __strmeth_lookup_done
__strmeth_try_after_rep:
    MOV   R1, __strmeth_name_byte
    CALL  __strmeth_key_is
    JF    R0, __strmeth_try_after_byte
    MOV   R0, __strmeth_byte
    OR    R0, BOXED_FUNCTION
    JMP   __strmeth_lookup_done
__strmeth_try_after_byte:
    MOV   R1, __strmeth_name_find
    CALL  __strmeth_key_is
    JF    R0, __strmeth_try_after_find
    MOV   R0, __strmeth_find
    OR    R0, BOXED_FUNCTION
    JMP   __strmeth_lookup_done
__strmeth_try_after_find:
    MOV   R1, __strmeth_name_reverse
    CALL  __strmeth_key_is
    JF    R0, __strmeth_try_after_reverse
    MOV   R0, __strmeth_reverse
    OR    R0, BOXED_FUNCTION
    JMP   __strmeth_lookup_done
__strmeth_try_after_reverse:
    MOV   R1, __strmeth_name_gsub
    CALL  __strmeth_key_is
    JF    R0, __strmeth_try_after_gsub
    MOV   R0, __strmeth_gsub
    OR    R0, BOXED_FUNCTION
    JMP   __strmeth_lookup_done
__strmeth_try_after_gsub:
    MOV   R0, BOXED_NIL
__strmeth_lookup_done:
    POP   R2
    POP   R1
    MOV   SP, BP
    POP   BP
    RET  

;; __strmeth_key_is (internal): R1 = raw ROM address of a name; compares it
;; with the lookup key at [BP+2] of the CALLER's frame -> R0 = 1 / 0
    ;@fn __strmeth_key_is clobbers=R0-R4
__strmeth_key_is:
    OR    R1, BOXED_ROMSTRING
    MOV   R2, [BP+2]
    PUSH  R2                      ; left  -> [BP+3]
    PUSH  R1                      ; right -> [BP+2]
    CALL  __builtin_eq
    IADD  SP, 2
    IEQ   R0, BOXED_TRUE
    RET  

__strmeth_len:
    PUSH  BP
    MOV   BP, SP
    MOV   R0, [BP+2]
    PUSH  R0
    CALL  __builtin_string_len
    MOV   SP, BP
    POP   BP
    RET  
__strmeth_upper:
    PUSH  BP
    MOV   BP, SP
    MOV   R0, [BP+2]
    PUSH  R0
    CALL  __builtin_string_upper
    MOV   SP, BP
    POP   BP
    RET  
__strmeth_lower:
    PUSH  BP
    MOV   BP, SP
    MOV   R0, [BP+2]
    PUSH  R0
    CALL  __builtin_string_lower
    MOV   SP, BP
    POP   BP
    RET  
__strmeth_reverse:
    PUSH  BP
    MOV   BP, SP
    MOV   R0, [BP+2]
    PUSH  R0
    CALL  __builtin_string_reverse
    MOV   SP, BP
    POP   BP
    RET  
__strmeth_rep:
    PUSH  BP
    MOV   BP, SP
    MOV   R0, [BP+3]
    PUSH  R0
    MOV   R0, [BP+2]
    PUSH  R0
    CALL  __builtin_string_rep
    MOV   SP, BP
    POP   BP
    RET  
__strmeth_find:
    PUSH  BP
    MOV   BP, SP
    MOV   R0, [BP+3]
    PUSH  R0
    MOV   R0, [BP+2]
    PUSH  R0
    CALL  __builtin_string_find
    MOV   SP, BP
    POP   BP
    RET  
__strmeth_sub:
    PUSH  BP
    MOV   BP, SP
    MOV   R0, [BP+4]
    PUSH  R0
    MOV   R0, [BP+3]
    PUSH  R0
    MOV   R0, [BP+2]
    PUSH  R0
    CALL  __builtin_string_sub
    MOV   SP, BP
    POP   BP
    RET  
__strmeth_byte:
    PUSH  BP
    MOV   BP, SP
    MOV   R0, [BP+4]
    PUSH  R0
    MOV   R0, [BP+3]
    PUSH  R0
    MOV   R0, [BP+2]
    PUSH  R0
    CALL  __builtin_string_byte
    MOV   SP, BP
    POP   BP
    RET  
__strmeth_gsub:
    PUSH  BP
    MOV   BP, SP
    MOV   R0, [BP+4]
    PUSH  R0
    MOV   R0, [BP+3]
    PUSH  R0
    MOV   R0, [BP+2]
    PUSH  R0
    CALL  __builtin_string_gsub
    MOV   SP, BP
    POP   BP
    RET  

;; ===========================================================================
;; __builtin_tonumber_base: tonumber(s, base), as Lua 5.4's (b_str2int):
;; s must be a string and base 2..36; leading/trailing whitespace, one
;; optional '-', then at least one digit 0-9 / a-z / A-Z below base, and
;; nothing else ("0x10" in base 16 is nil). Anything else gives nil.
;;   [BP+2] = base   [BP+3] = s      (base pushed first: argument order)
;; Returns R0 = the number or nil. Preserves R1-R7.
;; ===========================================================================
    ;@fn __builtin_tonumber_base clobbers=R0
__builtin_tonumber_base:
    PUSH  BP
    MOV   BP, SP
    PUSH  R1
    PUSH  R2
    PUSH  R3
    PUSH  R4
    PUSH  R5
    PUSH  R6
    PUSH  R7

    ;; --- base: a number, 2..36 ---
    MOV   R5, [BP+2]
    MOV   R1, R5
    AND   R1, NAN_VALUE
    IEQ   R1, NAN_VALUE
    JT    R1, __tnb_nil               ; not a number
    CFI   R5
    MOV   R1, R5
    ILT   R1, 2
    JT    R1, __tnb_nil
    MOV   R1, R5
    IGT   R1, 36
    JT    R1, __tnb_nil
    MOV   R6, R5
    CIF   R6                          ; R6 = base as a float

    ;; --- s: a string ---
    MOV   R0, [BP+3]
    MOV   R1, R0
    AND   R1, BOXED_DATA
    IEQ   R1, BOXED_ROMSTRING
    JT    R1, __tnb_is_string
    MOV   R1, R0
    AND   R1, BOXED_DATA
    IEQ   R1, BOXED_RAMSTRING
    JF    R1, __tnb_nil
    MOV   R1, R0
    AND   R1, BOXED_PAYLOAD
    ILT   R1, 4
    JT    R1, __tnb_nil               ; nil / false / true
__tnb_is_string:
    CALL  __unbox_string              ; R0 = char pointer

    CALL  __tnb_skip_space
    MOV   R3, 0                       ; negative?
    MOV   R1, [R0]
    IEQ   R1, 45                      ; '-'
    JF    R1, __tnb_digits
    MOV   R3, 1
    IADD  R0, 1
__tnb_digits:
    MOV   R4, 0.0                     ; value
    MOV   R2, 0                       ; digits seen
__tnb_loop:
    MOV   R1, [R0]
    MOV   R7, R1
    ISUB  R7, 48                      ; '0'..'9'
    MOV   R1, R7
    ILT   R1, 0
    JT    R1, __tnb_end
    MOV   R1, R7
    ILT   R1, 10
    JT    R1, __tnb_have_digit
    MOV   R7, [R0]
    OR    R7, 32                      ; lower-case a letter
    ISUB  R7, 87                      ; 'a' -> 10
    MOV   R1, R7
    ILT   R1, 10
    JT    R1, __tnb_end
    MOV   R1, R7
    IGT   R1, 35
    JT    R1, __tnb_end
__tnb_have_digit:
    MOV   R1, R7
    IGE   R1, R5                      ; digit >= base: not part of the number
    JT    R1, __tnb_end
    FMUL  R4, R6
    CIF   R7
    FADD  R4, R7
    IADD  R2, 1
    IADD  R0, 1
    JMP   __tnb_loop
__tnb_end:
    JF    R2, __tnb_nil               ; no digits
    CALL  __tnb_skip_space
    MOV   R1, [R0]
    JT    R1, __tnb_nil               ; something else follows
    JF    R3, __tnb_done
    FSGN  R4
__tnb_done:
    MOV   R0, R4
    JMP   __tnb_ret
__tnb_nil:
    MOV   R0, BOXED_NIL
__tnb_ret:
    POP   R7
    POP   R6
    POP   R5
    POP   R4
    POP   R3
    POP   R2
    POP   R1
    MOV   SP, BP
    POP   BP
    RET  

;; skips whitespace (space, \t \n \v \f \r) at R0; clobbers R1
    ;@fn __tnb_skip_space clobbers=R0-R1
__tnb_skip_space:
    MOV   R1, [R0]
    IEQ   R1, 32
    JT    R1, __tnb_skip_one
    MOV   R1, [R0]
    ISUB  R1, 9
    ILT   R1, 0
    JT    R1, __tnb_skip_done
    MOV   R1, [R0]
    IGT   R1, 13
    JT    R1, __tnb_skip_done
__tnb_skip_one:
    IADD  R0, 1
    JMP   __tnb_skip_space
__tnb_skip_done:
    RET  

;; ---------------------------------------------------------------------------
;; __arith_coerce: R0 = an arithmetic operand that is not a number (the
;; compiler only calls this for NaN-boxed values). A numeric string becomes
;; its number, as Lua and PICO-8 do ("12" + 1 == 13, ("0x".."ff") + 0 ==
;; 255); anything else is returned unchanged. Preserves R1-R13.
;; ---------------------------------------------------------------------------
    ;@fn __arith_coerce clobbers=R0
__arith_coerce:
    PUSH  R1
    PUSH  R2
    PUSH  R3
    PUSH  R4
    PUSH  R5
    PUSH  R6
    PUSH  R7
    PUSH  R8
    PUSH  R9
    PUSH  R10
    PUSH  R11
    PUSH  R12
    PUSH  R13
    PUSH  R0
    PUSH  R0
    CALL  __builtin_string_to_number
    IADD  SP, 1
    POP   R1                      ; the original value
    MOV   R2, R0
    IEQ   R2, BOXED_NIL
    JF    R2, __arith_coerce_done
    MOV   R0, R1                  ; not a numeric string: unchanged
__arith_coerce_done:
    POP   R13
    POP   R12
    POP   R11
    POP   R10
    POP   R9
    POP   R8
    POP   R7
    POP   R6
    POP   R5
    POP   R4
    POP   R3
    POP   R2
    POP   R1
    RET  
;; ===========================================================================
;; SECTION: PRINT OPERATIONS
;; ===========================================================================

;; ---------------------------------------------------------------------------
;; Built-in: Print to Screen at Position (x, y) with Split-Tag Coercion
;; Incoming Stack: [BP+4] = X Coordinate (integer)
;;                 [BP+3] = Y Coordinate (integer)
;;                 [BP+2] = Target Value (tagged)
;; ---------------------------------------------------------------------------
__builtin_print:
    PUSH  BP
    MOV   BP, SP

    ;; 1. Initialize GPU Texture/Region state
    IN    R5, GPU_SelectedTexture ; Save current texture
    PUSH  R5
    IN    R6, GPU_SelectedRegion  ; Save current region
    PUSH  R6
    OUT   GPU_SelectedTexture, -1 ; Set BIOS font texture

    ;; 2. Load Parameters
    MOV   R1, [BP+4]          ; R1 = X Pixel Coordinate (Integer)
    MOV   R2, [BP+3]          ; R2 = Y Pixel Coordinate (Integer)
    MOV   R3, [BP+2]          ; R3 = Target Value to Print

__print_check_tag:
    ;; 3. SAFETY COERCION LAYER: Check for ROM vs RAM String tags
    MOV   R4, R3              ; Copy target value to scratch register R4
    AND   R4, BOXED_DATA      ; Isolate upper 10 bits (Tag)

    IEQ   R4, BOXED_ROMSTRING      ; Is it a ROM String Literal?
    JT    R4, __print_unbox_rom

    MOV   R4, R3              ; Copy target value to scratch register R4
    AND   R4, BOXED_DATA      ; Isolate upper 10 bits (Tag)

    ;; Check if it's a RAM Heap String (Tag 0xFFC0xxxx with address >= 4)
    IEQ   R4, BOXED_RAMSTRING ; Does it have the RAM String / Primitive tag?
    JF    R4, __print_coerce  ; If neither string tag, coerce!

    ;; It is BOXED_DATA. Make sure it's not Nil (0), False (1), or True (2)!
    MOV   R4, R3
    AND   R4, BOXED_PAYLOAD      ; Isolate payload
    ILT   R4, 4               ; Is payload < 4 (Nil/False/True)?
    JT    R4, __print_coerce  ; If < 4, it's a boolean/nil -> coerce!

__print_unbox_ram:
    ;; 4a. Unbox RAM String: Keep raw 22-bit heap address (4MW limit)
    AND   R3, BOXED_PAYLOAD      ; Isolate 22-bit raw RAM heap pointer
    JMP   __print_dispatch

__print_unbox_rom:
    ;; 4b. Unbox ROM String: Isolate offset and add ROM page bit
    AND   R3, BOXED_PAYLOAD      ; Isolate up to 27-bit raw ROM offset
    OR    R3, V32_CART_PAGE      ; Restore Vircon32 CART page bit

__print_dispatch:
    ;; 5. Dispatch Unboxed Pointer to BIOS
    PUSH  R3                  ; Push Unboxed Raw String Pointer
    PUSH  R2                  ; Push Y Coordinate
    PUSH  R1                  ; Push X Coordinate
    CALL  __bios_print_text   ; Draw the string directly to the GPU screen
    IADD  SP, 3               ; Clean up arguments from stack

    ;; 6. Restore previous GPU texture and region
    POP   R6
    POP   R5
    OUT   GPU_SelectedTexture, R5 ; Restore previous texture
    OUT   GPU_SelectedRegion, R6  ; Restore previous region

    MOV   SP, BP
    POP   BP
    RET  

__print_coerce:
    PUSH  R6                  ; Save GPU_SelectedRegion
    PUSH  R5                  ; Save GPU_SelectedTexture
    PUSH  R2                  ; Preserve Y coordinate
    PUSH  R1                  ; Preserve X coordinate
    PUSH  R3                  ; Push non-string value as argument
    CALL  __builtin_tostring_scratch_a  ; R0 = Tagged String result.
                                          ; Was __builtin_tostring: the
                                          ; coerced string is drawn to the
                                          ; GPU by __bios_print_text right
                                          ; below and never referenced
                                          ; again, so it never needs its
                                          ; own permanent allocation. Only
                                          ; ONE buffer is ever needed here
                                          ; -- print() coerces and draws
                                          ; one value at a time -- so "A"
                                          ; alone (shared with
                                          ; string.format()'s numeric
                                          ; specifiers) is safe.

    POP   R3                  ; Discard the argument (was pushed before CALL)
    POP   R1                  ; Restore X
    POP   R2                  ; Restore Y
    POP   R5                  ; Restore GPU_SelectedTexture
    POP   R6                  ; Restore GPU_SelectedRegion
    MOV   R3, R0              ; Move result to R3
    JMP   __print_check_tag

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; __bios_print_text: Displays ASCII string content to the GPU screen 
;;
;;     Incoming Stack: [BP+2] = X
;;                     [BP+3] = Y
;;                     [BP+4] = Unboxed Heap String Pointer 
;;
    ;@fn __bios_print_text clobbers=R1-R4
__bios_print_text:
    PUSH  BP
    MOV   BP, SP

    ;; Load Parameters from Stack 
    MOV   R1, [BP+2]              ; R1 = X Pixel Coordinate (Integer) 
    MOV   R2, [BP+3]              ; R2 = Y Pixel Coordinate (Integer) 
    MOV   R3, [BP+4]              ; R3 = Heap Offset to unboxed ASCII String

__bios_print_loop:
    MOV   R4, [R3]                ; Read character from string memory 
    OUT   GPU_SelectedRegion, R4  ; set character to display 
    IEQ   R4, 0                   ; Check for null terminator 
    JT    R4, __bios_print_done

    ;; Display character to screen 
    OUT   GPU_DrawingPointX, R1   ; display at X 
    OUT   GPU_DrawingPointY, R2   ; display at Y 
    OUT   GPU_Command, GPUCommand_DrawRegion ; display to screen 
         
    ;; Advance to next character and increment X coordinate 
    IADD  R3, 1                   ; Next char word in memory 
    IADD  R1, 10                  ; Advance X by font width (e.g., 10 pixels) 
    JMP   __bios_print_loop

__bios_print_done:

    MOV   SP, BP
    POP   BP
    RET  

;; ===========================================================================
;; Built-in: next(table, key) - Stateless iterator used by pairs()
;;
;; Incoming Stack (post-FIX-3): [BP+2] = Tagged Table Pointer (state, now
;; the first formal parameter to match how ordinary Lua functions receive
;; their parameters -- see node_for_generic()'s FIX 3 comment in v32lua.c),
;; [BP+3] = Current Key (or NIL for first call).
;; Returns: R0 = key, R2 = value (or R0 = NIL when exhausted)
;; ===========================================================================
    ;@fn __builtin_next clobbers=R0,R2
__builtin_next:
    ;; Order: the array part (keys 1..capacity with a value), then the hash
    ;; part's slots in slot order. See "TABLE STORAGE" in table.s.
    PUSH  BP
    MOV   BP, SP
    PUSH  R1
    PUSH  R3
    PUSH  R4
    PUSH  R5
    PUSH  R6
    PUSH  R7
    PUSH  R8

    MOV   R1, [BP+2]          ; R1 = Tagged Table Pointer (state)
    MOV   R2, [BP+3]          ; R2 = Current Key (or NIL for first call)
    MOV   R3, R1
    AND   R3, BOXED_DATA
    IEQ   R3, BOXED_TABLE
    JF    R3, __next_error_not_table
    AND   R1, BOXED_PAYLOAD

    MOV   R7, [R1]
    AND   R7, TABLE_ARRAYSIZE ; R7 = array capacity
    MOV   R6, 0               ; R6 = array index to resume at (0-based)
    MOV   R3, R2
    IEQ   R3, BOXED_NIL
    JT    R3, __next_array
    ;; a whole-number key inside the array part?
    MOV   R3, R2
    AND   R3, NAN_VALUE
    IEQ   R3, NAN_VALUE
    JT    R3, __next_from_hash
    MOV   R3, R2
    CFI   R3
    MOV   R4, R3
    CIF   R4
    INE   R4, R2
    JT    R4, __next_from_hash
    MOV   R4, R3
    ILT   R4, 1
    JT    R4, __next_from_hash
    MOV   R4, R3
    IGT   R4, R7
    JT    R4, __next_from_hash
    MOV   R6, R3              ; resume after key k -> index k

__next_array:
    MOV   R8, [R1+2]
__next_array_loop:
    MOV   R3, R6
    IGE   R3, R7
    JT    R3, __next_hash_start
    MOV   R3, R8
    IADD  R3, R6
    MOV   R2, [R3]
    MOV   R3, R2
    IEQ   R3, BOXED_NIL
    JF    R3, __next_array_hit
    IADD  R6, 1
    JMP   __next_array_loop
__next_array_hit:
    MOV   R0, R6
    IADD  R0, 1
    CIF   R0                  ; key = index + 1 (value already in R2)
    JMP   __next_done

__next_from_hash:
    MOV   R5, [R1+3]
    MOV   R3, R5
    IEQ   R3, 0
    JT    R3, __next_done_nil
    MOV   R3, R2
    IEQ   R3, 0x80000000      ; -0 == 0
    JF    R3, __next_find
    MOV   R2, 0
__next_find:
    CALL  __table_hash_find   ; R0 = slot address of the current key, or 0
    MOV   R3, R0
    IEQ   R3, 0
    JT    R3, __next_done_nil
    ISUB  R0, R5
    ISUB  R0, 2
    SHL   R0, -1
    MOV   R6, R0
    IADD  R6, 1               ; resume at the following slot
    JMP   __next_hash_loop_init

__next_hash_start:
    MOV   R6, 0
__next_hash_loop_init:
    MOV   R5, [R1+3]
    MOV   R3, R5
    IEQ   R3, 0
    JT    R3, __next_done_nil
    MOV   R7, [R5]            ; hash capacity
__next_hash_loop:
    MOV   R3, R6
    IGE   R3, R7
    JT    R3, __next_done_nil
    MOV   R4, R6
    SHL   R4, 1
    IADD  R4, R5
    IADD  R4, 2               ; slot address
    MOV   R0, [R4]
    MOV   R3, R0
    IEQ   R3, BOXED_NIL
    JT    R3, __next_hash_next
    MOV   R2, [R4+1]
    MOV   R3, R2
    IEQ   R3, BOXED_NIL
    JF    R3, __next_done       ; live entry: R0 = key, R2 = value
__next_hash_next:
    IADD  R6, 1
    JMP   __next_hash_loop

__next_done_nil:
    MOV   R0, BOXED_NIL

__next_done:
    POP   R8
    POP   R7
    POP   R6
    POP   R5
    POP   R4
    POP   R3
    POP   R1
    MOV   SP, BP
    POP   BP
    RET  

__next_error_not_table:
    HLT  
    JMP   __next_error_not_table

;; ===========================================================================
;; Built-in: ipairs_iter(t, index) - Numeric iterator for ipairs()
;;
;; Incoming Stack (post-FIX-3): [BP+2] = Tagged Table Pointer (state, now
;; the first formal parameter, matching node_for_generic()'s FIX 3),
;; [BP+3] = Current Index.
;; Returns: R0 = next index (boxed float, or BOXED_NIL when done),
;;          R2 = value at that index (2-value return via R0/R2, matching
;;          this compiler's standard multi-return convention -- see
;;          node_return() in v32lua.c)
;; Register Usage: R1-R7
;;
;; IMPLEMENTATION NOTE: fetches the value via __builtin_table_get rather
;; than reading through the table's array-data-pointer header field
;; directly. That field is always null in this runtime (real array
;; allocation is still a __builtin_table_set_reallocate TODO), so the
;; previous direct-array-read version returned garbage for every table
;; with a nonzero tracked length.
;; ===========================================================================
__builtin_ipairs_iter:
    PUSH  BP
    MOV   BP, SP

    ;; --- Callee-Save ---
    PUSH  R1
    PUSH  R3
    PUSH  R4
    PUSH  R5
    PUSH  R6
    PUSH  R7

    ;; --- Load arguments ---
    MOV   R1, [BP+2]          ; R1 = Tagged Table Pointer (kept BOXED for table_get)
    MOV   R2, [BP+3]          ; R2 = Current Index (boxed float, or NIL for first call)

    ;; --- Validate table ---
    MOV   R3, R1
    AND   R3, BOXED_DATA
    IEQ   R3, BOXED_TABLE
    JF    R3, __ipairs_iter_error

    MOV   R4, R1
    AND   R4, BOXED_PAYLOAD   ; R4 = raw table header address (R1 stays boxed)

    ;; --- Get tracked contiguous array length (Word 1) ---
    MOV   R5, [R4+1]

    ;; --- Compute next index into R6 ---
    MOV   R6, R2
    IEQ   R6, BOXED_NIL
    JT    R6, __ipairs_iter_start

    MOV   R6, R2
    CFI   R6
    IADD  R6, 1                ; Next index

    JMP   __ipairs_iter_check_bounds

__ipairs_iter_start:
    MOV   R6, 1                ; First call: start at index 1

__ipairs_iter_check_bounds:
    MOV   R7, R6
    IGT   R7, R5
    JT    R7, __ipairs_iter_done_nil   ; Next index exceeds length -> done

    ;; --- Fetch value at index R6 via __builtin_table_get ---
    MOV   R7, R6
    CIF   R7                   ; R7 = boxed float key
    PUSH  R1                   ; Arg1: Table Pointer (boxed)
    PUSH  R7                   ; Arg2: Key
    CALL  __builtin_table_get
    IADD  SP, 2

    MOV   R2, R0                ; R2 = fetched value (2nd return value)
    MOV   R0, R6
    CIF   R0                    ; R0 = next index as boxed float (1st return value)
    JMP   __ipairs_iter_done

__ipairs_iter_done_nil:
    MOV   R0, BOXED_NIL

__ipairs_iter_done:
    ;; --- Callee-Restore ---
    POP   R7
    POP   R6
    POP   R5
    POP   R4
    POP   R3
    POP   R1

    MOV   SP, BP
    POP   BP
    RET  

__ipairs_iter_error:
    HLT  
    JMP   __ipairs_iter_error

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Vircon32 Native Runtime Subroutines
;; spr(), btn(), btnp() implementations for v32lua compiler
;;
;; These subroutines are called by the compiler-generated code when
;; neither PICO-8 nor TIC-80 compatibility modes are enabled.
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; ============================================================================
;; Button State Tracking Memory (for btnp edge detection)
;; ============================================================================
;; We need to track the previous frame's button state for each button on each
;; gamepad to detect edge presses.
;;
;; This CANNOT be a statically-declared data label in this file. Everything
;; assembled from this source -- code and any ".fill"/"integer"-style static
;; data alike -- ends up in the cartridge's ROM image at V32_CART_PAGE
;; (0x20000000+), which is read-only. A previous version of this file
;; declared __vircon32_btn_prev_state as static data right here, which
;; assembled fine but faulted the first time __builtin_vircon32_btnp below
;; tried to write to it at runtime (MOV [Rd], Rs against a ROM address).
;;
;; An earlier fix for that routed this through a lazy __malloc the first
;; time btnp() ran (mirroring __builtin_table_new's lazy-allocate pattern).
;; That's the right shape for something whose size depends on what the Lua
;; program does at runtime -- but this table's size is fixed at compile
;; time (always exactly 44 words, known before a single byte of Lua runs),
;; so heap allocation was solving a problem this doesn't have: it added an
;; OOM path, a register just to hold the pointer, and a pointer indirection
;; that a compile-time-fixed table never needed. This version instead
;; reserves 44 words of RAM directly in the compiler's fixed global layout
;; -- the same mechanism HEAP_POINTER/FTOA_SCRATCH_PTR_A/B already use,
;; just a wider slice of it. That requires two small additions on the
;; compiler side (v32lua.c) -- no malloc/heap involvement at all:
;;
;;   1. Add a global to remember the reserved base address, near
;;      RuntimeRequirements/CompilerConfig (context.h/.c):
;;          int vircon32_btn_prev_state_base = -1; // -1 = not reserved
;;
;;   2. Reserve the 44 words when this mode is active -- in the semantic
;;      analyzer stage, alongside the existing TIC80_MAP_BUFFER_PTR /
;;      PICO8_MAP_BUFFER_PTR reservations (~v32lua.c:11705-11721), BEFORE
;;      register_all_globals_prepass() runs so user globals land after it:
;;          if (runtime_req.needs_vircon32)
;;          {
;;              vircon32_btn_prev_state_base = next_ram_address;
;;              next_ram_address             = next_ram_address + 44;
;;          }
;;
;;   3. Emit its %define -- in emit_variable_map(), right after
;;      FTOA_SCRATCH_PTR_B (~v32lua.c:1890):
;;          if (vircon32_btn_prev_state_base != -1) {
;;              fprintf (out(), "%%define  VIRCON32_BTN_PREV_STATE  0x%.8X\n",
;;                       vircon32_btn_prev_state_base);
;;          }
;;
;;   4. Zero-initialize it at startup -- in generate_global_setup(), right
;;      after the FTOA_SCRATCH_PTR_A/B zero-inits (~v32lua.c:3020). Unlike
;;      those two (which MUST start at exactly 0 because a lazy-allocate
;;      check reads them), this is just for a clean first frame -- a stale
;;      nonzero value here would only cost btnp() one wrong edge on frame
;;      0, never a crash -- so skipping this step is a correctness nicety,
;;      not a safety requirement, if you'd rather not spend the few extra
;;      instructions:
;;          if (vircon32_btn_prev_state_base != -1) {
;;              emit_asm ("MOV R1, VIRCON32_BTN_PREV_STATE ; zero-init btnp() prev-state table\n");
;;              emit_asm ("MOV R2, 44\n");
;;              emit_asm ("MOV R3, 0.0\n");
;;              emit_asm ("__vircon32_btn_prev_state_zero_loop:\n");
;;              emit_asm ("JF R2, __vircon32_btn_prev_state_zero_done\n");
;;              emit_asm ("MOV [R1], R3\n");
;;              emit_asm ("IADD R1, 1\n");
;;              emit_asm ("ISUB R2, 1\n");
;;              emit_asm ("JMP __vircon32_btn_prev_state_zero_loop\n");
;;              emit_asm ("__vircon32_btn_prev_state_zero_done:\n");
;;          }
;;
;; Layout: 44 words (4 gamepads * 11 buttons, one word each) starting at
;; VIRCON32_BTN_PREV_STATE + (gamepad * 11 + button). Each entry stores 1.0
;; if that button was pressed last frame, 0.0 if not (floats, to match
;; Lua's number representation in v32lua).
;;
;; ============================================================================
;; Channel-Ownership Tracking Memory (for music.volume()/sfx.volume()'s
;; no-channel "apply to every channel I've used" mode)
;; ============================================================================
;; Two words, VIRCON32_MUSIC_CHANNEL_MASK and VIRCON32_SFX_CHANNEL_MASK,
;; each a 16-bit-relevant bitmask over channels 0-15: bit N set means
;; channel N was last assigned a sound by that namespace's .play(). Written
;; by __builtin_vircon32_play/_sfx_play below (and by the compile-time
;; equivalent in the static-fold path, emit_vircon32_claim_channel_static()
;; in v32lua.c) every time a channel is claimed; read by
;; __builtin_vircon32_volume_mask when music.volume(VOL)/sfx.volume(VOL) is
;; called with no channel argument. These are ordinary fixed RAM words, not
;; a heap allocation -- same reasoning as VIRCON32_BTN_PREV_STATE above:
;; their size (1 word each) is fixed at compile time, so a malloc/heap
;; indirection would be solving a problem this doesn't have.
;;
;; Requires four small additions on the compiler side (v32lua.c), mirroring
;; the VIRCON32_BTN_PREV_STATE additions above:
;;
;;   1. Two globals, in context.c near vircon32_sfx_cursor_base:
;;          int vircon32_music_channel_mask_base = -1;
;;          int vircon32_sfx_channel_mask_base   = -1;
;;
;;   2. Reserve one word each, in main.c, in the same
;;      "if (runtime_req.needs_vircon32)" block that reserves
;;      VIRCON32_BTN_PREV_STATE/VIRCON32_SFX_CURSOR, BEFORE
;;      register_all_globals_prepass() runs:
;;          vircon32_music_channel_mask_base = next_ram_address;
;;          next_ram_address                 = next_ram_address + 1;
;;          vircon32_sfx_channel_mask_base    = next_ram_address;
;;          next_ram_address                  = next_ram_address + 1;
;;
;;   3. Emit their %defines -- in emit_variable_map() (emit.c), right after
;;      VIRCON32_SFX_CURSOR:
;;          fprintf (out(), "%%define  VIRCON32_MUSIC_CHANNEL_MASK 0x%.8X\n",
;;                   vircon32_music_channel_mask_base);
;;          fprintf (out(), "%%define  VIRCON32_SFX_CHANNEL_MASK   0x%.8X\n",
;;                   vircon32_sfx_channel_mask_base);
;;
;;   4. Zero-initialize both at startup -- in generate_global_setup()
;;      (generator.c), right after the VIRCON32_SFX_CURSOR init. UNLIKE the
;;      btnp prev-state table's zero-init (a correctness nicety), this one
;;      matters: a stale nonzero mask would make music.volume()/
;;      sfx.volume()'s no-channel mode apply volume to channels nothing
;;      has actually played on yet.
;;          emit_asm ("MOV R1, 0 ; no channels claimed by either namespace yet\n");
;;          emit_asm ("MOV [VIRCON32_MUSIC_CHANNEL_MASK], R1\n");
;;          emit_asm ("MOV [VIRCON32_SFX_CHANNEL_MASK], R1\n");
;;
;; ============================================================================
;; Memory Card: fixed hardware address range, NOT a compiler-reserved
;; allocation
;; ============================================================================
;; VIRCON32_MEMCARD_BASE (0x30000000), VIRCON32_MEMCARD_DATA_BASE
;; (0x30000018), and VIRCON32_MEMCARD_END (0x3003FFFF) are fixed physical
;; addresses -- unlike everything else on this page, they are NOT reserved
;; out of next_ram_address; they live entirely outside the compiler's RAM
;; pool, the same category as V32_CART_PAGE. This VM is word-addressed
;; throughout (an address unit is one 4-byte word -- see the [Rd+N] note
;; on __builtin_vircon32_btnp below), so VIRCON32_MEMCARD_DATA_BASE sits
;; exactly 24 WORDS past VIRCON32_MEMCARD_BASE: the first 20 words
;; (0x30000000..0x30000013) are the card's title, one word per character
;; (see __builtin_vircon32_memcard_title below); the next 4 words
;; (0x30000014..0x30000017) are reserved compiler-managed metadata,
;; including the auto-append cursor at VIRCON32_MEMCARD_CURSOR_ADDR (the
;; last of the 4); and VIRCON32_MEMCARD_DATA_BASE is where
;; memcard.save()/load()/[pos]'s position 0 lands. Position -1..-4 reach
;; back into metadata; position -5..-24 reach back into the title itself.
;;
;; Requires three %defines on the compiler side, in emit_variable_map()
;; (emit.c), alongside the ones above -- these need no globals and no
;; next_ram_address reservation, since the addresses are fixed constants
;; already available as C #defines in v32lua.h:
;;          fprintf (out(), "%%define  VIRCON32_MEMCARD_BASE      0x%.8X\n",
;;                   VIRCON32_MEMCARD_BASE);
;;          fprintf (out(), "%%define  VIRCON32_MEMCARD_DATA_BASE 0x%.8X\n",
;;                   VIRCON32_MEMCARD_DATA_BASE);
;;          fprintf (out(), "%%define  VIRCON32_MEMCARD_END       0x%.8X\n",
;;                   VIRCON32_MEMCARD_END);
;;
;; See the full compiler-side patch notes for the complete drop-in function
;; bodies all of the above lives inside.
;;
;; ============================================================================
;; __builtin_vircon32_spr: Sprite drawing -- dispatches, at RUNTIME, to the
;; cheapest GPU draw command the actual scale/angle values allow
;; ============================================================================
;; Stack layout relative to BP:
;; [BP+2]: region_id
;; [BP+3]: x
;; [BP+4]: y
;; [BP+5]: scale_x    (default 1.0)
;; [BP+6]: scale_y    (default 1.0)
;; [BP+7]: angle_deg  (default 0.0, degrees)
;; [BP+8]: color_mult (default 0xFFFFFFFF)
;; [BP+9]: blend_mode (default VIRCON32_BLEND_ALPHA)
;;
;; Vircon32's GPU exposes four region-draw commands, each consulting only
;; the transform state relevant to it:
;;   GPUCommand_DrawRegion            -- position only
;;   GPUCommand_DrawRegionZoomed      -- position + scale
;;   GPUCommand_DrawRegionRotated     -- position + angle
;;   GPUCommand_DrawRegionRotozoomed  -- position + scale + angle
;;
;; The compiler-side emitter (emit_vircon32_spr_intrinsic in v32lua.c) always
;; calls this one routine now, unconditionally pushing all 8 stack args (any
;; not supplied in the Lua call already default to 1.0/1.0/0.0/0xFFFFFFFF/
;; ALPHA there). Rather than a caller having to spell out scale_x/scale_y/
;; angle_deg pushing this call down a fixed, compile-time-chosen path (which
;; used to force the expensive Rotozoomed command even for a literal
;; spr(id, x, y, 1.0, 1.0, 0)), this routine reads the actual runtime values
;; of scale_x/scale_y/angle_deg and picks Plain/Zoomed/Rotated/Rotozoomed
;; every time it runs, based on what they actually equal.
;;
;; color_mult and blend_mode are handled unconditionally, before the
;; scale/angle dispatch: GPU_MultiplyColor and GPU_ActiveBlending are
;; independent, persistent GPU state that (as far as this audit can tell)
;; every DrawRegion* variant consults regardless of which one is used, so
;; they're always set here rather than left as whatever a *previous* draw
;; call happened to leave behind. GPU_DrawingScaleX/Y/Angle, by contrast,
;; are only written in the branches that actually need them -- leaving them
;; stale is fine there, since e.g. DrawRegionRotated is defined to ignore
;; ScaleX/Y entirely; that's the reason the 4 separate commands exist.
;;
;; (The "color/blend apply to every variant" assumption is inferred from the
;; GPU port layout in v32lua.h, not verified against Vircon32 GPU source --
;; worth confirming if you have it handy.)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

    ;@fn __builtin_vircon32_spr clobbers=R1-R5
__builtin_vircon32_spr:
    PUSH  BP
    MOV   BP, SP

    ;; --- 1. Set region ---
    MOV   R1, [BP+2]        ; region_id
    CFI   R1
    OUT   GPU_SelectedRegion, R1

    ;; --- 2. Set position ---
    MOV   R1, [BP+3]        ; x
    CFI   R1
    OUT   GPU_DrawingPointX, R1

    MOV   R1, [BP+4]        ; y
    CFI   R1
    OUT   GPU_DrawingPointY, R1

    ;; --- 3. Set color multiply / blending (applies to every draw variant) ---
    ;; color_mult arrives as the RAW packed 0xAABBGGRR word, not as a Lua
    ;; number: the compiler folds numeric literals to that word, and hex()
    ;; or a variable holding a hex() value already is one. It goes straight
    ;; to the port. (It used to be CFI'd, which turned hex("0xFFFFFFFF") --
    ;; a NaN bit pattern -- into 0 on ARM64 hosts and 0x80000000 on x86_64.)
    MOV   R1, [BP+8]        ; color_mult (raw packed RGBA word)
    OUT   GPU_MultiplyColor, R1

    MOV   R1, [BP+9]        ; blend_mode
    CFI   R1
    OUT   GPU_ActiveBlending, R1

    ;; --- 4. Decide which draw command the runtime values actually need ---
    ;; R2 = scale_x, R3 = scale_y, R4 = angle_deg -- kept live and never
    ;; themselves compared (only disposable copies in R5 are), so all three
    ;; are still available afterward to write out to GPU_DrawingScaleX/Y/
    ;; Angle if a transform turns out to be needed.
    MOV   R2, [BP+5]        ; scale_x
    MOV   R3, [BP+6]        ; scale_y
    MOV   R4, [BP+7]        ; angle_deg

    MOV   R5, R2
    FEQ   R5, 1.0
    JF    R5, _vircon32_spr_scaled        ; scale_x != 1.0 -> scaled
    MOV   R5, R3
    FEQ   R5, 1.0
    JF    R5, _vircon32_spr_scaled        ; scale_y != 1.0 -> scaled
    JMP   _vircon32_spr_check_angle_only  ; scale is default either way

_vircon32_spr_scaled:
    MOV   R5, R4
    FEQ   R5, 0.0
    JT    R5, _vircon32_spr_zoomed_only   ; angle == 0 -> scale only
    JMP   _vircon32_spr_rotozoomed        ; both scale and angle differ

_vircon32_spr_check_angle_only:
    MOV   R5, R4
    FEQ   R5, 0.0
    JT    R5, _vircon32_spr_plain         ; angle == 0 too -> fully default
    ;; falls through: scale is default, angle differs -> rotated only

_vircon32_spr_rotated_only:
    FMUL  R4, 0.017453292519943295 ; degrees -> radians
    OUT   GPU_DrawingAngle, R4
    OUT   GPU_Command, GPUCommand_DrawRegionRotated
    JMP   _vircon32_spr_end

_vircon32_spr_zoomed_only:
    OUT   GPU_DrawingScaleX, R2
    OUT   GPU_DrawingScaleY, R3
    OUT   GPU_Command, GPUCommand_DrawRegionZoomed
    JMP   _vircon32_spr_end

_vircon32_spr_rotozoomed:
    OUT   GPU_DrawingScaleX, R2
    OUT   GPU_DrawingScaleY, R3
    FMUL  R4, 0.017453292519943295 ; degrees -> radians
    OUT   GPU_DrawingAngle, R4
    OUT   GPU_Command, GPUCommand_DrawRegionRotozoomed
    JMP   _vircon32_spr_end

_vircon32_spr_plain:
    OUT   GPU_Command, GPUCommand_DrawRegion

_vircon32_spr_end:
    MOV   SP, BP
    POP   BP
    RET  

;; NOTE: __builtin_vircon32_spr_rotozoom used to live here as a separate
;; routine, called only when the compiler saw scale_x/scale_y/angle_deg
;; written out explicitly in the source spr() call. Its logic (set scale,
;; set angle, set color/blend, GPUCommand_DrawRegionRotozoomed) is now the
;; _vircon32_spr_rotozoomed branch inside __builtin_vircon32_spr above --
;; reached by runtime value, not by which arguments were textually present
;; -- so the standalone routine was folded in and removed rather than kept
;; as dead code. emit_vircon32_spr_intrinsic() in v32lua.c must be updated
;; to match (always `CALL __builtin_vircon32_spr`, dropping the
;; needs_rotozoom branch) -- see the accompanying note on that change.

;; ============================================================================
;; __builtin_vircon32_btn: Check if button is currently pressed
;; ============================================================================
;; Stack layout relative to BP:
;; [BP+2]: button_id (0-10)
;; [BP+3]: player (gamepad index 0-3 as float, or BOXED_NIL for current)
;;
;; Returns BOXED_TRUE or BOXED_FALSE in R0
;;
;; Button IDs (Vircon32 IOPorts order):
;;   0 = Left, 1 = Right, 2 = Up, 3 = Down
;;   4 = Start, 5 = A, 6 = B, 7 = X, 8 = Y
;;   9 = L (Left Shoulder), 10 = R (Right Shoulder)
;;
;; NOTE: Vircon32 comparison instructions are strictly 2-operand and
;; DESTRUCTIVE -- "OP Rd, Src" computes (Rd OP Src) and overwrites Rd with
;; the 0/1 boolean result. Any register whose value must survive a compare
;; has to be re-copied into a scratch register before each compare, which is
;; why button_id/player are kept in R1/R2 and only ever compared via a
;; throwaway copy in R2/R3.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

    ;@fn __builtin_vircon32_btn clobbers=R0-R3
__builtin_vircon32_btn:
    PUSH  BP
    MOV   BP, SP

    ;; --- 1. Get button_id and validate range (0-10) ---
    MOV   R1, [BP+2]
    CFI   R1

    ;; Validate: 0 <= button_id <= 10   (R1 preserved via R2/R3 copies)
    MOV   R2, R1
    ILT   R2, 0              ; R2 = (button_id < 0)
    MOV   R3, R1
    IGT   R3, 10             ; R3 = (button_id > 10)
    OR    R2, R3
    JT    R2, _vircon32_btn_invalid

    ;; --- 2. Handle player parameter ---
    ;; If player is BOXED_NIL, use current gamepad (don't write to INP_SelectedGamepad)
    ;; Otherwise, set INP_SelectedGamepad to the specified player
    MOV   R2, [BP+3]
    MOV   R3, R2
    IEQ   R3, BOXED_NIL
    JT    R3, _vircon32_btn_use_current_gamepad

    ;; Player specified - convert to int and set it (through v32io, which
    ;; remembers it: the port can't be read back)
    CFI   R2
    PUSH  R1
    MOV   R1, R2
    CALL  __v32io_select
    POP   R1
    JMP   _vircon32_btn_check_button

_vircon32_btn_use_current_gamepad:
    ;; Use whatever gamepad is already selected - do nothing

_vircon32_btn_check_button:
    ;; --- 3. Map button_id to IOPort and read state ---
    ;; Chain of copy+compare+branch: comparisons are destructive and
    ;; 2-operand only, so R2 is a disposable scratch copy of R1 each time.

    MOV   R2, R1
    IEQ   R2, 0
    JT    R2, _vircon32_btn_read_left
    MOV   R2, R1
    IEQ   R2, 1
    JT    R2, _vircon32_btn_read_right
    MOV   R2, R1
    IEQ   R2, 2
    JT    R2, _vircon32_btn_read_up
    MOV   R2, R1
    IEQ   R2, 3
    JT    R2, _vircon32_btn_read_down
    MOV   R2, R1
    IEQ   R2, 4
    JT    R2, _vircon32_btn_read_start
    MOV   R2, R1
    IEQ   R2, 5
    JT    R2, _vircon32_btn_read_a
    MOV   R2, R1
    IEQ   R2, 6
    JT    R2, _vircon32_btn_read_b
    MOV   R2, R1
    IEQ   R2, 7
    JT    R2, _vircon32_btn_read_x
    MOV   R2, R1
    IEQ   R2, 8
    JT    R2, _vircon32_btn_read_y
    MOV   R2, R1
    IEQ   R2, 9
    JT    R2, _vircon32_btn_read_l
    MOV   R2, R1
    IEQ   R2, 10
    JT    R2, _vircon32_btn_read_r

    JMP   _vircon32_btn_invalid

_vircon32_btn_read_left:
    IN    R2, INP_GamepadLeft
    JMP   _vircon32_btn_eval
_vircon32_btn_read_right:
    IN    R2, INP_GamepadRight
    JMP   _vircon32_btn_eval
_vircon32_btn_read_up:
    IN    R2, INP_GamepadUp
    JMP   _vircon32_btn_eval
_vircon32_btn_read_down:
    IN    R2, INP_GamepadDown
    JMP   _vircon32_btn_eval
_vircon32_btn_read_start:
    IN    R2, INP_GamepadButtonStart
    JMP   _vircon32_btn_eval
_vircon32_btn_read_a:
    IN    R2, INP_GamepadButtonA
    JMP   _vircon32_btn_eval
_vircon32_btn_read_b:
    IN    R2, INP_GamepadButtonB
    JMP   _vircon32_btn_eval
_vircon32_btn_read_x:
    IN    R2, INP_GamepadButtonX
    JMP   _vircon32_btn_eval
_vircon32_btn_read_y:
    IN    R2, INP_GamepadButtonY
    JMP   _vircon32_btn_eval
_vircon32_btn_read_l:
    IN    R2, INP_GamepadButtonL
    JMP   _vircon32_btn_eval
_vircon32_btn_read_r:
    IN    R2, INP_GamepadButtonR

_vircon32_btn_eval:
    ;; Vircon32 returns >0 for pressed, <=0 for not pressed
    IGE   R2, 1
    JT    R2, _vircon32_btn_true

_vircon32_btn_false:
_vircon32_btn_invalid:
    MOV   R0, BOXED_FALSE
    JMP   _vircon32_btn_end

_vircon32_btn_true:
    MOV   R0, BOXED_TRUE

_vircon32_btn_end:
    MOV   SP, BP
    POP   BP
    RET  

;; ============================================================================
;; __builtin_vircon32_btnp: Check if button was pressed THIS frame (edge)
;; ============================================================================
;; Stack layout relative to BP:
;; [BP+2]: button_id (0-10)
;; [BP+3]: player (gamepad index 0-3 as float, or BOXED_NIL for current)
;;
;; Uses VIRCON32_BTN_PREV_STATE -- a fixed, compiler-reserved 44-word RAM
;; range (see the big comment at the top of this file) -- to track state
;; from the previous frame. Returns true only on the frame transition from
;; NOT pressed to pressed.
;;
;; Returns BOXED_TRUE or BOXED_FALSE in R0
;;
;; NOTE: same 2-operand/destructive-compare constraint as __builtin_vircon32_btn
;; above -- button_id lives in R1, player lives in R4, and every compare goes
;; through a disposable R2/R3/R7 copy so R1/R4 survive to be used afterward
;; as the (player * 11 + button_id) word index into VIRCON32_BTN_PREV_STATE.
;;
;; The player index is also clamped to 0-3 before it's used as an array
;; index -- an out-of-range player (e.g. a caller passing a bad explicit
;; value) would otherwise compute an address outside the 44-word prev-state
;; table and corrupt adjacent heap memory. Adjust/remove this clamp if
;; you'd rather treat an out-of-range player as an error some other way.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

    ;@fn __builtin_vircon32_btnp clobbers=R0-R7
__builtin_vircon32_btnp:
    PUSH  BP
    MOV   BP, SP

    ;; --- 1. Get button_id and validate range (0-10) ---
    MOV   R1, [BP+2]
    CFI   R1

    ;; Validate: 0 <= button_id <= 10   (R1 preserved via R2/R3 copies)
    MOV   R2, R1
    ILT   R2, 0
    MOV   R3, R1
    IGT   R3, 10
    OR    R2, R3
    JT    R2, _vircon32_btnp_invalid

    ;; --- 2. Handle player parameter ---
    MOV   R2, [BP+3]
    MOV   R3, R2
    IEQ   R3, BOXED_NIL
    JT    R3, _vircon32_btnp_use_current_gamepad

    ;; Player specified - convert to int, save in R4, and set gamepad
    CFI   R2
    MOV   R4, R2            ; Save player index in R4
    PUSH  R1
    MOV   R1, R2
    CALL  __v32io_select
    POP   R1
    JMP   _vircon32_btnp_clamp_player

_vircon32_btnp_use_current_gamepad:
    ;; Use current gamepad - which one is selected. Not read from
    ;; INP_SelectedGamepad: the emulator returns garbage for that port
    ;; (V32GamepadController::ReadPort falls through to the per-gamepad
    ;; ports), so v32io.s keeps the last selection in V32IO_GAMEPAD.
    MOV   R4, [V32IO_GAMEPAD]

_vircon32_btnp_clamp_player:
    ;; Clamp R4 to 0-3 so the prev-state index below can never run off
    ;; the end of the 44-word table.
    MOV   R2, R4
    ILT   R2, 0
    JT    R2, _vircon32_btnp_player_low
    MOV   R2, R4
    IGT   R2, 3
    JT    R2, _vircon32_btnp_player_high
    JMP   _vircon32_btnp_read_current

_vircon32_btnp_player_low:
    MOV   R4, 0
    JMP   _vircon32_btnp_read_current

_vircon32_btnp_player_high:
    MOV   R4, 3

_vircon32_btnp_read_current:
    ;; R1 = button_id, R4 = player index

    ;; --- 3. Read current button state ---
    MOV   R2, R1
    IEQ   R2, 0
    JT    R2, _vircon32_btnp_read_current_left
    MOV   R2, R1
    IEQ   R2, 1
    JT    R2, _vircon32_btnp_read_current_right
    MOV   R2, R1
    IEQ   R2, 2
    JT    R2, _vircon32_btnp_read_current_up
    MOV   R2, R1
    IEQ   R2, 3
    JT    R2, _vircon32_btnp_read_current_down
    MOV   R2, R1
    IEQ   R2, 4
    JT    R2, _vircon32_btnp_read_current_start
    MOV   R2, R1
    IEQ   R2, 5
    JT    R2, _vircon32_btnp_read_current_a
    MOV   R2, R1
    IEQ   R2, 6
    JT    R2, _vircon32_btnp_read_current_b
    MOV   R2, R1
    IEQ   R2, 7
    JT    R2, _vircon32_btnp_read_current_x
    MOV   R2, R1
    IEQ   R2, 8
    JT    R2, _vircon32_btnp_read_current_y
    MOV   R2, R1
    IEQ   R2, 9
    JT    R2, _vircon32_btnp_read_current_l
    MOV   R2, R1
    IEQ   R2, 10
    JT    R2, _vircon32_btnp_read_current_r

    JMP   _vircon32_btnp_invalid

_vircon32_btnp_read_current_left:
    IN    R2, INP_GamepadLeft
    JMP   _vircon32_btnp_got_current_state
_vircon32_btnp_read_current_right:
    IN    R2, INP_GamepadRight
    JMP   _vircon32_btnp_got_current_state
_vircon32_btnp_read_current_up:
    IN    R2, INP_GamepadUp
    JMP   _vircon32_btnp_got_current_state
_vircon32_btnp_read_current_down:
    IN    R2, INP_GamepadDown
    JMP   _vircon32_btnp_got_current_state
_vircon32_btnp_read_current_start:
    IN    R2, INP_GamepadButtonStart
    JMP   _vircon32_btnp_got_current_state
_vircon32_btnp_read_current_a:
    IN    R2, INP_GamepadButtonA
    JMP   _vircon32_btnp_got_current_state
_vircon32_btnp_read_current_b:
    IN    R2, INP_GamepadButtonB
    JMP   _vircon32_btnp_got_current_state
_vircon32_btnp_read_current_x:
    IN    R2, INP_GamepadButtonX
    JMP   _vircon32_btnp_got_current_state
_vircon32_btnp_read_current_y:
    IN    R2, INP_GamepadButtonY
    JMP   _vircon32_btnp_got_current_state
_vircon32_btnp_read_current_l:
    IN    R2, INP_GamepadButtonL
    JMP   _vircon32_btnp_got_current_state
_vircon32_btnp_read_current_r:
    IN    R2, INP_GamepadButtonR

_vircon32_btnp_got_current_state:
    ;; R2 = current state (>0 for pressed, <=0 for released)
    ;; R1 = button_id, R4 = player index

    ;; Check if currently pressed
    ILE   R2, 0
    JT    R2, _vircon32_btnp_not_pressed

    ;; Button IS currently pressed - check if it was NOT pressed last frame
    ;; Calculate word index: player * 11 + button_id (one word per entry --
    ;; VIRCON32_BTN_PREV_STATE is a fixed compile-time RAM address, not a
    ;; byte offset, so no *4 here despite each entry being a 4-byte float.
    ;; [Rd+N] addressing throughout this VM's runtime -- see e.g. table
    ;; array/hash access in runtime.s -- already steps by whole words per
    ;; unit of N.)
    MOV   R3, R4
    IMUL  R3, 11           ; player * 11
    IADD  R3, R1           ; + button_id

    ;; Load previous state from memory
    MOV   R5, VIRCON32_BTN_PREV_STATE
    IADD  R5, R3           ; Address of this button's previous state
    MOV   R6, [R5]         ; Load previous state (0.0 or 1.0)

    ;; If previous state was 0.0 (not pressed), this is a new press
    MOV   R7, R6
    FEQ   R7, 0.0
    JT    R7, _vircon32_btnp_edge_detected

    ;; Previous state was 1.0 (pressed) - not an edge
    JMP   _vircon32_btnp_no_edge

_vircon32_btnp_not_pressed:
    ;; Button is NOT currently pressed
    ;; Calculate word index (see note above -- no *4)
    MOV   R3, R4
    IMUL  R3, 11
    IADD  R3, R1

    ;; Update state to 0.0 (not pressed)
    MOV   R5, VIRCON32_BTN_PREV_STATE
    IADD  R5, R3
    MOV   R6, 0.0
    MOV   [R5], R6

    MOV   R0, BOXED_FALSE
    JMP   _vircon32_btnp_end

_vircon32_btnp_edge_detected:
    ;; New press detected - update state to 1.0 and return true
    MOV   R3, R4
    IMUL  R3, 11
    IADD  R3, R1

    MOV   R5, VIRCON32_BTN_PREV_STATE
    IADD  R5, R3
    MOV   R6, 1.0
    MOV   [R5], R6

    MOV   R0, BOXED_TRUE
    JMP   _vircon32_btnp_end

_vircon32_btnp_no_edge:
    ;; Button was already pressed - update state (stay at 1.0) and return false
    MOV   R3, R4
    IMUL  R3, 11
    IADD  R3, R1

    MOV   R5, VIRCON32_BTN_PREV_STATE
    IADD  R5, R3
    MOV   R6, 1.0
    MOV   [R5], R6

    MOV   R0, BOXED_FALSE
    JMP   _vircon32_btnp_end

_vircon32_btnp_invalid:
    MOV   R0, BOXED_FALSE

_vircon32_btnp_end:
    MOV   SP, BP
    POP   BP
    RET  

;; ============================================================================
;; __builtin_vircon32_play: play a sound on a channel
;; ============================================================================
;; Stack layout relative to BP:
;; [BP+2]: sound      (Lua float resource id, or BOXED_NIL -> nothing to play)
;; [BP+3]: channel    (Lua float 0-15,        or BOXED_NIL -> 0)
;; [BP+4]: chanloop   (any Lua value,         or BOXED_NIL -> false)
;; [BP+5]: chanvol    (Lua float,             or BOXED_NIL -> 1.0)
;; [BP+6]: startindex (Lua float sample index, or BOXED_NIL -> no seek)
;;
;; Returns: R0 = the channel actually used, as a Lua float
;;
;; The compiler-side emitter (emit_vircon32_play_intrinsic in v32lua.c) only
;; calls this when at least one argument is NOT known at compile time. A call
;; whose arguments are all literals -- including a bare --#sound name, which
;; the compiler folds to its resource id -- is emitted as a straight-line OUT
;; sequence instead and never reaches this routine. Both paths emit the SAME
;; port order, and that order is not arbitrary:
;;
;;   1. SPU_SelectedChannel        -- every per-channel port acts on it
;;   2. SPUCommand_StopSelectedChannel
;;   3. SPU_ChannelAssignedSound
;;   4. SPU_ChannelVolume
;;   5. SPUCommand_PlaySelectedChannel
;;   6. SPU_ChannelLoopEnabled     <-- AFTER the play command
;;   7. SPU_ChannelPosition        <-- AFTER the play command
;;
;; Three console behaviours force that shape. All three are silent when you
;; get them wrong -- no error, no port-write rejection, just the wrong sound:
;;
;; A. A SOUND CANNOT BE ASSIGNED TO A BUSY CHANNEL.
;;    WriteSPUChannelAssignedSound() ignores the write outright unless the
;;    channel is Stopped ("sounds can only be assigned to a non playing
;;    channel"). Without the stop at step 2, calling play() with a new sound
;;    on a channel that is still sounding replays the OLD sound. Stopping an
;;    already-idle channel costs nothing.
;;
;; B. THE PLAY COMMAND OVERWRITES THE CHANNEL'S LOOP FLAG.
;;    PlayChannel(), for a Stopped or already-Playing channel, does:
;;        TargetChannel.LoopEnabled = ChannelSound->PlayWithLoop;
;;        TargetChannel.Position    = 0;
;;    so SPU_ChannelLoopEnabled written BEFORE the command is thrown away and
;;    replaced by the SOUND's own PlayWithLoop -- which is false unless
;;    something set SPU_SoundPlayWithLoop on that sound. That is why loop is
;;    written at step 6: by then the channel is Playing and the value sticks.
;;    It is written even when it is 0, because step 5 has just replaced it
;;    with the sound's flag and "no loop argument" must mean no loop.
;;
;; C. THE PLAY COMMAND REWINDS THE CHANNEL.
;;    Same two lines: Position = 0. A seek written before the command is
;;    discarded, so the optional startindex is applied at step 7.
;;
;;    (For a PAUSED channel PlayChannel() takes neither branch -- it just
;;    sets State = Playing -- which is what makes resume() work. So on a
;;    resume neither the loop flag nor the position is disturbed.)
;;
;; SPU_ChannelPosition is an INTEGER port: a sample index, clamped by the
;; console to 0..length-1. It takes CFI like the sound and channel ids, NOT
;; a raw Lua float. SPU_ChannelVolume and SPU_ChannelSpeed are the genuine
;; float ports here.
;;
;; Register use follows __builtin_vircon32_spr/btn/btnp: R1-R3 are used
;; freely without saving, since the emitter frees its registers before the
;; CALL. Comparisons are 2-operand and destructive, so R1 (the channel) is
;; never compared directly -- only disposable copies in R2/R3 are.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

    ;@fn __builtin_vircon32_play clobbers=R0-R3
__builtin_vircon32_play:
    PUSH  BP
    MOV   BP, SP

    ;; --- 1. Resolve the channel: nil -> 0, otherwise clamp to 0-15 ---
    MOV   R1, [BP+3]
    MOV   R2, R1
    IEQ   R2, BOXED_NIL
    JT    R2, _vircon32_play_chan_zero

    CFI   R1                      ; Lua float -> hardware integer
    MOV   R2, R1
    ILT   R2, 0
    JT    R2, _vircon32_play_chan_zero
    MOV   R2, R1
    IGT   R2, 15
    JF    R2, _vircon32_play_chan_ready
    MOV   R1, 15                  ; clamp high
    JMP   _vircon32_play_chan_ready

_vircon32_play_chan_zero:
    MOV   R1, 0                   ; nil or negative -> channel 0

_vircon32_play_chan_ready:
    OUT   SPU_SelectedChannel, R1

    ;; --- 2. Fetch the sound; a nil sound leaves the channel alone ---
    MOV   R2, [BP+2]
    MOV   R3, R2
    IEQ   R3, BOXED_NIL
    JT    R3, _vircon32_play_done

    ;; --- 3. Stop, THEN assign (see note A above) ---
    OUT   SPU_Command, SPUCommand_StopSelectedChannel
    CFI   R2
    OUT   SPU_ChannelAssignedSound, R2

    ;; --- 3b. Track channel ownership: claim R1 for music, release it from
    ;; sfx if sfx had it. See emit_vircon32_claim_channel_static() in
    ;; v32lua.c (intrinsics_vircon32_sound.c) for the mirror-image
    ;; compile-time version of this, used by the static-fold path this
    ;; routine's dynamic path complements. R1 (channel) must survive this;
    ;; R2 held the sound value, already OUT'd above and not needed again
    ;; until step 4 re-fetches it from the stack, so R2/R3 are free scratch
    ;; here. VIRCON32_MUSIC_CHANNEL_MASK/VIRCON32_SFX_CHANNEL_MASK are new
    ;; compiler-reserved RAM words -- see the header comment block at the
    ;; top of this file for the v32lua.c-side additions they require.
    MOV   R2, 1
    SHL   R2, R1                        ; R2 = 1 << channel
    MOV   R3, [VIRCON32_MUSIC_CHANNEL_MASK]
    OR    R3, R2
    MOV   [VIRCON32_MUSIC_CHANNEL_MASK], R3

    MOV   R3, R2
    NOT   R3                            ; R3 = ~(1 << channel)
    MOV   R2, [VIRCON32_SFX_CHANNEL_MASK]
    AND   R2, R3
    MOV   [VIRCON32_SFX_CHANNEL_MASK], R2

    ;; --- 4. Channel volume: nil -> 1.0 (true float port, no cast) ---
    MOV   R2, [BP+5]
    MOV   R3, R2
    IEQ   R3, BOXED_NIL
    JF    R3, _vircon32_play_vol_ready
    MOV   R2, 1.0

_vircon32_play_vol_ready:
    OUT   SPU_ChannelVolume, R2

    ;; --- 5. Start playback ---
    OUT   SPU_Command, SPUCommand_PlaySelectedChannel

    ;; --- 6. Loop flag, AFTER the play command (see note B above) ---
    ;; Lua truthiness: only nil and false are falsy; every number, 0
    ;; included, is truthy.
    MOV   R2, [BP+4]
    MOV   R3, R2
    IEQ   R3, BOXED_NIL
    JT    R3, _vircon32_play_loop_off
    MOV   R3, R2
    IEQ   R3, BOXED_FALSE
    JT    R3, _vircon32_play_loop_off

    OUT   SPU_ChannelLoopEnabled, 1
    JMP   _vircon32_play_seek

_vircon32_play_loop_off:
    OUT   SPU_ChannelLoopEnabled, 0

    ;; --- 7. Optional seek, AFTER the play command (see note C above) ---
_vircon32_play_seek:
    MOV   R2, [BP+6]
    MOV   R3, R2
    IEQ   R3, BOXED_NIL
    JT    R3, _vircon32_play_done
    CFI   R2                      ; integer sample index, not a float
    OUT   SPU_ChannelPosition, R2

_vircon32_play_done:
    ;; Return the channel actually used, as a Lua number, so callers can
    ;; write  local ch = play(MUSIC)  and later  stop(ch).
    MOV   R0, R1
    CIF   R0

    MOV   SP, BP
    POP   BP
    RET  

;; ============================================================================
;; __builtin_vircon32_chanselect: resolve stop()/pause()/resume()'s CHANNEL
;; ============================================================================
;; Stack layout relative to BP:
;; [BP+2]: channel (Lua float 0-15, or BOXED_NIL -> apply to all channels)
;;
;; Returns:
;;   R0 = 1   a channel was named: it has been nil-checked, CFI-ed, clamped
;;            to 0-15 and written to SPU_SelectedChannel. The caller
;;            follows up with its per-channel command.
;;   R0 = 0   the channel was nil: nothing was written. The caller follows
;;            up with its all-channels command.
;;
;; This routine resolves only the CHANNEL half of stop()/pause()/resume();
;; the emitter's dynamic path calls it and then branches on R0. The command
;; half is a compile-time constant of the call site, and SPUCommand_*
;; labels are valid ONLY as operands of OUT -- they cannot be moved into a
;; register or pushed on the stack. (The old __builtin_vircon32_chancmd
;; received the two commands as pushed stack words, which forced the
;; compiler to emit "MOV R0, SPUCommand_..." -- not a real instruction --
;; so that routine was removed and the commands moved to the call site:)
;;
;;   CALL __builtin_vircon32_chanselect
;;   JT   R0, <selected>
;;   OUT  SPU_Command, SPUCommand_<AllChannels>       ; nil case
;;   JMP  <done>
;; <selected>:
;;   OUT  SPU_Command, SPUCommand_<SelectedChannel>
;; <done>:
;;
;; The command pairs, chosen by the emitter's table:
;;   stop(ch)    StopSelectedChannel     stop()    StopAllChannels
;;   pause(ch)   PauseSelectedChannel    pause()   PauseAllChannels
;;   resume(ch)  PlaySelectedChannel     resume()  ResumeAllChannels
;;
;; resume(ch) uses Play rather than a "resume selected channel" command
;; because the console has no such command. PlayChannel() leaves a PAUSED
;; channel's position and loop flag untouched and simply sets it Playing,
;; so on a paused channel Play IS the per-channel resume. (ResumeAllChannels
;; is literally a loop calling PlayChannel on every PAUSED channel and
;; skipping the rest.)
;;
;; But Play is only a resume for a PAUSED channel: on a STOPPED channel it
;; rewinds to 0 and restarts, and on a PLAYING one it retriggers from 0.
;; So the emitter (spu_emit_selected_channel_cmd in intrinsics/spu.c)
;; guards resume(ch)'s per-channel command with IN SPU_ChannelState == 0x41
;; (Paused) and skips it otherwise; stop(ch) and pause(ch) are unguarded.
;; The guard is emitted inline at the <selected> label above, not here.
;; A "resume" that restarts from the beginning therefore no longer happens
;; on a stopped channel; if a sound restarts when you did not expect it,
;; the usual cause is a loop flag lost on a sound that ran to its end; see
;; note B on __builtin_vircon32_play above.
;;
;; The BOXED_NIL test happens on the raw boxed value, BEFORE the CFI --
;; once converted, the tag bits are gone and nil is indistinguishable
;; from 0.0.
;;
;; Register use follows the other __builtin_vircon32_* routines: R1-R2
;; are used freely without saving, since the emitter frees its registers
;; before the CALL. The channel lives in R1 and is only ever compared
;; via a throwaway copy in R2, because IEQ/ILT/IGT are destructive.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; ============================================================================
;; __builtin_vircon32_chanselect
;; [BP+2]: channel (Lua float 0-15, or BOXED_NIL -> all channels)
;; Returns: R0 = 1 with SPU_SelectedChannel already written (clamped 0-15),
;;          R0 = 0 when the channel was nil (caller issues its
;;          all-channels command itself -- SPUCommand_* labels are only
;;          valid in OUT operands, so the command cannot travel through
;;          a register or the stack).
;; ============================================================================
    ;@fn __builtin_vircon32_chanselect clobbers=R0-R2
__builtin_vircon32_chanselect:
    PUSH  BP
    MOV   BP, SP
    MOV   R1, [BP+2]
    MOV   R2, R1
    IEQ   R2, BOXED_NIL
    JT    R2, _vircon32_chanselect_nil
    CFI   R1
    MOV   R2, R1
    ILT   R2, 0
    JT    R2, _vircon32_chanselect_low
    MOV   R2, R1
    IGT   R2, 15
    JT    R2, _vircon32_chanselect_high
    JMP   _vircon32_chanselect_out
_vircon32_chanselect_low:
    MOV   R1, 0
    JMP   _vircon32_chanselect_out
_vircon32_chanselect_high:
    MOV   R1, 15
_vircon32_chanselect_out:
    OUT   SPU_SelectedChannel, R1
    MOV   R0, 1
    JMP   _vircon32_chanselect_done
_vircon32_chanselect_nil:
    MOV   R0, 0
_vircon32_chanselect_done:
    MOV   SP, BP
    POP   BP
    RET  

;; ============================================================================
;; __builtin_vircon32_sfx_play: fire a transient sound effect
;; ============================================================================
;; Stack layout relative to BP:
;; [BP+2]: sound   (Lua float resource id, or BOXED_NIL -> nothing to play)
;; [BP+3]: channel (Lua float 0-15, or BOXED_NIL -> allocate one)
;; [BP+4]: volume  (Lua float,      or BOXED_NIL -> 1.0)
;; [BP+5]: speed   (Lua float,      or BOXED_NIL -> 1.0)
;;
;; Returns: R0 = the channel actually used, as a Lua float
;;
;; Port order is the same as __builtin_vircon32_play and for the same three
;; reasons -- stop before assigning (a sound only assigns to a stopped
;; channel), and clear the loop flag AFTER the play command (which overwrites
;; it from the sound's own PlayWithLoop). See the notes on that routine.
;;
;; CHANNEL ALLOCATION
;; ------------------
;; With no channel given, the next one is taken from VIRCON32_SFX_CURSOR, a
;; single compiler-reserved RAM word cycling over channels 1-15. Channel 0 is
;; never handed out, so music.play() on the default channel is never cut off
;; by a sound effect.
;;
;; The cursor is advanced unconditionally rather than searching for a channel
;; that is actually idle. Searching would cost up to 15 IN + compare on the
;; hot path -- sfx.play() runs on every jump and footstep -- to protect
;; against a case that only arises when 15 effects overlap, at which point
;; the oldest is the right one to lose anyway.
;;
;; The cursor holds a plain hardware integer, not a Lua float: nothing but
;; this routine ever reads it. generate_global_setup() initialises it to 1.
;;
;; Register use follows the other __builtin_vircon32_* routines: R1-R3 used
;; freely without saving, since the emitter frees its registers before the
;; CALL. Comparisons are 2-operand and destructive, so R1 (the channel) is
;; only ever compared through a throwaway copy.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

    ;@fn __builtin_vircon32_sfx_play clobbers=R0-R3
__builtin_vircon32_sfx_play:
    PUSH  BP
    MOV   BP, SP

    ;; --- 1. Resolve the channel ---
    MOV   R1, [BP+3]
    MOV   R2, R1
    IEQ   R2, BOXED_NIL
    JT    R2, _vircon32_sfx_auto_channel

    ;; Explicit channel: cast and clamp to 0-15
    CFI   R1
    MOV   R2, R1
    ILT   R2, 0
    JT    R2, _vircon32_sfx_clamp_low
    MOV   R2, R1
    IGT   R2, 15
    JF    R2, _vircon32_sfx_channel_ready
    MOV   R1, 15
    JMP   _vircon32_sfx_channel_ready

_vircon32_sfx_clamp_low:
    MOV   R1, 0
    JMP   _vircon32_sfx_channel_ready

_vircon32_sfx_auto_channel:
    ;; Take the cursor, then advance it with wrap 1 -> 15 -> 1
    MOV   R1, [VIRCON32_SFX_CURSOR]

    MOV   R2, R1
    IADD  R2, 1
    MOV   R3, R2
    IGT   R3, 15
    JF    R3, _vircon32_sfx_store_cursor
    MOV   R2, 1

_vircon32_sfx_store_cursor:
    MOV   [VIRCON32_SFX_CURSOR], R2

_vircon32_sfx_channel_ready:
    OUT   SPU_SelectedChannel, R1

    ;; --- 2. Fetch the sound; a nil sound leaves the channel alone ---
    MOV   R2, [BP+2]
    MOV   R3, R2
    IEQ   R3, BOXED_NIL
    JT    R3, _vircon32_sfx_done

    ;; --- 3. Stop, THEN assign ---
    OUT   SPU_Command, SPUCommand_StopSelectedChannel
    CFI   R2
    OUT   SPU_ChannelAssignedSound, R2

    ;; --- 3b. Track channel ownership: claim R1 for sfx, release it from
    ;; music if music had it. Mirror image of the block in
    ;; __builtin_vircon32_play above -- see the notes there. R1 (channel)
    ;; must survive this; R2/R3 are free scratch (R2's sound value was
    ;; already OUT'd above, and gets re-fetched from the stack at step 4).
    MOV   R2, 1
    SHL   R2, R1                        ; R2 = 1 << channel
    MOV   R3, [VIRCON32_SFX_CHANNEL_MASK]
    OR    R3, R2
    MOV   [VIRCON32_SFX_CHANNEL_MASK], R3

    MOV   R3, R2
    NOT   R3                            ; R3 = ~(1 << channel)
    MOV   R2, [VIRCON32_MUSIC_CHANNEL_MASK]
    AND   R2, R3
    MOV   [VIRCON32_MUSIC_CHANNEL_MASK], R2

    ;; --- 4. Volume: nil -> 1.0 (true float port, no cast) ---
    MOV   R2, [BP+4]
    MOV   R3, R2
    IEQ   R3, BOXED_NIL
    JF    R3, _vircon32_sfx_volume_ready
    MOV   R2, 1.0

_vircon32_sfx_volume_ready:
    OUT   SPU_ChannelVolume, R2

    ;; --- 5. Speed / pitch: nil -> 1.0 (float port, console clamps 0-128) ---
    MOV   R2, [BP+5]
    MOV   R3, R2
    IEQ   R3, BOXED_NIL
    JF    R3, _vircon32_sfx_speed_ready
    MOV   R2, 1.0

_vircon32_sfx_speed_ready:
    OUT   SPU_ChannelSpeed, R2

    ;; --- 6. Start, then clear the loop flag the command just set ---
    OUT   SPU_Command, SPUCommand_PlaySelectedChannel
    OUT   SPU_ChannelLoopEnabled, 0

_vircon32_sfx_done:
    ;; Return the channel used, as a Lua number, so the caller can hold on to
    ;; it (local ch = sfx.play(ENGINE) ... sfx.stop(ch)).
    MOV   R0, R1
    CIF   R0

    MOV   SP, BP
    POP   BP
    RET  

;; ============================================================================
;; __builtin_vircon32_sfx_stop_all: stop channels 1-15, leaving 0 alone
;; ============================================================================
;; Takes no arguments. Returns R0 = BOXED_NIL.
;;
;; This is what bare sfx.stop() compiles to. It deliberately does NOT use
;; SPUCommand_StopAllChannels: that would stop channel 0 as well and cut the
;; music, which is the one thing a caller asking to silence sound EFFECTS
;; does not mean.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

    ;@fn __builtin_vircon32_sfx_stop_all clobbers=R0-R2
__builtin_vircon32_sfx_stop_all:
    PUSH  BP
    MOV   BP, SP

    MOV   R1, 1

_vircon32_sfx_stop_loop:
    OUT   SPU_SelectedChannel, R1
    OUT   SPU_Command, SPUCommand_StopSelectedChannel

    IADD  R1, 1
    MOV   R2, R1
    IGT   R2, 15
    JF    R2, _vircon32_sfx_stop_loop

    MOV   R0, BOXED_NIL

    MOV   SP, BP
    POP   BP
    RET  

;; ============================================================================
;; __builtin_vircon32_volume: set one channel's volume, or the true global
;; volume when the channel is -1
;; ============================================================================
;; Stack layout relative to BP:
;; [BP+2]: volume  (Lua float, or BOXED_NIL -> 1.0 default)
;; [BP+3]: channel (Lua float; -1 -> SPU_GlobalVolume, else clamped 0-15)
;;
;; Returns: R0 = BOXED_NIL
;;
;; Backs music.volume(VOL, CHANNEL)/sfx.volume(VOL, CHANNEL) when CHANNEL
;; was written explicitly at the call site (as opposed to omitted, which
;; goes to __builtin_vircon32_volume_mask instead -- see that routine).
;; The compiler-side emitter (emit_vircon32_volume_intrinsic in v32lua.c,
;; file intrinsics_vircon32_sound.c) only reaches THIS routine when the
;; volume and/or the channel value isn't known at compile time; an
;; all-literal call folds to a straight-line OUT sequence instead.
;;
;; CHANNEL is guaranteed by the emitter to be a real (non-nil) expression
;; here -- "no channel argument at all" is a different call site entirely.
;; The BOXED_NIL check on volume is kept anyway as a cheap defensive
;; default, same as every other optional numeric argument in this file.
;;
;; SPU_ChannelVolume (per-channel, clamped 0-8 by the console) and
;; SPU_GlobalVolume (clamped 0-2) are both genuine float ports -- the Lua
;; volume value is used directly, no CFI. The channel argument, when it
;; isn't -1, takes the same clamp-to-0-15 treatment as every other
;; per-channel routine in this file (see __builtin_vircon32_play/_chancmd/
;; _sfx_play above).
;;
;; Register use follows the other __builtin_vircon32_* routines: R1-R3 used
;; freely without saving, since the emitter frees its registers before the
;; CALL. R1 holds the resolved volume and is never itself compared -- only
;; the disposable copies in R2/R3 are, since IEQ/ILT/IGT are destructive.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

    ;@fn __builtin_vircon32_volume clobbers=R0-R3
__builtin_vircon32_volume:
    PUSH  BP
    MOV   BP, SP

    ;; --- 1. Resolve the volume: nil -> 1.0 ---
    MOV   R1, [BP+2]
    MOV   R2, R1
    IEQ   R2, BOXED_NIL
    JF    R2, _vircon32_volume_val_ready
    MOV   R1, 1.0

_vircon32_volume_val_ready:
    ;; --- 2. Channel == -1 -> global volume; otherwise clamp and select ---
    MOV   R2, [BP+3]
    CFI   R2                      ; Lua float -> hardware integer
    MOV   R3, R2
    IEQ   R3, -1
    JT    R3, _vircon32_volume_global

    MOV   R3, R2
    ILT   R3, 0
    JT    R3, _vircon32_volume_chan_low
    MOV   R3, R2
    IGT   R3, 15
    JF    R3, _vircon32_volume_chan_ready
    MOV   R2, 15                  ; clamp high
    JMP   _vircon32_volume_chan_ready

_vircon32_volume_chan_low:
    MOV   R2, 0                   ; clamp low

_vircon32_volume_chan_ready:
    OUT   SPU_SelectedChannel, R2
    OUT   SPU_ChannelVolume, R1
    JMP   _vircon32_volume_done

_vircon32_volume_global:
    OUT   SPU_GlobalVolume, R1

_vircon32_volume_done:
    MOV   R0, BOXED_NIL

    MOV   SP, BP
    POP   BP
    RET  

;; ============================================================================
;; __builtin_vircon32_volume_mask: apply a volume to every channel a
;; namespace's channel-ownership mask currently marks as claimed
;; ============================================================================
;; Stack layout relative to BP:
;; [BP+2]: volume    (Lua float, or BOXED_NIL -> 1.0 default)
;; [BP+3]: mask_addr (raw hardware integer -- the RAM address of either
;;                    VIRCON32_MUSIC_CHANNEL_MASK or VIRCON32_SFX_CHANNEL_MASK,
;;                    pushed by the compiler as a literal, NOT a Lua value)
;;
;; Returns: R0 = BOXED_NIL
;;
;; Backs music.volume(VOL)/sfx.volume(VOL) called with NO channel argument
;; at all. The mask is a 16-bit-relevant bitmask, bit N set meaning channel
;; N was last assigned a sound by THIS namespace's .play() (see
;; emit_vircon32_claim_channel_static() in v32lua.c, and the matching
;; inline claim/release block in __builtin_vircon32_play/_sfx_play above,
;; for how the mask gets populated). Every set bit's channel gets its
;; SPU_ChannelVolume written; an empty mask (nothing has played on this
;; namespace yet) makes this a no-op, which is the correct behaviour --
;; there is nothing to apply the volume to.
;;
;; This intentionally does NOT touch SPU_GlobalVolume. Reaching the true
;; global port is music.volume(VOL, -1)/sfx.volume(VOL, -1), handled by
;; __builtin_vircon32_volume above instead.
;;
;; Register use: R1 = resolved volume (kept live, never compared directly).
;; R2 = mask address, then free once the mask value is loaded into R3
;; (kept live for the whole loop). R4 = channel counter 0-15 (kept live).
;; R5/R6/R7 are throwaway per-iteration scratch.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

    ;@fn __builtin_vircon32_volume_mask clobbers=R0-R7
__builtin_vircon32_volume_mask:
    PUSH  BP
    MOV   BP, SP

    ;; --- 1. Resolve the volume: nil -> 1.0 ---
    MOV   R1, [BP+2]
    MOV   R2, R1
    IEQ   R2, BOXED_NIL
    JF    R2, _vircon32_volmask_val_ready
    MOV   R1, 1.0

_vircon32_volmask_val_ready:
    ;; --- 2. Load this namespace's channel-ownership mask ---
    MOV   R2, [BP+3]              ; mask RAM address (raw hardware int)
    MOV   R3, [R2]                ; mask value: bit N set = channel N is ours

    ;; --- 3. For each set bit 0-15, select that channel and set its volume ---
    MOV   R4, 0                   ; channel counter

_vircon32_volmask_loop:
    MOV   R5, R4
    IGE   R5, 16
    JT    R5, _vircon32_volmask_done

    MOV   R6, 1
    SHL   R6, R4                  ; R6 = 1 << channel
    MOV   R7, R3
    AND   R7, R6
    JF    R7, _vircon32_volmask_next   ; bit clear -> not ours, skip

    OUT   SPU_SelectedChannel, R4
    OUT   SPU_ChannelVolume, R1

_vircon32_volmask_next:
    IADD  R4, 1
    JMP   _vircon32_volmask_loop

_vircon32_volmask_done:
    MOV   R0, BOXED_NIL

    MOV   SP, BP
    POP   BP
    RET  

; ============================================================================
; __builtin_vircon32_memcard_get_title: read the memcard's title block back
; as a Lua TABLE -- the no-argument memcard.title() form
; ============================================================================
; Takes NO arguments.
;
; Returns: R0 = a fresh table whose elements 1..20 are ALL
; VIRCON32_MEMCARD_TITLE_DISPLAY_WORDS (20) title words, in order --
; element i is the word at VIRCON32_MEMCARD_BASE + (i-1), i.e. memcard
; position -24 + (i-1).
;
; THE TITLE BLOCK IS AN ID FIELD, NOT DISPLAY TEXT: this is a FIXED-WIDTH
; transaction. All 20 words are read, verbatim, every call -- no early
; exit at a zero word, no conversion loss. Each element is its word CIF'd:
; CIF reads the register's content as an INTEGER and yields that value as
; a Lua number, so 'M' (raw bits 0x0000004D) becomes the number 77, and a
; zero pad word becomes 0. A plain number every time, whatever the bits
; resemble -- CIF never consults the NaN-box tags, so a word whose bits
; happen to look like a string/boolean/nil encoding still comes back as a
; number. (Words >= 0x80000000 come back negative: 32-bit signed.)
;
; WHY A TABLE AND NOT A STRING: this VM's strings are NUL-terminated and
; every string operation (string.len, ==, .., print, string.sub) scans to
; the first zero word -- a string return made all words after the first
; zero invisible, so two IDs differing only there were indistinguishable.
; A table has no such blind spot: zero words are visible element 0s, and
; words past them are ordinary comparable elements. It also restores the
; readable-char-code property the raw-words writer change took away from
; memcard.load(-24): element 1 for a title starting 'M' is 77.
;
; == ON TABLES IS IDENTITY in this VM (only strings compare by content):
; two reads of the same title are different table objects and never ==
; each other. ID checks are an element-wise loop in Lua -- see the
; title/ID unit test for the pattern.
;
; NO NIL-OOM PATH: __builtin_table_new and __builtin_table_set both halt
; through __oom_handler on heap exhaustion and never return 0 -- the same
; contract __builtin_vircon32_memcard_load_table already has. This
; routine returns a valid table or never returns at all.
;
; Register/stack discipline: __builtin_table_set is called 20 times, so
; per this file's standing rule nothing the loop needs is trusted to
; survive a CALL in a plain register -- the table ([BP-1]), the card read
; pointer ([BP-2]), and the 1-based element index ([BP-3]) all live in
; stack slots, reloaded/rewritten around every iteration, the same shape
; as load_table's restore loop. (table_set's own header says it preserves
; R1-R8; that is deliberately NOT relied on here.) The KEY is pushed as a
; real Lua number -- CIF of the index -- because table_set treats its key
; as a boxed value; a raw integer pushed bare would CFI (float-read) to
; 0. Same reason node_table.c's constructor CIFs its array keys.
;
; NOTE: 20 is a LITERAL here for the same reason as in
; __builtin_vircon32_memcard_title below -- emit_variable_map() in
; emit.c only exports BASE/DATA_BASE/CURSOR_ADDR/END as %defines for the
; assembler, not word-count constants. If a future change adds an export
; for this one, both routines should switch over together.
;
; Verified (20261014-dev) on the real CPU core via the headless harness:
; strings, oversize/empty/sparse/fractional input, and the round trip with
; the table form of memcard.title().
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

    ;@fn __builtin_vircon32_memcard_get_title clobbers=R0-R1,R9-R13
__builtin_vircon32_memcard_get_title:
    PUSH  BP
    MOV   BP, SP

    ;; --- 1. Fresh table (fatal on OOM via __oom_handler -- no branch here)
    CALL  __builtin_table_new
    PUSH  R0                            ; [BP-1] = the new table

    ;; --- 2. Loop state in stack slots, not registers ---
    MOV   R1, VIRCON32_MEMCARD_BASE
    PUSH  R1                            ; [BP-2] = card read pointer
    MOV   R1, 1
    PUSH  R1                            ; [BP-3] = element index (1-based)

_memcard_get_title_loop:
    MOV   R1, [BP-3]
    IGT   R1, 20
    JT    R1, _memcard_get_title_done

    ;; --- 3. One __builtin_table_set per word: [BP+4]=table,
    ;; [BP+3]=key, [BP+2]=value -- same stack shape as load_table's
    ;; restore loop.
    MOV   R1, [BP-1]
    PUSH  R1                            ; table
    MOV   R1, [BP-3]
    CIF   R1                            ; key as a real Lua number
    PUSH  R1
    MOV   R1, [BP-2]
    MOV   R1, [R1]                      ; the raw title word
    CIF   R1                            ; ... its integer value, as a number
    PUSH  R1
    CALL  __builtin_table_set
    IADD  SP, 3

    MOV   R1, [BP-2]
    IADD  R1, 1
    MOV   [BP-2], R1
    MOV   R1, [BP-3]
    IADD  R1, 1
    MOV   [BP-3], R1
    JMP   _memcard_get_title_loop

_memcard_get_title_done:
    MOV   R0, [BP-1]                    ; return the table

    MOV   SP, BP
    POP   BP
    RET  


; ============================================================================
; __builtin_vircon32_memcard_title: set the memcard's title into the full
; VIRCON32_MEMCARD_TITLE_DISPLAY_WORDS (20) title block -- from a STRING
; or, new, from a TABLE. Metadata (VIRCON32_MEMCARD_METADATA_WORDS,
; including the cursor at VIRCON32_MEMCARD_CURSOR_ADDR) lives in its own
; separate region right after the title -- see the header comment block
; near the top of this file -- so it's never touched here regardless of
; title length or form.
; ============================================================================
;
; NOW ACCEPTS A STRING OR A TABLE: the write form is memcard.title(str|t),
; and the runtime DISPATCHES ON THE ARGUMENT'S NaN-BOX TAG at entry --
; the two forms deliberately share one entry point (Lua call syntax can't
; distinguish them any other way), and a non-literal argument's type is a
; runtime property the compiler can't know at compile time, so the
; dispatch lives here, not in the emitter.
;
;   STRING (ROM or RAM): behavior unchanged -- one raw word per
;   character, NUL-stop against STR itself, truncate at 20 characters,
;   zero-pad the remainder (details in the CIF-DROPPED note below).
;
;   TABLE: the write-side mirror of the read form -- a FIXED-WIDTH
;   20-word transaction built from elements 1..20. For each i the
;   routine evaluates t[i] via __builtin_table_get (so a metatable's
;   __index answers misses exactly as a Lua t[i] would):
;     - a NUMBER element is CFI'd and its integer bits become the word
;       (77.0 -> 0x0000004D, -1.0 -> 0xFFFFFFFF) -- the exact inverse
;       of the read form's per-element CIF, so a value read back from
;       memcard.title() ROUND-TRIPS BIT-EXACTLY, signed words included
;       (the read form reports words >= 0x80000000 as negative 32-bit
;       values, and writing that negative number reproduces the same
;       bits);
;     - an ABSENT element (nil) writes a zero word -- the table form's
;       zero-pad;
;     - elements past 20 are ignored;
;     - any other element value (string/boolean/table/function) is a
;       RUNTIME ERROR -- no silent fallback: the routine traps through
;       __runtime_error_bad_memcard_title (below), the same family as
;       table.s's __runtime_error_not_table. The offending value is
;       printed before the halt.
;   This closes the write-side blind spot: a Lua string in this VM
;   cannot carry a zero word past its terminator, so an ID with an
;   interior zero word could previously only be written raw via
;   memcard.save(value, position) at positions -5..-24. The table form
;   writes all 20 words verbatim -- and memcard.title(memcard.title())
;   is a bit-exact no-op round trip, the ID update pattern.
;
;   ANYTHING ELSE (number, boolean, nil, function): the same runtime
;   error. This is new coverage, not a regression: the old body fed the
;   argument to __unbox_string unvalidated, so a wrong-type write was
;   undefined behavior; the dispatch turns that into a loud trap.
;
; CIF DROPPED -- CHARACTERS ARE STORED RAW NOW: the string path's copy
; loop used to CIF each character into a float before its store, so the
; card words held float encodings of char codes. The title block is an
; ID field, so both directions are bit-exact word copies now: characters
; go in as the same raw words this VM's own internal string
; representation uses (one word per character -- see
; __builtin_string_len; NOT a packed byte string, and no longer a float
; encoding either), and __builtin_vircon32_memcard_get_title above
; reads them back verbatim (each word CIF'd into its integer value as a
; table element). The table path's per-element CFI is the same word
; convention arriving from numbers instead of characters.
;
; COMPATIBILITY NOTE: cards written by the pre-drop writer carry float
; bit patterns in their title words. memcard.title() now reports those
; words' integer values (e.g. 0x42820000 for a CIF'd 'A' reads back as
; the large number 1111636864, not 65), and memcard.load(-5..-24) reads
; them as the floats they are. A card still carrying an old-format title
; should be re-titled once before its title is used as an ID.
;
; STRING-PATH WRITE SHAPE UNCHANGED: the copy still stops at STR's OWN
; NUL terminator -- a short Lua string's buffer must not be read past
; its end -- and zero-pads the remainder of the 20. An ID containing a
; zero word mid-stream cannot be written through the STRING form (a Lua
; string in this VM cannot carry one past its terminator); use the table
; form -- or raw memcard.save(value, position) at positions -5..-24,
; which memcard.title() then reads back verbatim.
;
; NOTE: 20 is a LITERAL here, not the symbolic VIRCON32_MEMCARD_TITLE_
; DISPLAY_WORDS constant -- emit_variable_map() in emit.c only exports
; BASE/DATA_BASE/CURSOR_ADDR/END as %defines for the assembler, not word-
; count constants. If a future change adds an export for this one, this
; is a good spot to switch it over (and the table path's two 20s and the
; getter's two should follow).
;
; Stack layout relative to BP:
; [BP+2]: title -- a boxed string (ROM or RAM) or a boxed table
;
; Returns: R0 = BOXED_NIL
;
; STRING path: truncates to 20 characters if the string is longer;
; zero-pads the remainder of the 20 if shorter. TABLE path: elements
; 1..20 only, nil/absent = zero pad.
;
; Register use: STRING path -- R1 = source pointer (from __unbox_string),
; R2 = dest pointer (starts at VIRCON32_MEMCARD_BASE), R3 = characters
; written so far (0-20), R4/R5 per-iteration scratch; no CALLs on that
; path, so plain registers are safe. TABLE path -- __builtin_table_get
; callee-saves only R1-R5 and clobbers R6-R8, so per this file's standing
; rule nothing the loop needs is trusted to survive a CALL in a plain
; register: the 1..20 element index lives in stack slot [BP-1], the table
; itself is the caller's [BP+2] (stable across CALLs), and the
; destination address is recomputed from the index each iteration.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

__builtin_vircon32_memcard_title:
    PUSH  BP
    MOV   BP, SP

    ;; --- 0. Tag dispatch: string or table? A non-literal argument's ---
    ;; --- type is a runtime property, so this is decided here, not in ---
    ;; --- the emitter. The dispatch doubles as the write form's first  ---
    ;; --- ever argument validation -- a wrong type used to reach       ---
    ;; --- __unbox_string unvalidated.                                   ---
    MOV   R1, [BP+2]
    AND   R1, BOXED_DATA
    IEQ   R1, BOXED_TABLE
    JT    R1, _memcard_title_table_init

    MOV   R1, [BP+2]
    AND   R1, BOXED_DATA
    IEQ   R1, BOXED_ROMSTRING
    JT    R1, _memcard_title_string_path

    MOV   R1, [BP+2]
    AND   R1, BOXED_DATA
    IEQ   R1, BOXED_RAMSTRING
    JF    R1, _memcard_title_arg_error   ; number/function/... -> error

    ;; nil/false/true share the RAM-string tag with payload < 4 -- only
    ;; payload >= 4 is a real RAM string (same check as __builtin_eq in
    ;; string.s and __table_get_non_table in table.s).
    MOV   R1, [BP+2]
    AND   R1, BOXED_PAYLOAD
    ILT   R1, 4
    JT    R1, _memcard_title_arg_error

_memcard_title_string_path:
    MOV   R0, [BP+2]
    CALL  __unbox_string           ; R0 = raw char-array address
    MOV   R1, R0                   ; R1 = source pointer

    MOV   R2, VIRCON32_MEMCARD_BASE   ; R2 = dest pointer (title start)
    MOV   R3, 0                    ; R3 = characters written so far

_memcard_title_copy_loop:
    MOV   R4, R3
    IGE   R4, 20
    JT    R4, _memcard_title_done     ; wrote all 20 -> nothing to pad either
    MOV   R4, [R1]
    IEQ   R4, 0
    JT    R4, _memcard_title_pad_loop ; hit NUL -> pad the remainder with 0
    MOV   R4, [R1]                    ; IEQ above clobbered R4 with its 0/1
                                       ; result -- reload the real character
    MOV   [R2], R4                    ; raw character word (CIF dropped)
    IADD  R1, 1
    IADD  R2, 1
    IADD  R3, 1
    JMP   _memcard_title_copy_loop

_memcard_title_pad_loop:
    MOV   R4, R3
    IGE   R4, 20
    JT    R4, _memcard_title_done
    MOV   R5, 0
    MOV   [R2], R5
    IADD  R2, 1
    IADD  R3, 1
    JMP   _memcard_title_pad_loop

_memcard_title_done:
    MOV   R0, BOXED_NIL

    MOV   SP, BP
    POP   BP
    RET  

;; ---------------------------------------------------------------------------
;; TABLE path: elements 1..20 of the caller's table become the 20 title
;; words, one __builtin_table_get per word -- the mirror of the read
;; form's one __builtin_table_set per word. The table is the caller's
;; [BP+2] (stable across CALLs); the 1-based element index lives in
;; stack slot [BP-1] because table_get callee-saves only R1-R5 and
;; clobbers R6-R8 -- nothing the loop needs is trusted to survive a
;; CALL in a plain register, this file's standing rule.
;; ---------------------------------------------------------------------------
_memcard_title_table_init:
    MOV   R1, 1
    PUSH  R1                            ; [BP-1] = element index (1-based)

_memcard_title_table_loop:
    MOV   R1, [BP-1]
    IGT   R1, 20
    JT    R1, _memcard_title_done       ; SP reset below BP happens via
                                       ; MOV SP,BP at _memcard_title_done
                                       ; -- the [BP-1] slot is reclaimed
    ;; t[i] via __builtin_table_get: [BP+3] = table, [BP+2] = key. The
    ;; key is pushed as a real Lua number (CIF of the index) -- a raw
    ;; integer pushed bare would CFI (float-read) to 0, same reason as
    ;; the getter's keys.
    MOV   R1, [BP+2]
    PUSH  R1                            ; table (boxed)
    MOV   R1, [BP-1]
    CIF   R1                            ; key as a real Lua number
    PUSH  R1
    CALL  __builtin_table_get
    IADD  SP, 2                         ; R0 = t[i] (BOXED_NIL if absent)

    ;; absent element -> zero word, the table form's zero-pad
    MOV   R1, R0
    IEQ   R1, BOXED_NIL
    JT    R1, _memcard_title_store_zero

    ;; numbers are raw float bits; every other type is NaN-boxed. The
    ;; same "is it a number" test table.s's array path uses.
    MOV   R1, R0
    AND   R1, NAN_VALUE
    IEQ   R1, NAN_VALUE
    JT    R1, _memcard_title_elem_error

    ;; number -> its integer bits: the inverse of the read form's
    ;; per-element CIF, so a value read from memcard.title() round-trips
    ;; bit-exactly, signed words included.
    CFI   R0
    JMP   _memcard_title_store_word

_memcard_title_store_zero:
    MOV   R0, 0

_memcard_title_store_word:
    MOV   R1, VIRCON32_MEMCARD_BASE     ; dest = BASE + (i-1), recomputed
    MOV   R2, [BP-1]                    ; from the index each iteration
    IADD  R1, R2
    ISUB  R1, 1
    MOV   [R1], R0

    MOV   R1, [BP-1]
    IADD  R1, 1
    MOV   [BP-1], R1
    JMP   _memcard_title_table_loop

;; --- runtime error: the argument is neither a string nor a table ---
;; --- (R0 = the offending argument for the panic below)             ---
_memcard_title_arg_error:
    MOV   R0, [BP+2]
    JMP   __runtime_error_bad_memcard_title

;; --- runtime error: t[i] holds a non-number, non-nil value ---
;; --- (R0 = the offending element, still untouched above)  ---
_memcard_title_elem_error:
    JMP   __runtime_error_bad_memcard_title


; ============================================================================
; __runtime_error_bad_memcard_title: panic for a memcard.title(...) write
; whose argument is neither a string nor a table, or whose table argument
; holds a non-number element. Same family as table.s's
; __runtime_error_not_table and exec.s's __runtime_error_not_callable: red
; screen, PANIC banner, message, the offending value, then __panic_halt.
; Never returns -- no frame unwinding needed.
; Incoming: R0 = the offending value.
; ============================================================================
__runtime_error_bad_memcard_title:
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 1
    MOV   [BP-1], R0                ; the value that couldn't be a title

    MOV   R0, 0xFF000080            ; A=FF, B=00, G=00, R=80 (see hex_to_v32_color)
    OUT   GPU_ClearColor, R0
    OUT   GPU_Command, GPUCommand_ClearScreen

    MOV   R0, 20
    PUSH  R0
    MOV   R0, 0
    PUSH  R0
    MOV   R0, __const_str_panic_banner
    OR    R0, BOXED_ROMSTRING
    PUSH  R0
    CALL  __builtin_print

    MOV   R0, 20
    PUSH  R0
    MOV   R0, 20
    PUSH  R0
    MOV   R0, __const_str_err_bad_memcard_title
    OR    R0, BOXED_ROMSTRING
    PUSH  R0
    CALL  __builtin_print

    MOV   R0, 20
    PUSH  R0
    MOV   R0, 40
    PUSH  R0
    MOV   R0, [BP-1]
    PUSH  R0
    CALL  __builtin_print

    JMP   __panic_halt

;; ============================================================================
;; __builtin_vircon32_memcard_append: memcard.save(value) with NO position
;; -- auto-append through the persistent on-card cursor
;; ============================================================================
;; Stack layout relative to BP:
;; [BP+2]: value (any Lua value)
;;
;; Returns: R0 = value, unchanged (same "returns what you gave" convention
;; as memcard.save(value, position) and play()/sfx.play() elsewhere).
;;
;; THE CURSOR (VIRCON32_MEMCARD_CURSOR_ADDR, the memcard's own last title
;; word) is a raw hardware integer -- a WORD OFFSET from
;; VIRCON32_MEMCARD_DATA_BASE, NOT a Lua value -- and lives ON THE CARD
;; ITSELF, not in Vircon32 RAM. That is the entire point: it survives a
;; reboot, so repeated no-position saves keep extending a log across many
;; play sessions instead of overwriting position 0 every single run. A
;; blank/never-written card reads this word as its all-zero bit pattern,
;; which is exactly the float 0.0 -- so a fresh card's cursor starts at 0
;; with no separate "is this formatted" check needed.
;;
;; ENTRY FORMAT written at the cursor's current word:
;;   Real Lua string (ROM or RAM):  [TAG_STRING][length][char words...]
;;                                  -- 2 + length words total.
;;   Everything else (number,
;;   boolean, nil, table, function): [TAG_SCALAR][raw boxed value]
;;                                  -- 2 words total. A table/function is
;;                                  stored as a raw pointer here, same
;;                                  within-a-run-only caveat as the
;;                                  explicit-position save form -- see the
;;                                  safety note in v32lua.c.
;;
;; The classification test (real ROM/RAM string vs. everything else) is
;; the same tag+payload check __builtin_strcat uses to decide whether an
;; operand needs tostring() coercion -- see that routine above for the
;; original.
;;
;; CALL-CLOBBERS-EVERYTHING DISCIPLINE: this routine calls
;; __builtin_string_len and __unbox_string internally. Per this file's
;; standing rule (plain registers do not survive a CALL), every value that
;; must outlive one of those calls is PUSHed onto the stack as a local
;; instead of being trusted to survive in a register -- old_cursor at
;; [BP-1], base_addr at [BP-2], and (string path only) length at [BP-3].
;; These are never individually POPped; MOV SP, BP at the end discards
;; them wholesale, which is safe regardless of which path was taken.
;;
;; UNVERIFIED: written to the same standard as the rest of this file, but
;; not yet run through v32sim. Flagging per this project's usual
;; verification discipline.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

__builtin_vircon32_memcard_append:
    PUSH  BP
    MOV   BP, SP

    ;; --- 1. Read the persistent on-card cursor ---
    ;; Stored as a real Lua number (see the CIF at the bottom of this
    ;; routine) so memcard.load(-1)/memcard[-1] can read it back directly
    ;; like any other value -- CFI it back to a raw hardware int here for
    ;; this routine's own arithmetic.
    MOV   R1, [VIRCON32_MEMCARD_CURSOR_ADDR]
    CFI   R1
    PUSH  R1                            ; [BP-1] = old_cursor

    ;; --- 2. This entry's base address, saturated to stay on-card ---
    MOV   R2, VIRCON32_MEMCARD_DATA_BASE
    IADD  R2, R1
    MOV   R3, R2
    IGT   R3, VIRCON32_MEMCARD_END
    JF    R3, _memcard_append_addr_ok
    MOV   R2, VIRCON32_MEMCARD_END      ; card is full: keep overwriting the last slot
_memcard_append_addr_ok:
    PUSH  R2                            ; [BP-2] = base_addr

    ;; --- 3. Classify the value: real string, table, or anything else ---
    MOV   R1, [BP+2]
    MOV   R3, R1
    AND   R3, BOXED_DATA
    IEQ   R3, BOXED_ROMSTRING
    JT    R3, _memcard_append_is_string
    IEQ   R3, BOXED_RAMSTRING
    JF    R3, _memcard_append_scalar
    MOV   R3, R1
    AND   R3, BOXED_PAYLOAD
    ILT   R3, 4                         ; payload < 4 -> nil/true/false, not a real string
    JT    R3, _memcard_append_scalar
    JMP   _memcard_append_is_string

_memcard_append_scalar:
    ;; A table falls through to here from the classify block above (it
    ;; matches neither BOXED_ROMSTRING nor BOXED_RAMSTRING), so it gets one
    ;; more check before being treated as an opaque scalar -- see
    ;; _memcard_append_is_table below for its own dump format.
    MOV   R3, R1
    AND   R3, BOXED_DATA
    IEQ   R3, BOXED_TABLE
    JT    R3, _memcard_append_is_table

    ;; --- SCALAR PATH: [TAG_SCALAR][raw value] -- 2 words ---
    ;; The tag is written as a proper Lua number (CIF'd) so a caller can
    ;; compare "memcard.load(pos) == 0" directly, the same way any other
    ;; memcard word behaves -- a bare hardware-integer 0 happens to share
    ;; 0.0's all-zero bit pattern, so this CIF is a no-op for TAG_SCALAR
    ;; specifically, but it's here for symmetry with TAG_STRING below,
    ;; where the equivalent CIF is NOT a no-op.
    MOV   R5, [BP-2]                    ; base_addr
    MOV   R7, 0                         ; TAG_SCALAR
    CIF   R7
    MOV   [R5], R7
    IADD  R5, 1
    MOV   R6, [BP+2]                    ; the value itself is already a
    MOV   [R5], R6                      ; properly boxed Lua value -- no CIF

    MOV   R1, [BP-1]                    ; old_cursor
    IADD  R1, 2                         ; new_cursor = old_cursor + 2
    JMP   _memcard_append_store_cursor

_memcard_append_is_string:
    ;; --- STRING PATH: [TAG_STRING][length][char words...] ---
    MOV   R0, [BP+2]
    PUSH  R0
    CALL  __builtin_string_len          ; R0 = length, boxed
    IADD  SP, 1
    CFI   R0                            ; R0 = length, hardware integer
    PUSH  R0                            ; [BP-3] = length (raw int -- loop counter)

    MOV   R0, [BP+2]
    CALL  __unbox_string                ; R0 = raw char-array pointer
    MOV   R4, R0                        ; R4 = source pointer (no more CALLs after this)

    MOV   R5, [BP-2]                    ; R5 = dest pointer, starts at base_addr
    MOV   R6, [BP-3]                    ; R6 = remaining char count (raw int)

    ;; TAG_STRING (1) and the length word both need CIF before storage --
    ;; unlike TAG_SCALAR's 0, a raw hardware integer 1 (0x00000001) is a
    ;; completely different bit pattern from the Lua float 1.0
    ;; (0x3F800000), so without this a caller comparing
    ;; "memcard.load(pos) == 1" against an unconverted tag would never
    ;; match.
    MOV   R7, 1
    CIF   R7                            ; TAG_STRING, as a real Lua number
    MOV   [R5], R7
    IADD  R5, 1

    MOV   R7, R6                        ; a COPY of the length -- R6 itself
    CIF   R7                            ; stays a raw int; the loop below
    MOV   [R5], R7                      ; still needs it that way
    IADD  R5, 1

_memcard_append_str_copy:
    MOV   R7, R6
    IEQ   R7, 0
    JT    R7, _memcard_append_str_done
    ;; Character words in this VM's internal string storage are raw
    ;; hardware-integer ASCII codes, NOT boxed Lua floats -- confirmed by
    ;; string.byte()'s own CIF on this exact read (see __builtin_string_
    ;; byte above). Copying them verbatim would embed that same raw-int
    ;; mismatch into the memcard; CIF each one on the way in so
    ;; memcard.load() at a char's position reads back the same value
    ;; string.byte() would report for it.
    MOV   R7, [R4]
    CIF   R7
    MOV   [R5], R7
    IADD  R4, 1
    IADD  R5, 1
    ISUB  R6, 1
    JMP   _memcard_append_str_copy

_memcard_append_str_done:
    MOV   R1, [BP-1]                    ; old_cursor
    IADD  R1, 2                         ; +tag +length words
    MOV   R6, [BP-3]                    ; original length
    IADD  R1, R6                        ; new_cursor = old_cursor + 2 + length
    JMP   _memcard_append_store_cursor

_memcard_append_is_table:
    ;; --- TABLE PATH: [TAG_TABLE][pair_count][k1][v1]...[kN][vN] ---
    ;; A RAW DUMP of the table's hash-bucket storage -- NOT a general
    ;; recursive serializer. See the design note in v32lua.c
    ;; (intrinsics_vircon32_memcard.c's header comment) for the full
    ;; rationale; in short: this compiler's array-part fast path is
    ;; currently unreachable in practice (__builtin_table_set_reallocate
    ;; above is a TODO stub -- capacity is always 0), so every table,
    ;; array-shaped or not, already lives entirely in the hash-bucket
    ;; chain walked here. Each key/value word is copied exactly as it's
    ;; boxed -- a number/boolean/nil round-trips correctly forever, but a
    ;; string/table/function key or value is copied as its raw RAM
    ;; pointer, valid only within this run, same as the scalar path above.
    ;; Restored by __builtin_vircon32_memcard_load_table below.
    ;; Pairs are enumerated with __builtin_next (array part, then hash
    ;; part; nil-valued entries skipped). __builtin_next preserves R1 and
    ;; R3-R8, returning the key in R0 and the value in R2.

    ;; --- Pass 1: count live pairs ---
    MOV   R3, 0                         ; R3 = running pair count
    MOV   R0, BOXED_NIL
_memcard_append_table_count_loop:
    PUSH  R0                            ; key
    MOV   R1, [BP+2]
    PUSH  R1                            ; table
    CALL  __builtin_next
    IADD  SP, 2
    MOV   R4, R0
    IEQ   R4, BOXED_NIL
    JT    R4, _memcard_append_table_count_done
    IADD  R3, 1
    JMP   _memcard_append_table_count_loop

_memcard_append_table_count_done:
    ;; --- Write the header: [TAG_TABLE][pair_count] ---
    MOV   R5, [BP-2]                    ; base_addr
    MOV   R7, 2                         ; TAG_TABLE
    CIF   R7
    MOV   [R5], R7
    IADD  R5, 1
    MOV   R7, R3
    CIF   R7
    MOV   [R5], R7
    IADD  R5, 1

    ;; --- Pass 2: write each Key/Value pair ---
    MOV   R0, BOXED_NIL
_memcard_append_table_write_pair:
    PUSH  R0
    MOV   R1, [BP+2]
    PUSH  R1
    CALL  __builtin_next
    IADD  SP, 2
    MOV   R4, R0
    IEQ   R4, BOXED_NIL
    JT    R4, _memcard_append_table_write_done
    MOV   [R5], R0                      ; key (boxed, verbatim)
    IADD  R5, 1
    MOV   [R5], R2                      ; value
    IADD  R5, 1
    JMP   _memcard_append_table_write_pair

_memcard_append_table_write_done:
    MOV   R1, [BP-1]                    ; old_cursor
    IADD  R1, 2                         ; +tag +pair_count words
    IADD  R1, R3                        ; +pair_count words for keys
    IADD  R1, R3                        ; +pair_count words for values
    JMP   _memcard_append_store_cursor

_memcard_append_store_cursor:
    ;; Best-effort saturate: once a no-position save would run the cursor
    ;; past the end of the card, pin it at the last valid data offset so
    ;; every further no-position save just keeps overwriting that final
    ;; slot instead of the cursor wandering out of range forever. This
    ;; clamp compares raw hardware integers -- CIF happens AFTER, right
    ;; before the store.
    MOV   R3, R1
    MOV   R2, VIRCON32_MEMCARD_END
    ISUB  R2, VIRCON32_MEMCARD_DATA_BASE
    IGT   R3, R2
    JF    R3, _memcard_append_cursor_ok
    MOV   R1, R2
_memcard_append_cursor_ok:
    CIF   R1                            ; store as a real Lua number --
    MOV   [VIRCON32_MEMCARD_CURSOR_ADDR], R1  ; see the read at the top

    ;; --- Return the original value, matching save()'s convention ---
    MOV   R0, [BP+2]

    MOV   SP, BP
    POP   BP
    RET  

;; ============================================================================
;; __builtin_vircon32_memcard_load_table: reconstruct a table previously
;; written by memcard.save(a_table)'s auto-append form
;; ============================================================================
;; Stack layout relative to BP:
;; [BP+2]: position (Lua float, required -- see v32lua.c for why there's
;;         no "no position" default for this one)
;;
;; Returns: R0 = a freshly built table (boxed), or BOXED_NIL if the tag
;; word at POSITION isn't TAG_TABLE.
;;
;; Reads [TAG_TABLE][pair_count][k1][v1]...[kN][vN] starting at POSITION
;; (see _memcard_append_is_table above for how that layout gets written),
;; validates the tag, then calls __builtin_table_new once and
;; __builtin_table_set once per pair to rebuild an equivalent table. Each
;; restored key/value is used exactly as stored -- no recursive
;; reconstruction of nested tables/strings/functions; see the safety note
;; in v32lua.c.
;;
;; CALL-CLOBBERS-EVERYTHING DISCIPLINE: __builtin_table_new and
;; __builtin_table_set are both called from inside the restore loop below,
;; so nothing this routine needs afterward is trusted to survive in a
;; plain register across either call -- entry_address, pair_count, the
;; new table, the current pair pointer, and the loop counter are all kept
;; in named stack slots ([BP-1] through [BP-5]) and reloaded/rewritten
;; around every CALL, same discipline as __builtin_vircon32_memcard_append
;; above.
;;
;; UNVERIFIED: written to the same standard as the rest of this file, but
;; not yet run through v32sim.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

__builtin_vircon32_memcard_load_table:
    PUSH  BP
    MOV   BP, SP

    ;; --- 1. Resolve + clamp the entry address (same as every other
    ;; memcard position argument in this file) ---
    MOV   R1, [BP+2]
    CFI   R1
    IADD  R1, VIRCON32_MEMCARD_DATA_BASE
    MOV   R2, R1
    ILT   R2, VIRCON32_MEMCARD_BASE
    JF    R2, _memcard_loadtable_lo_ok
    MOV   R1, VIRCON32_MEMCARD_BASE
_memcard_loadtable_lo_ok:
    MOV   R2, R1
    IGT   R2, VIRCON32_MEMCARD_END
    JF    R2, _memcard_loadtable_hi_ok
    MOV   R1, VIRCON32_MEMCARD_END
_memcard_loadtable_hi_ok:
    PUSH  R1                            ; [BP-1] = entry address

    ;; --- 2. Validate the tag before trusting anything after it ---
    MOV   R2, [R1]
    CFI   R2
    IEQ   R2, 2                         ; TAG_TABLE
    JF    R2, _memcard_loadtable_bad_tag

    ;; --- 3. Pair count ---
    MOV   R2, [R1+1]
    CFI   R2
    PUSH  R2                            ; [BP-2] = pair_count

    ;; --- 4. Build a fresh table -- nothing live in registers to lose
    ;; here, so no special handling needed around this CALL ---
    CALL  __builtin_table_new
    PUSH  R0                            ; [BP-3] = the new table

    ;; --- 5. Restore each pair. The current pair pointer and the "pairs
    ;; restored so far" counter both get clobbered by __builtin_table_set,
    ;; so both live in stack slots, reloaded/rewritten around every
    ;; iteration rather than trusted to survive in R1/R3. ---
    MOV   R1, [BP-1]
    IADD  R1, 2                         ; first Key word
    PUSH  R1                            ; [BP-4] = current pair pointer
    MOV   R3, 0
    PUSH  R3                            ; [BP-5] = pairs restored so far

_memcard_loadtable_restore_loop:
    MOV   R4, [BP-5]
    MOV   R5, [BP-2]
    IGE   R4, R5
    JT    R4, _memcard_loadtable_done

    MOV   R1, [BP-4]
    MOV   R6, [R1]                      ; Key
    MOV   R7, [R1+1]                    ; Value

    MOV   R0, [BP-3]
    PUSH  R0                            ; Arg: table  (lands at [BP+4])
    PUSH  R6                            ; Arg: key    (lands at [BP+3])
    PUSH  R7                            ; Arg: value  (lands at [BP+2])
    CALL  __builtin_table_set
    IADD  SP, 3

    MOV   R1, [BP-4]
    IADD  R1, 2                         ; advance to next Key/Value pair
    MOV   [BP-4], R1

    MOV   R3, [BP-5]
    IADD  R3, 1
    MOV   [BP-5], R3

    JMP   _memcard_loadtable_restore_loop

_memcard_loadtable_done:
    MOV   R0, [BP-3]                    ; return the reconstructed table
    JMP   _memcard_loadtable_return

_memcard_loadtable_bad_tag:
    MOV   R0, BOXED_NIL

_memcard_loadtable_return:
    MOV   SP, BP
    POP   BP
    RET  

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; __builtin_vircon32_tilemap_get
;;
;; Stack: [BP+2]=rom_ptr  [BP+3]=ram_ptr_addr  [BP+4]=width  [BP+5]=height
;;        [BP+6]=x        [BP+7]=y
;; Returns: R0 = tile value at (x,y), boxed, or BOXED_NIL if out of bounds.
;;
;; Reads the RAM copy if this tilemap has been promoted (ram_ptr_addr's
;; word is nonzero), otherwise reads straight out of ROM. ram_ptr_addr's
;; current value is never trusted after a destructive compare -- see the
;; scratch-copy note below.
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

    ;@fn __builtin_vircon32_tilemap_get clobbers=R0-R8
__builtin_vircon32_tilemap_get:
    PUSH  BP
    MOV   BP, SP

    MOV   R1, [BP+6]        ; x
    CFI   R1                ; R1 = round(x)

    MOV   R2, [BP+7]        ; y
    CFI   R2                ; R2 = round(y)

    MOV   R3, [BP+4]        ; width
    MOV   R4, [BP+5]        ; height

    ;; Bounds check x
    MOV   R5, R1
    ILT   R5, 0
    JT    R5, _v32_tmget_invalid
    MOV   R5, R1
    IGE   R5, R3
    JT    R5, _v32_tmget_invalid

    ;; Bounds check y
    MOV   R5, R2
    ILT   R5, 0
    JT    R5, _v32_tmget_invalid
    MOV   R5, R2
    IGE   R5, R4
    JT    R5, _v32_tmget_invalid

    ;; index = y*width + x
    MOV   R5, R3
    IMUL  R2, R5
    IADD  R1, R2            ; R1 = index

    ;; Promotion check. R7 must survive the IEQ below (destructive), so
    ;; the compare runs on a throwaway copy (R8), never on R7 itself.
    MOV   R6, [BP+3]        ; ram_ptr_addr
    MOV   R7, [R6]          ; current ram_ptr (0 == not promoted)
    MOV   R8, R7
    IEQ   R8, BOXED_NIL
    JT    R8, _v32_tmget_from_rom

    MOV   R0, R7
    IADD  R0, R1
    MOV   R0, [R0]
    JMP   _v32_tmget_box

_v32_tmget_from_rom:
    MOV   R0, [BP+2]        ; rom_ptr
    IADD  R0, R1
    MOV   R0, [R0]

_v32_tmget_box:
    CIF   R0
    JMP   _v32_tmget_done

_v32_tmget_invalid:
    MOV   R0, BOXED_NIL

_v32_tmget_done:
    MOV   SP, BP
    POP   BP
    RET  

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; __builtin_vircon32_tilemap_set
;;
;; Stack: [BP+2]=rom_ptr  [BP+3]=ram_ptr_addr  [BP+4]=width  [BP+5]=height
;;        [BP+6]=x        [BP+7]=y             [BP+8]=v
;; Returns: R0 = v (boxed). Out-of-bounds is a silent no-op, mirroring the
;; TIC-80 mset() convention already established.
;;
;; Promotes on first call: __malloc's width*height words, copies the ROM
;; data across, then stores the promotion pointer. R1 (index) and R9
;; (value) are the only things still needed after the CALL, so they're the
;; only things spilled across it -- width/height/rom_ptr/ram_ptr_addr are
;; our OWN stack arguments and are safe across any CALL by construction,
;; so they're just reloaded fresh afterward instead of being spilled.
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

    ;@fn __builtin_vircon32_tilemap_set clobbers=R0-R12
__builtin_vircon32_tilemap_set:
    PUSH  BP
    MOV   BP, SP

    MOV   R1, [BP+6]
    CFI   R1                ; R1 = round(x)

    MOV   R2, [BP+7]
    CFI   R2                ; R2 = round(y)

    MOV   R3, [BP+4]        ; width
    MOV   R4, [BP+5]        ; height

    MOV   R9, [BP+8]        ; v
    CFI   R9                ; native tilemap values aren't clamped to 0-255
                            ; the way the TIC-80 layer clamps sprite ids --
                            ; region ids can run past that.

    ;; Bounds check x
    MOV   R5, R1
    ILT   R5, 0
    JT    R5, _v32_tmset_noop
    MOV   R5, R1
    IGE   R5, R3
    JT    R5, _v32_tmset_noop

    ;; Bounds check y
    MOV   R5, R2
    ILT   R5, 0
    JT    R5, _v32_tmset_noop
    MOV   R5, R2
    IGE   R5, R4
    JT    R5, _v32_tmset_noop

    ;; index = y*width + x
    MOV   R5, R3
    IMUL  R2, R5
    IADD  R1, R2             ; R1 = index

    MOV   R6, [BP+3]         ; ram_ptr_addr
    MOV   R7, [R6]           ; current ram_ptr (0 == not promoted)
    MOV   R8, R7
    IEQ   R8, BOXED_NIL
    JF    R8, _v32_tmset_store   ; already promoted -> store directly

    ;; --- Promote: malloc, copy ROM -> RAM, persist the pointer ---
    PUSH  R1                 ; spill index across the CALL
    PUSH  R9                 ; spill value across the CALL
    MOV   R10, R3
    IMUL  R10, R4            ; total cell count = __malloc's argument
    PUSH  R10
    CALL  __malloc
    IADD  SP, 1              ; clean up __malloc's own argument
    POP   R9                 ; restore value
    POP   R1                 ; restore index
    MOV   R7, R0             ; R7 = new RAM buffer

    MOV   R6, [BP+3]
    MOV   [R6], R7           ; persist the promotion pointer

    ;; Recompute total cell count fresh -- R10 is not trusted post-CALL,
    ;; but width/height (our own stack args) are.
    MOV   R10, [BP+4]
    MOV   R6,  [BP+5]
    IMUL  R10, R6

    MOV   R11, 0             ; copy index
    MOV   R12, [BP+2]        ; rom_ptr

_v32_tmset_copy_loop:
    MOV   R8, R11
    ILT   R8, R10
    JF    R8, _v32_tmset_store

    MOV   R8, R12
    IADD  R8, R11
    MOV   R8, [R8]

    MOV   R6, R7
    IADD  R6, R11
    MOV   [R6], R8

    IADD  R11, 1
    JMP   _v32_tmset_copy_loop

_v32_tmset_store:
    MOV   R6, R7
    IADD  R6, R1
    MOV   [R6], R9

    CIF   R9
    MOV   R0, R9
    JMP   _v32_tmset_done

_v32_tmset_noop:
    MOV   R0, [BP+8]         ; return v unchanged, out-of-bounds writes nothing

_v32_tmset_done:
    MOV   SP, BP
    POP   BP
    RET  

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; __builtin_vircon32_tilemap_render
;;
;; Stack: [BP+2]=rom_ptr  [BP+3]=ram_ptr_addr  [BP+4]=width  [BP+5]=height
;;        [BP+6]=sx  [BP+7]=sy  [BP+8]=w  [BP+9]=h
;;        [BP+10]=x  [BP+11]=y  [BP+12]=tile_w  [BP+13]=tile_h
;;        [BP+14]=skip_id (boxed; BOXED_NIL means "draw every cell")
;;
;; Locals: [BP-1]=clamped sx  [BP-2]=clamped sy  [BP-3]=row  [BP-4]=col
;;
;; Read-only, never promotes. Nothing survives the per-cell CALL to
;; __builtin_vircon32_spr in a register -- loop state lives in the local
;; slots above, everything else is reloaded fresh from its stack argument
;; each time it's needed.
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

    ;@fn __builtin_vircon32_tilemap_render clobbers=R1-R8
__builtin_vircon32_tilemap_render:
    PUSH  BP
    MOV   BP, SP
    ISUB  SP, 4

    ;; ---- Clamp sx to 0 .. max(0, width-w) ----
    MOV   R1, [BP+6]
    CFI   R1
    MOV   R2, [BP+8]
    CFI   R2
    MOV   R3, [BP+4]         ; width
    MOV   R4, R3
    ISUB  R4, R2             ; R4 = width - w
    MOV   R5, R4
    ILT   R5, 0
    JT    R5, _v32_tmrender_sxmax_zero
    JMP   _v32_tmrender_sx_have_max
_v32_tmrender_sxmax_zero:
    MOV   R4, 0
_v32_tmrender_sx_have_max:
    MOV   R5, R1
    ILT   R5, 0
    JT    R5, _v32_tmrender_sx_use_zero
    MOV   R5, R1
    IGT   R5, R4
    JT    R5, _v32_tmrender_sx_use_max
    JMP   _v32_tmrender_sx_done
_v32_tmrender_sx_use_zero:
    MOV   R1, 0
    JMP   _v32_tmrender_sx_done
_v32_tmrender_sx_use_max:
    MOV   R1, R4
_v32_tmrender_sx_done:
    MOV   [BP-1], R1

    ;; ---- Clamp sy to 0 .. max(0, height-h) ----
    MOV   R1, [BP+7]
    CFI   R1
    MOV   R2, [BP+9]
    CFI   R2
    MOV   R3, [BP+5]         ; height
    MOV   R4, R3
    ISUB  R4, R2
    MOV   R5, R4
    ILT   R5, 0
    JT    R5, _v32_tmrender_symax_zero
    JMP   _v32_tmrender_sy_have_max
_v32_tmrender_symax_zero:
    MOV   R4, 0
_v32_tmrender_sy_have_max:
    MOV   R5, R1
    ILT   R5, 0
    JT    R5, _v32_tmrender_sy_use_zero
    MOV   R5, R1
    IGT   R5, R4
    JT    R5, _v32_tmrender_sy_use_max
    JMP   _v32_tmrender_sy_done
_v32_tmrender_sy_use_zero:
    MOV   R1, 0
    JMP   _v32_tmrender_sy_done
_v32_tmrender_sy_use_max:
    MOV   R1, R4
_v32_tmrender_sy_done:
    MOV   [BP-2], R1

    MOV   R1, 0
    MOV   [BP-3], R1         ; row = 0

_v32_tmrender_row_loop:
    MOV   R1, [BP-3]
    MOV   R2, [BP+9]         ; h
    CFI   R2
    MOV   R3, R1
    IGE   R3, R2
    JT    R3, _v32_tmrender_done

    MOV   R1, 0
    MOV   [BP-4], R1         ; col = 0

_v32_tmrender_col_loop:
    MOV   R1, [BP-4]
    MOV   R2, [BP+8]         ; w
    CFI   R2
    MOV   R3, R1
    IGE   R3, R2
    JT    R3, _v32_tmrender_row_next

    ;; map cell = (clamped_sx + col, clamped_sy + row)
    MOV   R1, [BP-1]
    MOV   R2, [BP-4]
    IADD  R1, R2             ; R1 = map cell x

    MOV   R2, [BP-2]
    MOV   R3, [BP-3]
    IADD  R2, R3             ; R2 = map cell y

    ;; index = mapY * width + mapX
    MOV   R3, [BP+4]         ; width
    IMUL  R2, R3
    IADD  R1, R2             ; R1 = index

    ;; resolve data ptr fresh (ROM or promoted RAM copy), read-only
    MOV   R4, [BP+3]         ; ram_ptr_addr
    MOV   R5, [R4]
    MOV   R6, R5
    IEQ   R6, BOXED_NIL
    JT    R6, _v32_tmrender_from_rom

    MOV   R4, R5
    IADD  R4, R1
    MOV   R7, [R4]           ; R7 = tile value
    JMP   _v32_tmrender_have_tile

_v32_tmrender_from_rom:
    MOV   R4, [BP+2]         ; rom_ptr
    IADD  R4, R1
    MOV   R7, [R4]

_v32_tmrender_have_tile:
    MOV   R4, [BP+14]        ; skip_id (boxed)
    MOV   R5, R4
    IEQ   R5, BOXED_NIL
    JT    R5, _v32_tmrender_draw    ; no skip_id given -> always draw

    CFI   R4
    MOV   R5, R7
    IEQ   R5, R4
    JT    R5, _v32_tmrender_col_next  ; tile == skip_id -> skip

_v32_tmrender_draw:
    ;; screen position = (x + col*tile_w, y + row*tile_h)
    MOV   R1, [BP-4]
    MOV   R2, [BP+12]        ; tile_w
    CFI   R2
    IMUL  R1, R2
    MOV   R2, [BP+10]        ; x
    CFI   R2
    IADD  R1, R2
    CIF   R1
    MOV   R8, R1             ; screen x, saved while y is computed

    MOV   R1, [BP-3]
    MOV   R2, [BP+13]        ; tile_h
    CFI   R2
    IMUL  R1, R2
    MOV   R2, [BP+11]        ; y
    CFI   R2
    IADD  R1, R2
    CIF   R1                 ; screen y

    ;; spr(tile_value, screen_x, screen_y, 1.0, 1.0, 0, 0xFFFFFFFF, ALPHA)
    MOV   R2, 32.000000      ; VIRCON32_BLEND_ALPHA
    PUSH  R2
    MOV   R2, 0xFFFFFFFF     ; color_mult: raw packed RGBA word (no CFI)
    PUSH  R2
    MOV   R2, 0.000000
    PUSH  R2
    MOV   R2, 1.000000
    PUSH  R2
    MOV   R2, 1.000000
    PUSH  R2
    PUSH  R1                 ; y
    PUSH  R8                 ; x
    MOV   R2, R7
    CIF   R2
    PUSH  R2                 ; region_id
    CALL  __builtin_vircon32_spr
    IADD  SP, 8

_v32_tmrender_col_next:
    MOV   R1, [BP-4]
    IADD  R1, 1
    MOV   [BP-4], R1
    JMP   _v32_tmrender_col_loop

_v32_tmrender_row_next:
    MOV   R1, [BP-3]
    IADD  R1, 1
    MOV   [BP-3], R1
    JMP   _v32_tmrender_row_loop

_v32_tmrender_done:
    MOV   SP, BP
    POP   BP
    RET  

;; ============================================================================
;; __builtin_tonebank_sfx: tone-bank sfx(n [, channel]) with a DYNAMIC n
;; Shared by PICO-8 sfx() and TIC-80 sfx() (moved here from pico8.s so the
;; TIC-80 layer can use it too).
;; ============================================================================
;; Stack layout relative to BP:
;; [BP+2]: n         (Lua float -- raw PICO-8 sfx index; <0 means "stop")
;; [BP+3]: channel   (Lua float 0-15, or BOXED_NIL -> auto/all)
;; [BP+4]: base_id   (RAW HARDWARE INTEGER, NOT a Lua float -- this is
;;                    pico8_tone_base_id, the resource id the compiler
;;                    assigned placeholder tone 0. Tones 1-7 are
;;                    base_id+1 .. base_id+7, contiguous, since the whole
;;                    bank is registered in one shot -- see
;;                    register_pico8_tone_bank() in pico8.c. Pushed bare,
;;                    never boxed: nothing but this one routine ever reads
;;                    it, same convention as VIRCON32_SFX_CURSOR below.)
;; [BP+5]: mask      (RAW INTEGER: bank size - 1. 7 for the 8 placeholder
;;                    tones; 63 for a .p8 cart's 64 synthesized SFX.)
;;
;; Only reached for a compile-time-UNKNOWN n -- emit_tonebank_sfx_intrinsic()
;; in pico8.c folds every literal n straight into a static
;; emit_vircon32_sfx_play_intrinsic() call, no CALL at all. This routine
;; exists purely so celeste.lua's `psfx` wrapper (sfx(num), where num is a
;; parameter, not a literal) has somewhere to resolve the PICO-8 index ->
;; placeholder-tone mapping at runtime. Its channel handling and channel-
;; ownership bookkeeping mirror __builtin_vircon32_sfx_play exactly (see
;; that routine's own comments for the rationale); only the sound-id
;; resolution step is different.
;;
;; n < 0 is PICO-8's "stop" form (sfx(-1 [, channel])). PICO-8 requires an
;; explicit channel to stop just one; a bare sfx(-1) with no channel here
;; is a deliberate no-op -- the all-sfx-channels form is sfx.stop() itself,
;; reachable directly and not through this dynamic path.
;; ============================================================================
    ;@fn __builtin_tonebank_sfx clobbers=R0
__builtin_tonebank_sfx:
    PUSH  BP
    MOV   BP, SP
    PUSH  R1
    PUSH  R2
    PUSH  R3
    PUSH  R4

    MOV   R1, [BP+2]
    CFI   R1                      ; raw PICO-8 index as a hardware integer

    MOV   R2, R1
    ILT   R2, 0
    JT    R2, _tonebank_sfx_stop_form

    ;; --- Non-negative index: map into the placeholder tone bank ---
    MOV   R2, [BP+5]              ; bank size - 1 (raw integer: 7 or 63)
    AND   R1, R2                  ; wrap into the bank
    MOV   R2, [BP+4]              ; base_id (raw integer, see header note)
    IADD  R1, R2                  ; R1 = resolved placeholder tone id

    ;; --- Resolve the channel (identical to __builtin_vircon32_sfx_play) ---
    MOV   R2, [BP+3]
    MOV   R3, R2
    IEQ   R3, BOXED_NIL
    JT    R3, _tonebank_sfx_auto_channel

    CFI   R2
    MOV   R3, R2
    ILT   R3, 0
    JT    R3, _tonebank_sfx_clamp_low
    MOV   R3, R2
    IGT   R3, 15
    JF    R3, _tonebank_sfx_channel_ready
    MOV   R2, 15
    JMP   _tonebank_sfx_channel_ready

_tonebank_sfx_clamp_low:
    MOV   R2, 0
    JMP   _tonebank_sfx_channel_ready

_tonebank_sfx_auto_channel:
    MOV   R2, [VIRCON32_SFX_CURSOR]
    MOV   R3, R2
    IADD  R3, 1
    MOV   R4, R3
    IGT   R4, 15
    JF    R4, _tonebank_sfx_store_cursor
    MOV   R3, 1

_tonebank_sfx_store_cursor:
    MOV   [VIRCON32_SFX_CURSOR], R3

_tonebank_sfx_channel_ready:
    OUT   SPU_SelectedChannel, R2

    ;; --- Stop, THEN assign (a sound only assigns to a stopped channel) ---
    OUT   SPU_Command, SPUCommand_StopSelectedChannel
    OUT   SPU_ChannelAssignedSound, R1

    ;; --- Track channel ownership: claim for sfx, release from music ---
    MOV   R3, 1
    SHL   R3, R2                        ; R3 = 1 << channel
    MOV   R4, [VIRCON32_SFX_CHANNEL_MASK]
    OR    R4, R3
    MOV   [VIRCON32_SFX_CHANNEL_MASK], R4

    MOV   R4, R3
    NOT   R4                            ; R4 = ~(1 << channel)
    MOV   R3, [VIRCON32_MUSIC_CHANNEL_MASK]
    AND   R3, R4
    MOV   [VIRCON32_MUSIC_CHANNEL_MASK], R3

    ;; --- Volume/speed: no arguments to carry them on this call shape ---
    OUT   SPU_ChannelVolume, 1.0
    OUT   SPU_ChannelSpeed, 1.0

    ;; --- Start, then clear the loop flag the command just set ---
    OUT   SPU_Command, SPUCommand_PlaySelectedChannel
    OUT   SPU_ChannelLoopEnabled, 0

    MOV   R0, R2
    CIF   R0
    JMP   _tonebank_sfx_done

_tonebank_sfx_stop_form:
    MOV   R2, [BP+3]
    MOV   R3, R2
    IEQ   R3, BOXED_NIL
    JT    R3, _tonebank_sfx_stop_done

    CFI   R2
    MOV   R3, R2
    ILT   R3, 0
    JT    R3, _tonebank_sfx_stop_clamp_low
    MOV   R3, R2
    IGT   R3, 15
    JF    R3, _tonebank_sfx_stop_channel_ready
    MOV   R2, 15
    JMP   _tonebank_sfx_stop_channel_ready

_tonebank_sfx_stop_clamp_low:
    MOV   R2, 0

_tonebank_sfx_stop_channel_ready:
    OUT   SPU_SelectedChannel, R2
    OUT   SPU_Command, SPUCommand_StopSelectedChannel

_tonebank_sfx_stop_done:
    MOV   R0, BOXED_NIL

_tonebank_sfx_done:
    POP   R4
    POP   R3
    POP   R2
    POP   R1
    MOV   SP, BP
    POP   BP
    RET  


;; ============================================================================
;; __builtin_vircon32_rect: native rect() / rectfill()
;; ============================================================================
;; Stack layout relative to BP:
;; [BP+2]: x1         [BP+3]: y1        (Lua numbers)
;; [BP+4]: x2         [BP+5]: y2        (Lua numbers)
;; [BP+6]: color      (RAW packed 0xAABBGGRR word, like spr()'s color_mult)
;; [BP+7]: filled     (raw int: 1 rectfill(), 0 rect())
;; Returns BOXED_NIL in R0. Preserves R1-R13.
;;
;; The corners are opposite and INCLUSIVE, in any order; each coordinate is
;; floored (nil / non-numbers count as 0). rectfill() is ONE zoomed draw of
;; the BIOS texture's (-1) region 256 -- one white pixel, (469, 29), hotspot
;; on it, defined by the BIOS -- at scale (w, h): exact for integer sizes,
;; since the region is 1x1, and the GPU's zoom correction keeps sampling
;; inside that pixel. rect() is up to 4 non-overlapping edges -- top and
;; bottom full width, left and right between them -- so a translucent color
;; is not drawn twice at the corners.
;;
;; The GPU state used is put back afterwards (selected texture and region,
;; multiply color, drawing scale), so rect() can sit in the middle of
;; ioports.gpu.* drawing code; the blending mode is left as the program set
;; it.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

    ;@fn __builtin_vircon32_rect clobbers=R0
__builtin_vircon32_rect:
    PUSH  BP
    MOV   BP, SP
    PUSH  R1
    PUSH  R2
    PUSH  R3
    PUSH  R4
    PUSH  R5
    PUSH  R6

    ;; --- GPU state to restore ---
    IN    R0, GPU_SelectedTexture
    PUSH  R0
    IN    R0, GPU_SelectedRegion
    PUSH  R0
    IN    R0, GPU_MultiplyColor
    PUSH  R0
    IN    R0, GPU_DrawingScaleX
    PUSH  R0
    IN    R0, GPU_DrawingScaleY
    PUSH  R0

    ;; --- corners -> R1 = left, R2 = top, R3 = width, R4 = height ---
    MOV   R1, [BP+4]
    CALL  __vircon32_coord
    MOV   R3, R1                  ; x2
    MOV   R1, [BP+5]
    CALL  __vircon32_coord
    MOV   R4, R1                  ; y2
    MOV   R1, [BP+3]
    CALL  __vircon32_coord
    MOV   R2, R1                  ; y1
    MOV   R1, [BP+2]
    CALL  __vircon32_coord        ; x1
    MOV   R0, R1
    IMIN  R1, R3                  ; left
    IMAX  R3, R0                  ; right
    ISUB  R3, R1
    IADD  R3, 1                   ; width  (>= 1)
    MOV   R0, R2
    IMIN  R2, R4                  ; top
    IMAX  R4, R0                  ; bottom
    ISUB  R4, R2
    IADD  R4, 1                   ; height (>= 1)

    OUT   GPU_SelectedTexture, -1    ; BIOS texture
    OUT   GPU_SelectedRegion, 256    ; its white pixel
    MOV   R0, [BP+6]
    OUT   GPU_MultiplyColor, R0   ; raw word, no CFI (see spr())

    MOV   R0, [BP+7]
    JF    R0, _vircon32_rect_outline
    CALL  __vircon32_fill         ; (left, top, w, h)
    JMP   _vircon32_rect_done

_vircon32_rect_outline:
    MOV   R5, R4                  ; h
    MOV   R6, R3                  ; w
    MOV   R4, 1
    CALL  __vircon32_fill         ; top    (x, y, w, 1)
    MOV   R0, R5
    IGT   R0, 1
    JF    R0, _vircon32_rect_done
    PUSH  R2
    IADD  R2, R5
    ISUB  R2, 1
    CALL  __vircon32_fill         ; bottom (x, y + h - 1, w, 1)
    POP   R2
    MOV   R0, R5
    IGT   R0, 2
    JF    R0, _vircon32_rect_done
    IADD  R2, 1
    MOV   R4, R5
    ISUB  R4, 2
    MOV   R3, 1
    CALL  __vircon32_fill         ; left   (x, y + 1, 1, h - 2)
    MOV   R0, R6
    IGT   R0, 1
    JF    R0, _vircon32_rect_done
    IADD  R1, R6
    ISUB  R1, 1
    CALL  __vircon32_fill         ; right  (x + w - 1, y + 1, 1, h - 2)

_vircon32_rect_done:
    POP   R0
    OUT   GPU_DrawingScaleY, R0
    POP   R0
    OUT   GPU_DrawingScaleX, R0
    POP   R0
    OUT   GPU_MultiplyColor, R0
    POP   R0
    OUT   GPU_SelectedRegion, R0
    POP   R0
    OUT   GPU_SelectedTexture, R0
    MOV   R0, BOXED_NIL
    POP   R6
    POP   R5
    POP   R4
    POP   R3
    POP   R2
    POP   R1
    MOV   SP, BP
    POP   BP
    RET  

;; __vircon32_fill (internal): draws the selected 1x1 region zoomed to
;; R3 x R4 pixels at (R1, R2) -- all raw ints. Clobbers R0 only.
    ;@fn __vircon32_fill clobbers=R0
__vircon32_fill:
    MOV   R0, R3
    CIF   R0
    OUT   GPU_DrawingScaleX, R0
    MOV   R0, R4
    CIF   R0
    OUT   GPU_DrawingScaleY, R0
    OUT   GPU_DrawingPointX, R1
    OUT   GPU_DrawingPointY, R2
    OUT   GPU_Command, GPUCommand_DrawRegionZoomed
    RET  

;; __vircon32_coord (internal): R1 = Lua number -> R1 = raw int, floored and
;; clamped to +-65536 (far past the 640x360 screen, and CFI is undefined out
;; of range -- see learnings on x86 vs ARM). nil / non-numbers -> 0.
;; Clobbers R0 only.
    ;@fn __vircon32_coord clobbers=R0-R1
__vircon32_coord:
    MOV   R0, R1
    AND   R0, NAN_VALUE
    IEQ   R0, NAN_VALUE
    JT    R0, _vircon32_coord_zero
    MOV   R0, R1
    FLT   R0, -65536.0
    JF    R0, _vircon32_coord_lo
    MOV   R1, -65536.0
_vircon32_coord_lo:
    MOV   R0, R1
    FGT   R0, 65536.0
    JF    R0, _vircon32_coord_hi
    MOV   R1, 65536.0
_vircon32_coord_hi:
    FLR   R1
    CFI   R1
    RET  
_vircon32_coord_zero:
    MOV   R1, 0
    RET  

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; v32io: gamepad group read
;;
;; Devices that tunnel their own data through a gamepad port (v32kbd,
;; v32mouse) all start the same way: read the 11 controls of THEIR
;; gamepad as one packed word, without disturbing the gamepad the program
;; has selected. This module is only that read; each device's decoder
;; (v32kbd.s, v32mouse.s) takes the word from here.
;;
;; It also owns gamepad SELECTION. The desktop and simplified emulators
;; return garbage when INP_SelectedGamepad is read (ReadPort sets the
;; result, then falls through to the per-gamepad port array and reads
;; PortArray[-1]), so code that must put the program's selection back can't
;; ask the port. Every runtime/compiler write of INP_SelectedGamepad goes
;; through __v32io_select (or also stores V32IO_GAMEPAD), and the selection
;; is read from V32IO_GAMEPAD instead. (A __rawasm__ OUT to the port
;; bypasses this and leaves V32IO_GAMEPAD stale.)
;;
;; Always emitted (btn() of every API selects through it); the group read
;; and connection check are called by v32kbd.s / v32mouse.s, emitted when
;; the program uses the keyboard / the mouse.
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; ============================================================================
;; __v32io_select: select a gamepad, and remember it
;; ============================================================================
;; In: R1 = gamepad (raw int). 0-3: selected, and stored in V32IO_GAMEPAD;
;; anything else is ignored, as the hardware ignores it.
;; Preserves ALL registers.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

    ;@fn __v32io_select clobbers=none
__v32io_select:
    PUSH  R0
    MOV   R0, R1
    ILT   R0, 0
    JT    R0, _v32io_select_done
    MOV   R0, R1
    IGT   R0, 3
    JT    R0, _v32io_select_done
    OUT   INP_SelectedGamepad, R1
    MOV   [V32IO_GAMEPAD], R1
_v32io_select_done:
    POP   R0
    RET  

;; ============================================================================
;; __v32io_read: every control of one gamepad, one bit each
;; ============================================================================
;; In:  R1 = gamepad port (raw int 0-3)
;; Out: R0 = packed controls, 1 = pressed (port value > 0):
;;
;;        bit  0  Left       bit  4  Start      bit  8  Y
;;        bit  1  Right      bit  5  A          bit  9  L
;;        bit  2  Up         bit  6  B          bit 10  R
;;        bit  3  Down       bit  7  X
;;
;;      i.e. bit n = INP port 0x402 + n -- the same packing as the v32kbd
;;      C library's v32kbd_scan() (NOT ioports.inp.inputs, whose order is
;;      reversed for the gamepad API).
;; The program's selected gamepad (V32IO_GAMEPAD) is selected again
;; afterwards. Preserves R1-R13.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

    ;@fn __v32io_read clobbers=R0
__v32io_read:
    PUSH  R2
    PUSH  R3
    MOV   R3, [V32IO_GAMEPAD]
    OUT   INP_SelectedGamepad, R1

    IN    R0, INP_GamepadLeft
    IGT   R0, 0                     ; bit 0
    IN    R2, INP_GamepadRight
    IGT   R2, 0
    SHL   R2, 1
    OR    R0, R2
    IN    R2, INP_GamepadUp
    IGT   R2, 0
    SHL   R2, 2
    OR    R0, R2
    IN    R2, INP_GamepadDown
    IGT   R2, 0
    SHL   R2, 3
    OR    R0, R2
    IN    R2, INP_GamepadButtonStart
    IGT   R2, 0
    SHL   R2, 4
    OR    R0, R2
    IN    R2, INP_GamepadButtonA
    IGT   R2, 0
    SHL   R2, 5
    OR    R0, R2
    IN    R2, INP_GamepadButtonB
    IGT   R2, 0
    SHL   R2, 6
    OR    R0, R2
    IN    R2, INP_GamepadButtonX
    IGT   R2, 0
    SHL   R2, 7
    OR    R0, R2
    IN    R2, INP_GamepadButtonY
    IGT   R2, 0
    SHL   R2, 8
    OR    R0, R2
    IN    R2, INP_GamepadButtonL
    IGT   R2, 0
    SHL   R2, 9
    OR    R0, R2
    IN    R2, INP_GamepadButtonR
    IGT   R2, 0
    SHL   R2, 10
    OR    R0, R2

    OUT   INP_SelectedGamepad, R3
    POP   R3
    POP   R2
    RET  

;; ============================================================================
;; __v32io_connected: is a device plugged into a gamepad port
;; ============================================================================
;; In:  R1 = gamepad port (raw int 0-3)
;; Out: R0 = 1 / 0 (INP_GamepadConnected). Selected gamepad restored.
;; Preserves R1-R13.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

    ;@fn __v32io_connected clobbers=R0
__v32io_connected:
    PUSH  R2
    MOV   R2, [V32IO_GAMEPAD]
    OUT   INP_SelectedGamepad, R1
    IN    R0, INP_GamepadConnected
    OUT   INP_SelectedGamepad, R2
    POP   R2
    RET  

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; SECTION: ROM DATA & STRING CONSTANTS
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

__const_str_nil:
    string "nil"

__const_str_boolean:
    string "boolean"

__const_str_false:
    string "false"

__const_str_true:
    string "true"

__const_str_string:
    string "string"

__const_str_number:
    string "number"

__const_str_table:
    string "table"

__const_str_function:
    string "function"

__const_str_panic_banner:
    string "V32LUA PANIC"

__const_str_err_call_nil:
    string "RUNTIME ERROR: ATTEMPT TO CALL A NON-FUNCTION VALUE"

__const_str_err_not_table:
    string "RUNTIME ERROR: ATTEMPT TO INDEX A NON-TABLE VALUE"

__const_str_err_for_initial:
    string "RUNTIME ERROR: 'FOR' INITIAL VALUE MUST BE A NUMBER"

__const_str_err_for_limit:
    string "RUNTIME ERROR: 'FOR' LIMIT MUST BE A NUMBER"

__const_str_err_for_step:
    string "RUNTIME ERROR: 'FOR' STEP MUST BE A NUMBER"

__const_str_err_oom:
    string "RUNTIME ERROR: OUT OF MEMORY -- HEAP WORDS USED:"

__const_str_err_bad_memcard_title:
    string "RUNTIME ERROR: memcard.title() MUST BE STRING OR INDEXED TABLE"

__const_str_pause:
    string "- PAUSED -"

;; ===========================================================================
;; Math Constants for Lua Library
;; ===========================================================================

;; ===========================================================================
;; Mathematical constants used by various math functions.
;; Each constant is defined as a float value for use in computations.
;; ===========================================================================
__const_math_pi:
    float 3.141592653589793   ; PI constant
__const_math_e:
    float 2.718281828459045   ; Euler's number (e)
__const_math_pi_over_2:
    float 1.570796326794897   ; PI/2 (90 degrees in radians)
__const_math_sqrt2_over_2:
    float 0.707106781186547   ; sqrt(2)/2

;; For math.huge: use maximum representable float value
;; IEEE 754 single precision max: approximately 3.4028235e+38
;; Represented as decimal: 340282356791093500000000000000000000000.0 is too large
;; Using a practical large value that fits in 32-bit float:
__const_math_huge:
    float 3402823466385288.0   ; Maximum finite single-precision float

;; ===========================================================================

;; --- every global starts as nil ---
__globals_to_nil:
    MOV  R0, BOXED_NIL
    MOV  [var_r_p01], R0
    MOV  [var_r_p02a], R0
    MOV  [var_r_p02b], R0
    MOV  [var_r_p03], R0
    MOV  [var_r_p04a], R0
    MOV  [var_r_p04b], R0
    MOV  [var_r_p05], R0
    MOV  [var_r_p06a], R0
    MOV  [var_r_p06b], R0
    MOV  [var_r_p07], R0
    MOV  [var_r_p08], R0
    MOV  [var_r_p09], R0
    MOV  [var_r_p10], R0
    MOV  [var_r_p11a], R0
    MOV  [var_r_p11b], R0
    MOV  [var_r_p12], R0
    MOV  [var_s], R0
    MOV  [var_i], R0
    MOV  [var_z], R0
    RET

;; =========================================================
;; Read-Only String Data Section
;; =========================================================
;; --- Cart Title (Vircon32 memory card formatting, max 20 chars) ---
__cart_title:
    string "directed"


__string_pool_start:
__string_pool_end:
    integer 0
