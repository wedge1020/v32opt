-- liveness-trust, Lua shape: every v32lua function is reached through __builtin_exec ("JMP R0"), so
-- its class is open; with the compiler-ABI rule (-L lua) the registers outside the ABI are free
-- at its RET and promote-loops can keep the loops' locals in registers. Results in r_*.
r_a = 0
r_b = 0
function brute(n)
  local count = 0
  for i = 2, n do
    local is_prime = true
    for j = 2, i - 1 do
      if i % j == 0 then is_prime = false end
    end
    if is_prime then count = count + 1 end
  end
  return count
end
function sumsq(n)
  local s = 0
  for i = 1, n do s = s + i * i end
  return s
end
function main()
  r_a = brute(60)
  r_b = sumsq(30)
  system.wait()
end
