/* primes: count the primes up to N by trial division, with the eight
 * variants of the v32lua primes demo:
 *     brute, +b (break), +o (odds only), +s (stop at sqrt) and the
 *     combinations of them.
 * The program stores the sum of the eight counts in `result`
 * (8 * pi(N); 776 for N = 512) and halts.  No I/O: it is a benchmark.
 *
 * Plain C in the subset both v32cc and the stock Vircon32 compiler accept
 * (bench.py rewrites the declarations for the latter).
 */
#define N 512

int result;

int brute(int n)
{
    int count = 0, i, j, ok;
    for (i = 2; i <= n; i++) {
        ok = 1;
        for (j = 2; j < i; j++) {
            if (i % j == 0) ok = 0;
        }
        count += ok;
    }
    return count;
}

int brute_b(int n)
{
    int count = 0, i, j, ok;
    for (i = 2; i <= n; i++) {
        ok = 1;
        for (j = 2; j < i; j++) {
            if (i % j == 0) { ok = 0; break; }
        }
        count += ok;
    }
    return count;
}

int brute_o(int n)
{
    int count = 1, i, j, ok;
    for (i = 3; i <= n; i += 2) {
        ok = 1;
        for (j = 3; j < i; j += 2) {
            if (i % j == 0) ok = 0;
        }
        count += ok;
    }
    return count;
}

int brute_s(int n)
{
    int count = 0, i, j, ok, limit = 1;
    for (i = 2; i <= n; i++) {
        ok = 1;
        while ((limit + 1) * (limit + 1) <= i) limit++;
        for (j = 2; j <= limit; j++) {
            if (i % j == 0) ok = 0;
        }
        count += ok;
    }
    return count;
}

int brute_bs(int n)
{
    int count = 0, i, j, ok, limit = 1;
    for (i = 2; i <= n; i++) {
        ok = 1;
        while ((limit + 1) * (limit + 1) <= i) limit++;
        for (j = 2; j <= limit; j++) {
            if (i % j == 0) { ok = 0; break; }
        }
        count += ok;
    }
    return count;
}

int brute_bo(int n)
{
    int count = 1, i, j, ok;
    for (i = 3; i <= n; i += 2) {
        ok = 1;
        for (j = 3; j < i; j += 2) {
            if (i % j == 0) { ok = 0; break; }
        }
        count += ok;
    }
    return count;
}

int brute_os(int n)
{
    int count = 1, i, j, ok, limit = 1;
    for (i = 3; i <= n; i += 2) {
        ok = 1;
        while ((limit + 1) * (limit + 1) <= i) limit++;
        for (j = 3; j <= limit; j += 2) {
            if (i % j == 0) ok = 0;
        }
        count += ok;
    }
    return count;
}

int brute_bos(int n)
{
    int count = 1, i, j, ok, limit = 1;
    for (i = 3; i <= n; i += 2) {
        ok = 1;
        while ((limit + 1) * (limit + 1) <= i) limit++;
        for (j = 3; j <= limit; j += 2) {
            if (i % j == 0) { ok = 0; break; }
        }
        count += ok;
    }
    return count;
}

int main(void)
{
    result = brute(N) + brute_b(N) + brute_o(N) + brute_s(N)
           + brute_bs(N) + brute_bo(N) + brute_os(N) + brute_bos(N);
    return 0;
}
