--
-- primes.lua: a CPU cycle evaluation attempt for v32opt validation
--
-- Calculate primes using trial by division method
-- Counts primes from 2 to N (inclusive)
--

--
-- global variables
--
brute_1024_tally=0
brute_1024_cycles=0
brute_1024_time=0.0
brute_b_1024_tally=0
brute_b_1024_cycles=0
brute_b_1024_time=0.0
brute_o_1024_tally=0
brute_o_1024_cycles=0
brute_o_1024_time=0.0
brute_s_1024_tally=0
brute_s_1024_cycles=0
brute_s_1024_time=0.0

-- Variant 1: Brute force - checks all divisors, no early exit
function count_brute(n)
    local count = 0
	local is_prime = true
    for i = 2, n do
        is_prime = true
        for j = 2, i - 1 do
			-- also likely not working, hence why tally equals upper bound
            if i % j == 0 then
                is_prime = false
            end
        end
		-- if statement condition evaluation still not fully working
		-- neither of these work (although the first works "better" than the other)
        -- if is_prime then
        -- if is_prime == true then
        if is_prime == true then
            count = count + 1
        end
    end
    return count
end

-- Variant 2: Break on composite - exits inner loop on first divisor found
function count_break(n)
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
function count_odds_only(n)
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

-- Variant 4: Square root cap - only checks divisors up to sqrt(i)
function count_sqrt(n)
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

--
-- Main
--
function main()
	ioports.gpu.clear("black")
	print(0,   0,  "Prime Number Computations")
	print(0,   20, "==================================")
	print(0,   40, "variant upper tally cycles time(s)")
	print(0,   60, "------- ----- ----- ------ -------")

	local start            = 0
	local stop             = 0

	print(0,   80, " brute:")
	start                  = ioports.tim.frames
	print(90,  80, "1024")

	n                      = 1024
	brute_1024_tally       = count_brute(n)
	stop                   = ioports.tim.frames
	brute_1024_cycles      = stop - start
	brute_1024_cycles      = brute_1024_cycles * 60
	--brute_1024_cycles      = brute_1024_cycles + ioports.tim.cycles
	print(150, 80, brute_1024_tally)
	print(210, 80, brute_1024_cycles)
	brute_1024_time        = brute_1024_cycles / 250000 / 60
	print(310, 80, brute_1024_time)

	print(0,   100, "+b    :")
	start                  = ioports.tim.frames
	print(90,  100, "1024")
	brute_b_1024_tally       = count_break(n)
	stop                   = ioports.tim.frames
	brute_b_1024_cycles      = stop - start
	brute_b_1024_cycles      = brute_b_1024_cycles * 60
	--brute_1024_cycles      = brute_1024_cycles + ioports.tim.cycles
	print(150, 100, brute_b_1024_tally)
	print(210, 100, brute_b_1024_cycles)
	brute_b_1024_time        = brute_b_1024_cycles / 250000 / 60
	print(310, 100, brute_b_1024_time)

	print(0,   120, "  +o  :")
	start                  = ioports.tim.frames
	print(90,  120, "1024")
	brute_o_1024_tally       = count_odds_only(n)
	stop                   = ioports.tim.frames
	brute_o_1024_cycles      = stop - start
	brute_o_1024_cycles      = brute_o_1024_cycles * 60
	--brute_1024_cycles      = brute_1024_cycles + ioports.tim.cycles
	print(150, 120, brute_o_1024_tally)
	print(210, 120, brute_o_1024_cycles)
	brute_o_1024_time        = brute_o_1024_cycles / 250000 / 60
	print(310, 120, brute_o_1024_time)

--[[
	start                  = ioports.tim.frames
	brute_b_1024_tally     = count_break(n)
	stop                   = ioports.tim.frames
	brute_b_1024_cycles    = stop - start
	brute_b_1024_cycles    = brute_1024_cycles * 60
	brute_b_1024_cycles    = brute_1024_cycles + ioports.tim.cycles

	brute_o_1024_tally  = count_odds_only(n)
	brute_s_1024_tally  = count_sqrt(n)

--]]
	-- system.wait()
	ioports.gpu.sync()
end
