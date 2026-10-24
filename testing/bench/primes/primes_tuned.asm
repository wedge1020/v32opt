; primes (assembly): the eight trial-division variants of primes.c, written
; by hand. The sum of the eight counts is stored in `result`
; (8 * pi(N); 776 for N = 512). Loops tested at the bottom (one jump per iteration) behind a guard.
;
; Convention: r1 = n, the count comes back in r0; the variants clobber r2-r7.

%define global_result 0

    call _main
    hlt

_main:
    mov r1, 512
    call _brute
    mov r8, r0
    mov r1, 512
    call _brute_b
    iadd r8, r0
    mov r1, 512
    call _brute_o
    iadd r8, r0
    mov r1, 512
    call _brute_s
    iadd r8, r0
    mov r1, 512
    call _brute_bs
    iadd r8, r0
    mov r1, 512
    call _brute_bo
    iadd r8, r0
    mov r1, 512
    call _brute_os
    iadd r8, r0
    mov r1, 512
    call _brute_bos
    iadd r8, r0
    mov [global_result], r8
    ret

_brute:                          ; r1 = n, returns the count in r0
    mov r2, 0                 ; count
    mov r3, 2                 ; i
_brute_i:
    mov r0, r3
    igt r0, r1
    jt r0, _brute_done
    mov r4, 1                 ; is_prime
    mov r5, 2                 ; j
    mov r0, r5
    ige r0, r3
    jt r0, _brute_jend
_brute_j:
    mov r0, r3
    imod r0, r5
    jt r0, _brute_next
    mov r4, 0
_brute_next:
    iadd r5, 1
    mov r0, r5
    ilt r0, r3
    jt r0, _brute_j
_brute_jend:
    iadd r2, r4
    iadd r3, 1
    jmp _brute_i
_brute_done:
    mov r0, r2
    ret

_brute_b:                          ; r1 = n, returns the count in r0
    mov r2, 0                 ; count
    mov r3, 2                 ; i
_brute_b_i:
    mov r0, r3
    igt r0, r1
    jt r0, _brute_b_done
    mov r4, 1                 ; is_prime
    mov r5, 2                 ; j
    mov r0, r5
    ige r0, r3
    jt r0, _brute_b_jend
_brute_b_j:
    mov r0, r3
    imod r0, r5
    jt r0, _brute_b_next
    mov r4, 0
    jmp _brute_b_jend
_brute_b_next:
    iadd r5, 1
    mov r0, r5
    ilt r0, r3
    jt r0, _brute_b_j
_brute_b_jend:
    iadd r2, r4
    iadd r3, 1
    jmp _brute_b_i
_brute_b_done:
    mov r0, r2
    ret

_brute_o:                          ; r1 = n, returns the count in r0
    mov r2, 1                 ; count
    mov r3, 3                 ; i
_brute_o_i:
    mov r0, r3
    igt r0, r1
    jt r0, _brute_o_done
    mov r4, 1                 ; is_prime
    mov r5, 3                 ; j
    mov r0, r5
    ige r0, r3
    jt r0, _brute_o_jend
_brute_o_j:
    mov r0, r3
    imod r0, r5
    jt r0, _brute_o_next
    mov r4, 0
_brute_o_next:
    iadd r5, 2
    mov r0, r5
    ilt r0, r3
    jt r0, _brute_o_j
_brute_o_jend:
    iadd r2, r4
    iadd r3, 2
    jmp _brute_o_i
_brute_o_done:
    mov r0, r2
    ret

_brute_s:                          ; r1 = n, returns the count in r0
    mov r2, 0                 ; count
    mov r3, 2                 ; i
    mov r6, 1                 ; limit = floor(sqrt(i)), kept incrementally
_brute_s_i:
    mov r0, r3
    igt r0, r1
    jt r0, _brute_s_done
    mov r4, 1                 ; is_prime
_brute_s_sq:
    mov r7, r6
    iadd r7, 1
    imul r7, r7
    igt r7, r3                ; (limit+1)^2 > i ?
    jt r7, _brute_s_sqdone
    iadd r6, 1
    jmp _brute_s_sq
_brute_s_sqdone:
    mov r5, 2                 ; j
    mov r0, r5
    igt r0, r6
    jt r0, _brute_s_jend
_brute_s_j:
    mov r0, r3
    imod r0, r5
    jt r0, _brute_s_next
    mov r4, 0
_brute_s_next:
    iadd r5, 1
    mov r0, r5
    ile r0, r6
    jt r0, _brute_s_j
_brute_s_jend:
    iadd r2, r4
    iadd r3, 1
    jmp _brute_s_i
