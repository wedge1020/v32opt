#include "check.h"
int arr[64];
int grid[16][16];
int sink;

int sum_arr(int n) {
    int s = 0, i;
    for (i = 0; i < n; i++) s += arr[i];
    return s;
}
void fill(int n) {
    int i;
    for (i = 0; i < n; i++) arr[i] = i * 3 + 1;
}
int nested(int w, int h) {
    int x, y, t = 0;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            grid[y][x] = x ^ y;
            t += grid[y][x];
        }
    return t;
}
int collatz(int n) {
    int steps = 0;
    while (n != 1) {
        if (n % 2 == 0) n = n / 2; else n = 3 * n + 1;
        steps++;
    }
    return steps;
}
int brk(int n) {
    int i = 0, s = 0;
    while (1) {
        if (i >= n) break;
        i++;
        if (i % 3 == 0) continue;
        s += i;
    }
    return s;
}
int dowhile(int n) {
    int i = 0, s = 0;
    do { s += i * i; i++; } while (i < n);
    return s;
}
int early(int n) {
    int i, s = 0;
    for (i = 0; i < n; i++) {
        s += i;
        if (s > 100) return s;
    }
    return -s;
}
int callin(int n) {
    int i, s = 0;
    for (i = 0; i < n; i++) s += sum_arr(i);
    return s;
}
void sort(int n) {
    int i, j, t;
    for (i = 0; i < n; i++)
        for (j = 0; j + 1 < n - i; j++)
            if (arr[j] < arr[j+1]) { t = arr[j]; arr[j] = arr[j+1]; arr[j+1] = t; }
}
int ptrloc(int n) {
    int v = 0, i;
    int *p = &v;
    for (i = 0; i < n; i++) *p += i;
    return v;
}
int main(void) {
    fill(40);
    CHECK(sum_arr(40) == 2380);
    CHECK(nested(16, 16) == 1920);
    CHECK(collatz(27) == 111);
    CHECK(brk(30) == 300);
    CHECK(dowhile(10) == 285);
    CHECK(early(50) == 105);
    CHECK(early(5) == -10);
    CHECK(callin(20) == 3610);
    sort(40);
    CHECK(arr[0] == 118 && arr[39] == 1);
    CHECK(ptrloc(10) == 45);
    return 0;
}
