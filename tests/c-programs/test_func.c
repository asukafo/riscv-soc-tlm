int add(int a, int b) { return a + b; }

int sum_loop(int n)
{
    int sum = 0;
    for (int i = 1; i <= n; i++)
        sum += i;
    return sum;
}

int fib(int n)
{
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

void main(void)
{
    int a = add(10, 32);
    int b = sum_loop(5);
    int c = a + b;
    int f = fib(4);    // small: fib(4) = 3

    *(volatile int*)0x100 = a;
    *(volatile int*)0x104 = b;
    *(volatile int*)0x108 = c;
    *(volatile int*)0x10C = f;
}
