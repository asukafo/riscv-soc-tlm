// Minimal test — just ADDI + ECALL, no function calls
void main(void)
{
    int a = 10;
    int b = 20;
    int c = a + b;
    // Store result so it shows in register dump
    *(volatile int*)0x100 = c;
}
