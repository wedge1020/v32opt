-- lua-returns: ';@fn F returns=N' (v32lua) lets v32opt treat R2/R3 as dead at the RET of a function that
-- returns at most one value. A function that returns two or three values, forwards a call's values or
-- returns `...` must keep them. Results in r_*.
r_a = 0
r_b = 0
r_c = 0
r_d = 0
r_e = 0
function one(n)
  local s = 0
  for i = 1, n do s = s + i end
  return s
end
function two(n)
  local s = 0
  for i = 1, n do s = s + i end
  return s, n
end
function three(n) return n, n + 1, n + 2 end
function fwd(n) return two(n) end
function va(...) return ... end
function none(n) local x = n * 2 end
local ops = { one = one, two = two, three = three, fwd = fwd }
function main()
  local a, b, c
  a, b = two(10)
  r_a = a + b * 1000
  a, b, c = three(1)
  r_b = a + b * 10 + c * 100
  a, b = fwd(4)
  r_c = a + b * 1000
  a, b, c = va(5, 6, 7)
  r_d = a + b * 10 + c * 100
  local f = ops.two
  a, b = f(3)
  local g = ops.one
  local x, y = g(3)
  none(2)
  if y == nil then r_e = a + b * 100 + x * 10000 else r_e = -1 end
  system.wait()
end
