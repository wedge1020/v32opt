#include "check.h"
int g;
int junk(int a) { int x = a * 3 + 7; int y = x * 5 + 11; int z = y * 3 + 2; int w = z * 5 + 1; return (x ^ y ^ z ^ w) & 1; }
int goto_in(int n) {
    int i, s = 0;
    i = 0;
    goto mid;
    for (i = 0; i < n; i++) {
        s += 1;
mid:
        s += 10;
    }
    return s;
}
int main(void) {
    g = junk(5);
    g = goto_in(3);
    CHECK(g == 32);
    return 0;
}
