#include "check.h"
int arr[64];
int brr[64];
int sts[16][3];
int g1(int x) { int y = x; for (int k = 0; k < 3; k++) y += k * x; return y - 3 * x + 1 - x * 0; }

/* ---- forms that must fire ---- */
int f_unit(int n) { int s = 0; for (int i = 0; i < n; i++) s += arr[i]; return s; }
int f_value(int n) { int s = 0; for (int i = 0; i < n; i++) s += arr[i] * i; return s; }
int f_scaled(int n) { int s = 0; for (int i = 0; i < n; i++) s += sts[i][1]; return s; }
int f_frame(int n) { int a[16]; for (int i = 0; i < 16; i++) a[i] = i * 2; int s = 0; for (int i = 0; i < n; i++) s += a[i]; return s; }
void f_two(int n) { for (int i = 0; i < n; i++) brr[i] = arr[i] + 1; }
int f_down(int n) { int s = 0; for (int i = n - 1; i >= 0; i--) s += arr[i] * i; return s; }
int f_step2(int n) { int s = 0; for (int i = 0; i < n; i += 2) s += arr[i]; return s; }
int f_offset(int n) { int s = 0; for (int i = 0; i < n; i++) s += arr[i + 3]; return s; }

/* ---- refusals: must not be rewritten ---- */
int r1_shape(int n) { int s = 0; int i = 0; while (i < n) { i++; s += arr[i]; } return s; }
int r2_call(int n) { int s = 0; for (int i = 0; i < n; i++) s += arr[i] + g1(i); return s; }
int r3_counter(int n) { int s = 0; for (int i = 0; i < n; i++) { s += arr[i]; if (arr[i] == 5) i++; } return s; }
int r4_goto(int n) { int s = 0; int i = 1; if (n > 3) goto inside; for (i = 0; i < n; i++) { inside: s += arr[i]; } return s; }
int r8_base(int n) { int *p = arr; int s = 0; for (int i = 0; i < n; i++) { s += p[i]; if (i == 2) p = brr; } return s; }

int main(void) {
  for (int i = 0; i < 64; i++) { arr[i] = i; brr[i] = 100 + i; }
  for (int i = 0; i < 16; i++) { sts[i][0] = 1; sts[i][1] = i * 10; sts[i][2] = 2; }
  CHECK(f_unit(10) == 45);
  CHECK(f_value(4) == 14);
  CHECK(f_scaled(4) == 60);
  CHECK(f_frame(5) == 20);
  f_two(8);
  CHECK(brr[7] == 8);
  CHECK(brr[8] == 108);
  CHECK(f_down(5) == 30);
  CHECK(f_step2(7) == 12);
  CHECK(f_offset(3) == 12);
  CHECK(r1_shape(3) == 6);
  CHECK(r2_call(3) == 6 + 3);
  CHECK(r3_counter(8) == 22);
  CHECK(r4_goto(5) == 10);
  CHECK(r4_goto(2) == 1);
  CHECK(r8_base(5) == 12);
  return 0;
}
