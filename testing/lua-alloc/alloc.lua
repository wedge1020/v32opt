function main()
local t = {}
for i = 1, 100 do t[i] = "x" .. i end
g = t
local m = arena.mark()
local u = {}
for i = 1, 50 do u[#u+1] = {i, tostring(i)} end
arena.release(m)
print(0,0,#t)
end
