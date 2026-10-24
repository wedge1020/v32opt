#include "check.h"
int a[64];
int b[64];
int total;

int sum_a(void) { int i, s = 0; for (i = 0; i < 64; i++) s += a[i]; return s; }
void clear_a(void) { int i; for (i = 0; i < 64; i++) a[i] = 0; }
void fill_a(void) { int i; for (i = 0; i < 64; i++) a[i] = i * 3 + 1; }          /* i used as a value too */
void copy_ab(void) { int i; for (i = 0; i < 64; i++) b[i] = a[i]; }              /* two bases */
int find(int v) { int i; for (i = 0; i < 64; i++) { if (a[i] == v) return i; } return -1; }
int first_neg(void) { int i, r = 99; for (i = 0; i < 64; i++) { if (a[i] < 0) { r = 1; break; } } return r; }
int after(void) { int i; for (i = 0; i < 40; i++) total += a[i]; return i; }       /* counter live after the loop */
int down(void) { int i, s = 0; for (i = 63; i >= 0; i--) s += a[i] * (2 - (i & 1)); return s; }
int skip2(void) { int i, s = 0; for (i = 0; i < 64; i += 2) s += a[i]; return s; }
int nested(void) { int i, j, s = 0; for (i = 0; i < 20; i++) for (j = 0; j < 20; j++) s += a[j] ^ b[i]; return s; }
int main(void) {
    int k;
    for (k = 0; k < 64; k++) { a[k] = k - 10; b[k] = 0; }
    CHECK(sum_a() == 64*63/2 - 640);
    fill_a();
    CHECK(a[0] == 1 && a[10] == 31 && a[63] == 190);
    copy_ab();
    CHECK(b[7] == 22 && b[63] == 190);
    CHECK(find(46) == 15 && find(5000) == -1);
    CHECK(first_neg() == 99);
    a[30] = -5;
    CHECK(first_neg() == 1);
    CHECK(after() == 40);
    CHECK(down() > 0);
    CHECK(skip2() > 0);
    CHECK(nested() != 0);
    clear_a();
    CHECK(a[0] == 0 && a[63] == 0 && sum_a() == 0);
    return 0;
}
