-- primes (Lua): the eight trial-division variants of primes.c, taken from
-- the v32lua primes demo without its display code.  The sum of the eight
-- counts goes to the global r_result (8 * pi(N); 776 for N = 512).

r_result = 0

-- Variant 1: Brute force - checks all divisors, no early exit
function brute(n)
    local count = 0
    for i = 2, n do
        local is_prime = true  -- Reset for each number
        for j = 2, i - 1 do
            if i % j == 0 then
                is_prime = false
            end
        end
        if is_prime then
            count = count + 1
        end
    end
    return count
end

-- Variant 2: Break on composite - exits inner loop on first divisor found
function brute_b(n)
    local count = 0
    for i = 2, n do
        local is_prime = true
        for j = 2, i - 1 do
            if i % j == 0 then
                is_prime = false
                break
            end
        end
        if is_prime then
            count = count + 1
        end
    end
    return count
end

-- Variant 3: Odds-only - assumes 2 is prime, checks only odd numbers in both loops
function brute_o(n)
    if n < 2 then return 0 end
    local count = 1  -- 2 is prime
    for i = 3, n, 2 do
        local is_prime = true
        for j = 3, i - 1, 2 do
            if i % j == 0 then
                is_prime = false
            end
        end
        if is_prime then
            count = count + 1
        end
    end
    return count
end

-- Variant: Square root cap - only checks divisors up to sqrt(i)
function brute_s(n)
    local count = 0
    for i = 2, n do
        local is_prime = true
        local limit = math.floor(math.sqrt(i))
        for j = 2, limit do
            if i % j == 0 then
                is_prime = false
            end
        end
        if is_prime then
            count = count + 1
        end
    end
    return count
end

-- Variant: break + odds - assumes 2 is prime, checks only odd numbers in both loops
function brute_bo(n)
    if n < 2 then return 0 end
    local count = 1  -- 2 is prime
    for i = 3, n, 2 do
        local is_prime = true
        for j = 3, i - 1, 2 do
            if i % j == 0 then
                is_prime = false
                break
            end
        end
        if is_prime then
            count = count + 1
        end
    end
    return count
end

-- Variant: break on composite + Square root cap - only checks divisors up to sqrt(i)
function brute_bs(n)
    local count = 0
    for i = 2, n do
        local is_prime = true
        local limit = math.floor(math.sqrt(i))
        for j = 2, limit do
            if i % j == 0 then
                is_prime = false
                break
            end
        end
        if is_prime then
            count = count + 1
        end
    end
    return count
end

-- Variant: odds + square root - assumes 2 is prime, checks only odd numbers in both loops, applies square root trick
function brute_os(n)
    if n < 2 then return 0 end
    local count = 1  -- 2 is prime
    for i = 3, n, 2 do
        local is_prime = true
        local limit = math.floor(math.sqrt(i))
        for j = 3, limit, 2 do
            if i % j == 0 then
                is_prime = false
            end
        end
        if is_prime then
            count = count + 1
        end
    end
    return count
end

-- Variant: brute + break + odds + sqrt
function brute_bos(n)
    if n < 2 then return 0 end
    local count = 1  -- 2 is prime
    for i = 3, n, 2 do
        local is_prime = true
        local limit     = math.floor(math.sqrt(i))
        for j = 3, limit, 2 do
            if i % j == 0 then
                is_prime = false
                break
            end
        end
        if is_prime then
            count = count + 1
        end
    end
    return count
end


function main()
    local N = 512
    r_result = brute(N) + brute_b(N) + brute_o(N) + brute_s(N)
             + brute_bs(N) + brute_bo(N) + brute_os(N) + brute_bos(N)
end