_brute_s_done:
    mov r0, r2
    ret

_brute_bs:                          ; r1 = n, returns the count in r0
    mov r2, 0                 ; count
    mov r3, 2                 ; i
    mov r6, 1                 ; limit = floor(sqrt(i)), kept incrementally
_brute_bs_i:
    mov r0, r3
    igt r0, r1
    jt r0, _brute_bs_done
    mov r4, 1                 ; is_prime
_brute_bs_sq:
    mov r7, r6
    iadd r7, 1
    imul r7, r7
    igt r7, r3                ; (limit+1)^2 > i ?
    jt r7, _brute_bs_sqdone
    iadd r6, 1
    jmp _brute_bs_sq
_brute_bs_sqdone:
    mov r5, 2                 ; j
    mov r0, r5
    igt r0, r6
    jt r0, _brute_bs_jend
_brute_bs_j:
    mov r0, r3
    imod r0, r5
    jt r0, _brute_bs_next
    mov r4, 0
    jmp _brute_bs_jend
_brute_bs_next:
    iadd r5, 1
    mov r0, r5
    ile r0, r6
    jt r0, _brute_bs_j
_brute_bs_jend:
    iadd r2, r4
    iadd r3, 1
    jmp _brute_bs_i
_brute_bs_done:
    mov r0, r2
    ret

_brute_bo:                          ; r1 = n, returns the count in r0
    mov r2, 1                 ; count
    mov r3, 3                 ; i
_brute_bo_i:
    mov r0, r3
    igt r0, r1
    jt r0, _brute_bo_done
    mov r4, 1                 ; is_prime
    mov r5, 3                 ; j
    mov r0, r5
    ige r0, r3
    jt r0, _brute_bo_jend
_brute_bo_j:
    mov r0, r3
    imod r0, r5
    jt r0, _brute_bo_next
    mov r4, 0
    jmp _brute_bo_jend
_brute_bo_next:
    iadd r5, 2
    mov r0, r5
    ilt r0, r3
    jt r0, _brute_bo_j
_brute_bo_jend:
    iadd r2, r4
    iadd r3, 2
    jmp _brute_bo_i
_brute_bo_done:
    mov r0, r2
    ret

_brute_os:                          ; r1 = n, returns the count in r0
    mov r2, 1                 ; count
    mov r3, 3                 ; i
    mov r6, 1                 ; limit = floor(sqrt(i)), kept incrementally
_brute_os_i:
    mov r0, r3
    igt r0, r1
    jt r0, _brute_os_done
    mov r4, 1                 ; is_prime
_brute_os_sq:
    mov r7, r6
    iadd r7, 1
    imul r7, r7
    igt r7, r3                ; (limit+1)^2 > i ?
    jt r7, _brute_os_sqdone
    iadd r6, 1
    jmp _brute_os_sq
_brute_os_sqdone:
    mov r5, 3                 ; j
    mov r0, r5
    igt r0, r6
    jt r0, _brute_os_jend
_brute_os_j:
    mov r0, r3
    imod r0, r5
    jt r0, _brute_os_next
    mov r4, 0
_brute_os_next:
    iadd r5, 2
    mov r0, r5
    ile r0, r6
    jt r0, _brute_os_j
_brute_os_jend:
    iadd r2, r4
    iadd r3, 2
    jmp _brute_os_i
_brute_os_done:
    mov r0, r2
    ret

_brute_bos:                          ; r1 = n, returns the count in r0
    mov r2, 1                 ; count
    mov r3, 3                 ; i
    mov r6, 1                 ; limit = floor(sqrt(i)), kept incrementally
_brute_bos_i:
    mov r0, r3
    igt r0, r1
    jt r0, _brute_bos_done
    mov r4, 1                 ; is_prime
_brute_bos_sq:
    mov r7, r6
    iadd r7, 1
    imul r7, r7
    igt r7, r3                ; (limit+1)^2 > i ?
    jt r7, _brute_bos_sqdone
    iadd r6, 1
    jmp _brute_bos_sq
_brute_bos_sqdone:
    mov r5, 3                 ; j
    mov r0, r5
    igt r0, r6
    jt r0, _brute_bos_jend
_brute_bos_j:
    mov r0, r3
    imod r0, r5
    jt r0, _brute_bos_next
    mov r4, 0
    jmp _brute_bos_jend
_brute_bos_next:
    iadd r5, 2
    mov r0, r5
    ile r0, r6
    jt r0, _brute_bos_j
_brute_bos_jend:
    iadd r2, r4
    iadd r3, 2
    jmp _brute_bos_i
_brute_bos_done:
    mov r0, r2
    ret
