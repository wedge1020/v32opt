#include "check.h"
int arr[64];
void clear16(void) { for (int i = 0; i < 16; i++) arr[i] = 0; }
void set4(int v) { for (int i = 0; i < 4; i++) arr[i + 20] = v; }
int main(void) { clear16(); set4(7); CHECK(arr[3] == 0); CHECK(arr[22] == 7); return 0; }
