-- promote-loops-calls (L4, commit f22c44d): numeric for loops with hot CALLs, one function per case.
--   q08  a table store in the body (the CALL __builtin_table_set has a clobbers= list): promoted
--   q07c goto continue: promoted
--   q04  the body calls a user function (through __builtin_exec, no list): not promoted across the call
--   q05  string concatenation in the body (a callee without a list): not promoted across the call
r_q08a = 0 r_q08b = 0 r_q07 = 0 r_q04 = 0 r_q05 = ""
function q08(t) local s = 0 for i = 1, 7 do t[i] = i s = s + i * 2 end return s end
function q07c(t) for i = 1, 10 do if i % 2 == 0 then goto continue end t[i] = i ::continue:: end return t[9] end
function g(x) return x + 1 end
function q04() local s = 0 for i = 1, 10 do s = s + g(i) end return s end
function q05() local s = "" for i = 1, 5 do s = s .. "ab" end return s end
function main()
  local t = {}
  r_q08a = q08(t) r_q08b = t[7]
  r_q07 = q07c({})
  r_q04 = q04()
  r_q05 = q05()
  system.wait()
end
