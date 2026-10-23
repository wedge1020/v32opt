#include "check.h"
int alias(int n) {
    int v = 0, i;
    int *p = &v;
    for (i = 0; i < n; i++) { v += 1; *p += 10; }
    return v;
}
int main(void) {
    CHECK(alias(5) == 55);
    CHECK(alias(0) == 0);
    return 0;
}
