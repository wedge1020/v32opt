-- lua-loop-var directed cases: one function per case, results in r_* globals.
-- p01..p07, p11, p12 carry var= on the ';@loop' line and must be rewritten (14 loops in all);
-- p08 (the variable is captured by a closure), p09 (assigned in the body) and p10 (variadic function)
-- have no var= from the compiler and must be left alone.
r_p01 = 0 r_p02a = 0 r_p02b = 0 r_p03 = 0 r_p04a = 0 r_p04b = 0 r_p05 = 0 r_p06a = 0 r_p06b = 0
r_p07 = 0 r_p08 = 0 r_p09 = 0 r_p10 = 0 r_p11a = 0 r_p11b = 0 r_p12 = 0

-- p01: plain loops, the variable read in the body
function p01_fill(t) for i = 1, 10 do t[i] = i * 2 end end
function p01()
  local t = {} p01_fill(t)
  local s = 0 for k = 1, 10 do s = s + t[k] end
  return s + t[7]
end

-- p02: a nested loop with the same variable name
function p02a() local s = 0 for i = 1, 3 do for i = 1, 4 do s = s + i end s = s + i * 100 end return s end
function p02b(t) for i = 1, 3 do t[i] = i for i = 5, 6 do t[i] = i * 3 end end return t[2] + t[6] end

-- p03: nested loops, different variables
function p03(t) local s = 0 for i = 1, 4 do for j = 1, 5 do t[j] = i * j s = s + t[j] end end return s end

-- p04: break; p05: goto continue; p06: return from inside the loop
function p04(t, n) local s = 0 for i = 1, 20 do t[i] = i if i > n then break end s = s + i end return s end
function p05(t) local s = 0 for i = 1, 12 do if i % 3 == 0 then goto continue end t[i] = i s = s + i ::continue:: end return s end
function p06(t, x) for i = 1, #t do if t[i] == x then return i end end return -1 end

-- p07: a call in the body
function p07_sq(x) return x * x end
function p07(t) local s = 0 for i = 1, 6 do local q = p07_sq(i) t[i] = q + i s = s + i end return s end

-- p08: the variable is captured by a closure (no var=)
function p08() local fs = {} for i = 1, 3 do fs[i] = function() return i * 10 end end return fs[2]() + fs[3]() end

-- p09: the variable is assigned in the body (no var=)
function p09(t) local s = 0 for i = 1, 5 do i = i * 2 t[i] = i s = s + i end return s end

-- p10: a variadic function (no var=)
function p10_n(a) return a end
function p10(...) local s = 0 for i = 1, 4 do s = s + i * p10_n(...) end return s end

-- p11: the variable's slot reused by a later local; a write before every read after the loop
function p11a(t) do for i = 1, 4 do t[i] = i end end local x = t[2] + 40 local y = x * 2 return x + y end
function p11b(t) for i = 1, 3 do t[i] = i end local z local w = 9 if t[1] == 1 then z = w end return z end

-- p12: the inner limit is the outer variable
function p12(t) local s = 0 for i = 1, 5 do for j = 1, i do t[j] = j + i s = s + t[j] end end return s end

function main()
  local t = {}
  r_p01 = p01()
  r_p02a = p02a() r_p02b = p02b({})
  r_p03 = p03(t)
  r_p04a = p04({}, 7) r_p04b = p04({}, 30)
  r_p05 = p05({})
  r_p06a = p06({5, 8, 13, 21, 34}, 21) r_p06b = p06({5, 8, 13, 21, 34}, 4)
  r_p07 = p07({})
  r_p08 = p08()
  r_p09 = p09({})
  r_p10 = p10(3, 4)
  r_p11a = p11a({}) r_p11b = p11b({})
  r_p12 = p12({})
  system.wait()
end
