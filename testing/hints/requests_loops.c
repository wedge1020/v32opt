/* v32opt hint test: loop hints (see run_requests.sh).  Every loop's body is one xor, so the number of xor
 * instructions in a function is the number of copies of the body.  Compiled with
 *   v32cc -O1 --freestanding --standard -e main requests_loops.c                                                       */
int g[128];
int plain4(void)  { int s = 0; for (int i = 0; i < 4; i++) s ^= g[i]; return s; }
int plain20(void) { int s = 0; for (int i = 0; i < 20; i++) s ^= g[i]; return s; }
int plain40(void) { int s = 0; for (int i = 0; i < 40; i++) s ^= g[i]; return s; }
int plain_n(int n) { int s = 0; for (int i = 0; i < n; i++) s ^= g[i]; return s; }

int nounroll4(void) { int s = 0;
#pragma nounroll
    for (int i = 0; i < 4; i++) s ^= g[i];
    return s; }
int unroll40(void) { int s = 0;
#pragma unroll 40
    for (int i = 0; i < 40; i++) s ^= g[i];
    return s; }
int unroll_full30(void) { int s = 0;
#pragma unroll
    for (int i = 0; i < 30; i++) s ^= g[i];
    return s; }
int unroll_part(int n) { int s = 0;
#pragma unroll 6
    for (int i = 0; i < n; i++) s ^= g[i];
    return s; }
int nounroll_n(int n) { int s = 0;
#pragma nounroll
    for (int i = 0; i < n; i++) s ^= g[i];
    return s; }

int hot_loop20(void) { int s = 0;
#pragma v32cc loop hot
    for (int i = 0; i < 20; i++) s ^= g[i];
    return s; }
int cold_loop4(void) { int s = 0;
#pragma v32cc loop cold
    for (int i = 0; i < 4; i++) s ^= g[i];
    return s; }
int cold_unroll4(void) { int s = 0;
#pragma v32cc loop cold unroll 4
    for (int i = 0; i < 4; i++) s ^= g[i];
    return s; }
__attribute__((hot)) int hot_fn20(void) { int s = 0; for (int i = 0; i < 20; i++) s ^= g[i]; return s; }
__attribute__((cold)) int cold_fn4(void) { int s = 0; for (int i = 0; i < 4; i++) s ^= g[i]; return s; }
__attribute__((cold)) int cold_fn_n(int n) { int s = 0; for (int i = 0; i < n; i++) s ^= g[i]; return s; }
int main(void)
{
    return plain4() + plain20() + plain40() + plain_n(3) + nounroll4() + unroll40() + unroll_full30() + unroll_part(5) +
           nounroll_n(5) + hot_loop20() + cold_loop4() + cold_unroll4() + hot_fn20() + cold_fn4() + cold_fn_n(3);
}
