-- 80784a8: a top-level loop, and a main() without loops (r_* added to the original reproducer)
t = {}
for i = 1, 3 do t[i] = i end
r_n = #t
r_sum = t[1] + t[2] + t[3]
function main() system.wait() end
