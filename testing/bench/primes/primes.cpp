/* primes (C++): the same eight trial-division variants as primes.c, as the
 * methods of a class (built by v32c++ -> C -> the stock compiler).  The sum
 * of the eight counts goes to `result` (8 * pi(N); 776 for N = 512).
 */
#define N 512

int result;

class Primes {
public:
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
};

int main()
{
    Primes p;
    result = p.brute(N) + p.brute_b(N) + p.brute_o(N) + p.brute_s(N)
           + p.brute_bs(N) + p.brute_bo(N) + p.brute_os(N) + p.brute_bos(N);
    return 0;
}
