CROSS = riscv64-elf
CC    = $(CROSS)-gcc
OBJCOPY = $(CROSS)-objcopy
CFLAGS = -march=rv32i -mabi=ilp32 -O1 -nostdlib -nostartfiles -ffreestanding -Wall

all: test_demo.hex

start.o: start.S
	$(CC) $(CFLAGS) -c start.S -o start.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.elf: start.o %.o link.ld
	$(CC) $(CFLAGS) -T link.ld -nostdlib start.o $*.o -o $@

%.hex: %.elf
	$(OBJCOPY) -O ihex $< $@

clean:
	rm -f *.o *.elf *.hex
