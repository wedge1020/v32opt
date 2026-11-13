-- a top-level numeric for loop + a numeric for loop in main(), -O1 or higher, -L lua: the two
-- loops used the same BP offsets (-2,-3) and peephole-lua-checks took them for one function
-- (reproducer of commit 80784a8, from repro_globalloop.lua; the result globals r_* are added)
t = {}
for i = 1, 3 do t[i] = i end        -- runs in __global_scope_initialization
r_result = #t
r_second = 0
function main()
  local s = 0
  for k = 1, 4 do s = s + k end     -- same BP offsets (-2,-3) as the top-level loop's step/counter
  r_second = s
  system.wait()
end
