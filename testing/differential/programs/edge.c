#include "check.h"
int g[8];
struct S { int a; int b; int c; };

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
int after_loop(int n) {
    int i;
    for (i = 0; i < n; i++) ;
    return i;
}
int structloop(int n) {
    struct S s;
    struct S *p = &s;
    int i;
    s.a = 1; s.b = 2; s.c = 3;
    for (i = 0; i < n; i++) { s.a += 1; p->b += 2; s.c += p->a; }
    return s.a * 100 + s.b * 10 + s.c;
}
int cont_while(int n) {
    int i = 0, s = 0;
    while (i < n) {
        i++;
        if (i & 1) continue;
        s += i;
    }
    return s * 1000 + i;
}
int nested_call(int n) {
    int i, j, t = 0, k;
    for (i = 0; i < n; i++) {
        t += i;
        for (j = 0; j < 4; j++) { t += j; k = t; t = k + 1; }
        if (i == 2) t += after_loop(3);
    }
    return t;
}
int dw(int n) {
    int i = 0, s = 0;
    do { s += i; i += 2; if (s > 50) break; } while (i < n);
    return s * 100 + i;
}
int brk_nested(int n) {
    int i, j, c = 0;
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            if (j > i) break;
            c++;
        }
        if (c > 20) break;
    }
    return c * 100 + i;
}
int param_loop(int a, int b) {
    int i, s = 0;
    for (i = 0; i < a; i++) { s += b; b ^= i; }
    return s;
}
int main(void) {
    CHECK(1);
    g[0] = goto_in(5); g[1] = structloop(4); g[2] = nested_call(5); g[3] = dw(40); g[4] = brk_nested(9); g[5] = param_loop(6, 5); g[6] = after_loop(7); g[7] = cont_while(10);
    return 0;
}
