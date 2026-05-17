int add(int a, int b) { return a + b; }

int sum_loop(int n)
{
    int sum = 0;
    for (int i = 1; i <= n; i++)
    {
        sum += i;
    }
    return sum;
}

void main(void)
{
    int a = add(10, 32);         // x10 = 42
    int b = sum_loop(5);         // x10 = 15
    int c = a + b;               // x10 = 57

    *(volatile int*)0x100 = a;
    *(volatile int*)0x104 = b;
    *(volatile int*)0x108 = c;
}
