// Bare-metal C test — runs on our RV32 simulator

volatile int *const UART = (volatile int*)0x10000000;

int add(int a, int b) { return a + b; }

void store_data(volatile int* addr, int val) { *addr = val; }

int load_data(volatile int* addr) { return *addr; }

int sum_loop(int n)
{
    int sum = 0;
    for (int i = 1; i <= n; i++)
    {
        sum += i;
    }
    return sum;
}

int fibonacci(int n)
{
    if (n <= 1) return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

void main(void)
{
    int a = add(10, 32);          // x1 = 42
    int b = sum_loop(5);          // x2 = 15
    int c = a + b;                // x3 = 57

    store_data((volatile int*)0x200, a);
    int d = load_data((volatile int*)0x200);  // x4 = 42

    int fib = fibonacci(6);       // x5 = 8

    // Prevent optimization: write results to memory
    store_data((volatile int*)0x100, a);
    store_data((volatile int*)0x104, b);
    store_data((volatile int*)0x108, c);
    store_data((volatile int*)0x10C, d);
    store_data((volatile int*)0x110, fib);
}
