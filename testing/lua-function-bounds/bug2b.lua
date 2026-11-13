-- c2abc22: the same non-convergence where the loop only stores t[i] in a local that is returned
-- afterwards (-O2 -L lua). The result global r_x is added.
r_x = 0
function f(t)
  local x = 0
  for i = 1, 10 do x = t[i] end
  return x
end
function main() r_x = f({10, 20, 30, 40, 50, 60, 70, 80, 90, 100}) system.wait() end
