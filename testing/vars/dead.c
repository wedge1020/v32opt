int g;
int main(void)
{
    int i;
    int junk = 0;
    int sum = 0;
    for (i = 0; i < 10; i++)
    {
        junk = junk + i;     /* only feeds itself: never observed */
        sum = sum + i;
    }
    g = sum;
    return 0;
}
