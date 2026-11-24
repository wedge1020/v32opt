-- promote-loops must not keep a loop variable in a register the loop body POPs into
-- ("PUSH R0 ... POP R2", what peephole-copies makes of a spill): v32opt 20261124-dev and earlier did at -O2.
local junk
function main()
  junk = {}
  for i = 1, 100 do junk[i] = {i, i} end
  number_n = #junk
  number_s = junk[100][2] + junk[37][1]
end
--[[
=== EXPECTED OUTPUT ===

number_n: 100.0000
number_s: 137.0000
]]
