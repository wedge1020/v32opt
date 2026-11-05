/* a CALL in the middle of a function: the variable a is promoted on each side of it on its own (promote-regs) */
int g;
__attribute__((noinline)) int f(int x) { return x + 1; }
int seg(int n)
{
    int a = n;
    int b;
    a = a + 1; a = a * 2; a = a + 3;
    b = f(a);
    a = b + 1; a = a + 2; a = a + 3;
    return a + b;
}
void main(void) { g = seg(4); }
