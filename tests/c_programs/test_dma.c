// DMA AT path test — exercises nb_transport_fw/nb_transport_bw
//
// This program:
//   1. Writes known data to a source buffer
//   2. Programs the DMA controller via MMIO to copy it
//   3. Polls DMA status until complete
//   4. Verifies destination data matches source
//   5. Writes 0 (pass) or 1 (fail) to address 0x200

// DMA MMIO register offsets (from base 0x10000000)
#define DMA_SRC_LO   (*(volatile unsigned int*)0x10000000)
#define DMA_SRC_HI   (*(volatile unsigned int*)0x10000004)
#define DMA_DST_LO   (*(volatile unsigned int*)0x10000008)
#define DMA_DST_HI   (*(volatile unsigned int*)0x1000000C)
#define DMA_SIZE     (*(volatile unsigned int*)0x10000010)
#define DMA_CTRL     (*(volatile unsigned int*)0x10000014)

#define SRC_ADDR  0x80001000
#define DST_ADDR  0x80002000
#define COPY_SIZE 64   // bytes

void main(void)
{
    int pass = 1;

    // 1. Write source data pattern
    volatile unsigned int* src = (volatile unsigned int*)SRC_ADDR;
    for (int i = 0; i < COPY_SIZE / 4; i++)
    {
        src[i] = i * 2 + 0xDEAD0000;
    }

    // 2. Clear destination
    volatile unsigned int* dst = (volatile unsigned int*)DST_ADDR;
    for (int i = 0; i < COPY_SIZE / 4; i++)
    {
        dst[i] = 0;
    }

    // 3. Program DMA: source address
    DMA_SRC_LO = SRC_ADDR;
    DMA_SRC_HI = 0;
    DMA_DST_LO = DST_ADDR;
    DMA_DST_HI = 0;

    // 4. Program DMA: size and start
    DMA_SIZE = COPY_SIZE;

    // Memory barrier: ensure all writes are visible before starting DMA
    asm volatile("fence" ::: "memory");

    DMA_CTRL = 1;  // bit0 = start

    // 5. Poll until DMA completes (CTRL bit0 cleared by DMA)
    int timeout = 10000;
    while ((DMA_CTRL & 1) && --timeout > 0)
    {
        asm volatile("nop");
    }

    if (timeout <= 0)
    {
        // DMA timeout — write error code
        *(volatile int*)0x200 = 2;
        return;
    }

    // Wait a bit more for AT transactions to fully complete
    for (volatile int i = 0; i < 100; i++)
    {
        asm volatile("nop");
    }

    // 6. Verify destination matches source
    for (int i = 0; i < COPY_SIZE / 4; i++)
    {
        if (dst[i] != src[i])
        {
            pass = 0;
            break;
        }
    }

    // 7. Write result to known address for testbench to check
    *(volatile int*)0x200 = pass ? 0 : 1;
}
