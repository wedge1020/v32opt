#include "check.h"
/* The inner loop is entered again and again by a loop that has a CALL (so the
 * inner loop is the one promoted). The accumulator s is read nowhere outside
 * it, but its preheader load re-reads it from memory on every entry: the
 * store at the inner loop's exit must stay. */
int g, h;
int ping(void) { return h++; }
int f(int n) {
    int s = 0, i, j;
    for (j = 0; j < n; j++) {
        ping();
        for (i = 0; i < 4; i++) { s += i + j; g = s; }
    }
    return 0;
}
int main(void) {
    f(5);
    CHECK(g == 6 * 5 + 4 * (0 + 1 + 2 + 3 + 4));
    return 0;
}
