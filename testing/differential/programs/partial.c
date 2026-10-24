#include "check.h"
int a[128];
int b[128];
int total;
int ns[11];

int sum_n(int n) { int i, s = 0; for (i = 0; i < n; i++) s += a[i]; return s; }
void fill_n(int n) { int i; for (i = 0; i < n; i++) a[i] = i * 3 + 1; }
void copy_n(int n) { int i; for (i = 0; i < n; i++) b[i] = a[i]; }
int step2(int n) { int i, s = 0; for (i = 0; i < n; i += 2) s += a[i]; return s; }
int step3(int n) { int i, s = 0; for (i = 0; i <= n; i += 3) s += a[i]; return s; }
int down(int n) { int i, s = 0; for (i = n; i > 0; i--) s = s * 3 + a[i]; return s; }
int downge(int n) { int i, s = 0; for (i = n; i >= 0; i -= 2) s += a[i]; return s; }
int brk(int n, int v) { int i, s = 0; for (i = 0; i < n; i++) { if (a[i] == v) break; s += a[i]; } return s; }
int cont(int n) { int i, s = 0; for (i = 0; i < n; i++) { if (a[i] & 1) continue; s += a[i]; } return s; }
int find(int n, int v) { int i; for (i = 0; i < n; i++) { if (a[i] == v) return i; } return -1; }
int nested(int n, int m) { int i, j, s = 0; for (i = 0; i < n; i++) for (j = 0; j < m; j++) s += a[j] + b[i]; return s; }
int countto(int n) { int i, c = 0; for (i = 0; i < n; i++) c++; return c; }
int bigbody(int n) {
    int i, s = 0;
    for (i = 0; i < n; i++) {
        s += a[i]; s ^= i; s += s >> 1; s -= a[i] & 3; s += (i * 7) & 15;
        s ^= a[i] << 1; s += b[i]; s -= i; s += 3; s ^= 5; s += i & 1; s += 1; s -= 1;
    }
    return s;
}
int main(void) {
    int k, n;
    for (k = 0; k < 128; k++) { a[k] = k - 7; b[k] = k * 2; }
    ns[0] = 0; ns[1] = 1; ns[2] = 2; ns[3] = 3; ns[4] = 4; ns[5] = 5;
    ns[6] = 7; ns[7] = 8; ns[8] = 9; ns[9] = 33; ns[10] = 100;
    for (k = 0; k < 11; k++) {
        n = ns[k];
        total = total * 31 + sum_n(n);
        total = total * 31 + step2(n);
        total = total * 31 + step3(n);
        total = total * 31 + down(n);
        total = total * 31 + downge(n);
        total = total * 31 + brk(n, 5);
        total = total * 31 + cont(n);
        total = total * 31 + find(n, 6);
        total = total * 31 + countto(n);
        total = total * 31 + bigbody(n);
        total = total * 31 + nested(n, 6);
        fill_n(n);
        copy_n(n);
        total = total * 31 + b[(n + 127) & 127];
    }
    CHECK(total != 0);
    CHECK(countto(37) == 37 && countto(0) == 0 && countto(-5) == 0);
    return 0;
}
