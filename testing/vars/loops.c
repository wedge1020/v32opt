/* loops promoted on their own (promote-loops): the counter and the sum live in registers inside the loop */
int g;
__attribute__((noinline)) int f(int x) { return x + 1; }
int lp(int n)
{
    int i;
    int s;
    s = 0;
    for (i = 0; i < n; i++) { s += i; s += i * 3; }
    g = f(s);
    for (i = 0; i < n; i++) { int t = i * 2; s += t; s -= i; }
    return s + g;
}
void main(void) { g = lp(10); }
