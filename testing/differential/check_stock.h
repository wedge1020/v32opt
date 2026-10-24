int __fails;
int __checks;
int[16] __lines;

void __check(int ok, int line)
{
    __checks++;
    if (!ok)
    {
        if (__fails < 16)
            __lines[__fails] = line;
        __fails++;
    }
}

void error_handler(void)
{
    __fails = 9999;
}
