-- c2abc22: the loop sums t[i]. The original optimizer never terminates on this at -O2 -L lua
-- (promote-* saved and reloaded a slot around a cold CALL and counted the reload as a use of
-- the slot, so every pass found something new to do). The result global r_s is added.
r_s = 0
function f(t)
  local s = 0
  for i = 1, 10 do s = s + t[i] end
  return s
end
function main() r_s = f({1, 2, 3, 4, 5, 6, 7, 8, 9, 10}) system.wait() end
