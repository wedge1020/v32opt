/* v32opt hint test: per-function inline limits (see run_requests.sh). Compiled with
 *   v32cc -O1 --freestanding --standard -e main requests_inline.c    (limit: -finline-max=8)  */
#define BODY(a, b) (((a) * 3 + (b)) ^ (a)) + (((b) << 2) - ((a) & 7)) * 5 + (a)          /* 15 instructions */
#define SMALL(a, b) ((a) * 3 + (b)) + ((b) << 2) + (a)                                    /* 9 instructions */
static int plain(int a, int b)  { return BODY(a, b); }
__attribute__((always_inline)) static int always(int a, int b) { return BODY(a, b); }
static inline int kw(int a, int b)  { return SMALL(a, b); }
static int kw_not(int a, int b)  { return SMALL(a, b); }
__attribute__((hot)) static int hot(int a, int b) { return SMALL(a, b); }
__attribute__((cold, always_inline)) static int cold_always(int a) { return a + 2; }
__attribute__((cold)) static int cold_tiny(int a) { return a + 2; }
__attribute__((noinline, always_inline)) static int never(int a) { return a + 3; }
int main(void)
{
    int n = 1;
    return plain(n, 2) + always(n, 2) + kw(n, 2) + kw_not(n, 2) + hot(n, 2) + cold_always(n) + cold_tiny(n) + never(n);
}
