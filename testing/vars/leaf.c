/* v32opt-promote-var.c -- V32DBG: `v32opt -O2`/`-O3` promote-regs moves i and s into registers but leaves their
   `;@var ... loc=BP-n` lines as they were, so a debugger reads stale frame words.

     v32cc -g -o pr.asm v32opt-promote-var.c
     v32opt -O3 -d pr.asm -o pr3.asm        # "[BP-1] lives in R5", "[BP-2] lives in R4"; ;@var i loc=BP-1, s loc=BP-2 unchanged
     v32as -g -o pr3.vbin pr3.asm           # (+ packrom)
     v32sim> break v32opt-promote-var.c:20  then  locals
       measured: i = 4194286 (BP-1), s = 201 (BP-2)  -- wrong; R5 = 10, R4 = 45
       with the two lines rewritten to loc=R5 / loc=R4: i = 10 (R5), s = 45 (R4)  -- right

   v32sim already reads loc=Rn (frame 0) and `optimized-out`. Wanted from v32opt: when a pass moves a variable, rewrite
   its ;@var loc= (or write a new ;@var of the same name at the point it moves, which v32as turns into a new range). */
int g;
int leafsum(int n)
{
    int i;
    int s;
    s = 0;
    for (i = 0; i < n; i++) s += i;
    return s;
}
void main(void) { g = leafsum(10); }
