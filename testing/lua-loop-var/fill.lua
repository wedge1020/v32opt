-- one numeric for loop with the variable read in the body: the base of the hand-edited negatives
-- of run.sh (fill.asm is v32lua's output with var= on the ';@loop' line, fill_old.asm the output of
-- a v32lua without it)
r_t = 0
function fill(t)
  for i = 1, 10 do t[i] = i * 2 end
end
function main() local t = {} fill(t) r_t = t[3] + t[10] system.wait() end
