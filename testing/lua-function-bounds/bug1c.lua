-- 80784a8: a top-level loop, a loop in main() and another in f() (r_* added to the original reproducer)
t = {}
for i = 1, 3 do t[i] = i end
r_n = #t
r_main = 0
r_f = 0
function main()
  local s = 0
  for k = 1, 4 do s = s + k end
  r_main = s
  r_f = f({1, 2, 3, 4, 5, 6, 7, 8, 9, 10})
  system.wait()
end
function f(t)
  local s = 0
  for i = 1, 10 do s = s + t[i] end
  return s
end
