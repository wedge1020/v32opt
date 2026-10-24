#include "check.h"
int arr[16];
int grid[4][4];
int total;
void clear16(void) { int i; for (i = 0; i < 16; i++) arr[i] = 0; }
int sum4(void) { int i, s = 0; for (i = 0; i < 4; i++) s += arr[i] * (i + 1); return s; }
void fill(void) {
    int i;
    for (i = 0; i < 16; i++) { if (i & 1) arr[i] = i; else arr[i] = -i; }
}
void g4(void) {
    int y, x;
    for (y = 0; y < 4; y++) for (x = 0; x < 4; x++) grid[y][x] = x * 10 + y;
}
int brk(void) { int i, s = 0; for (i = 0; i < 8; i++) { if (i == 5) break; if (i == 2) continue; s += i; } return s; }
int down(void) { int i, s = 0; for (i = 10; i > 0; i -= 3) s = s * 2 + i; return s; }
int liveR(void) { int i; for (i = 0; i < 3; i++) total += i; return i; }
int main(void) {
    int k;
    for (k = 0; k < 16; k++) arr[k] = k + 1;
    CHECK(sum4() == 1*1 + 2*2 + 3*3 + 4*4);
    fill();
    CHECK(arr[3] == 3 && arr[4] == -4 && arr[15] == 15);
    clear16();
    CHECK(arr[7] == 0 && arr[15] == 0);
    g4();
    CHECK(grid[2][3] == 32 && grid[3][1] == 13);
    CHECK(brk() == 0 + 1 + 3 + 4);
    CHECK(down() == (((0*2+10)*2+7)*2+4)*2+1);
    CHECK(liveR() == 3 && total == 3);
    return 0;
}
