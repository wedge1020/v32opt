-- 80784a8: a loop in f() that calls g(), a top-level loop (r_* added to the original reproducer)
t = {}
for i = 1, 3 do t[i] = i end
r_n = #t
r_f = 0
function g(x) return x end
function f()
  local s = 0
  for i = 1, 3 do s = g(s) + i end
  return s
end
function main() r_f = f() system.wait() end
