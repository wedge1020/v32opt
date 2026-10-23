/* Self-checking test support.  RAM[0] = failures (must be 0), RAM[1] = number
 * of checks run (must be > 0), RAM[2..17] = line numbers of the first failures. */
int __fails;
int __checks;
int __lines[16];

void __check(int ok, int line)
{
    __checks++;
    if (!ok) {
        if (__fails < 16)
            __lines[__fails] = line;
        __fails++;
    }
}

#define CHECK(c) __check((c) ? 1 : 0, __LINE__)

void error_handler(void)
{
    __fails = 9999;
}
