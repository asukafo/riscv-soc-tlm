// C program running on our RV32 simulator
// Demonstrates: arithmetic, loops, function calls, recursive Fibonacci, memory I/O

int add(int a, int b) { return a + b; }

int sum_to_n(int n)
{
    int s = 0;
    for (int i = 1; i <= n; i++) s += i;
    return s;
}

int fib(int n)
{
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

void main(void)
{
    int r1 = add(10, 32);          // 42  → mem[0x100]
    int r2 = sum_to_n(5);          // 15  → mem[0x104]
    int r3 = r1 + r2;              // 57  → mem[0x108]
    int r4 = fib(7);               // 13  → mem[0x10C]
    int r5 = fib(4);               // 3   → mem[0x110]

    *(volatile int*)0x100 = r1;
    *(volatile int*)0x104 = r2;
    *(volatile int*)0x108 = r3;
    *(volatile int*)0x10C = r4;
    *(volatile int*)0x110 = r5;
}
