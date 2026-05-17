# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```
make              # build the simulator (produces build/my_rv32)
make run          # build and run with tests/firmware.hex
make test         # build and run all four test hex files
make clean        # remove build/ directory
```

Requires SystemC 2.3.4 at `$HOME/opt/systemc-2.3.4`. Compiler: g++ with C++17. The binary accepts a single optional argument: path to an Intel HEX file.

## Directory Structure

```
main.cpp                          top-level wiring (sc_main)
cpu/rv32-lt/                      RV32 loosely-timed core
  core.h/cpp                      CPU SC_MODULE, fetch loop
  base_isa.h                      Instruction decode + Executor (pure callbacks)
  registers.h                     x0-x31 + PC, SP init, dump()
cache/                            configurable cache
  cache.h/cpp                     write-through, LRU, hit/miss stats
interconnect/                     N:M address router
  interconnect.h/cpp              multi-passthrough target -> per-target initiators
mem/                              main memory
  memory.h/cpp                    8MB SRAM, simple_target_socket, HEX loader
periph/dma/                       DMA controller
  dma.h/cpp                       initiator (data) + target (MMIO registers)
periph/display/                   display controller
  display.h/cpp                   initiator (framebuffer read) + target (MMIO)
tests/                            hand-written Intel HEX test programs
```

## Architecture

A **RISC-V RV32I loosely-timed (LT) SoC simulator** built on **SystemC + TLM-2.0**. All code in `riscv_soc_tlm` namespace. Uses `b_transport` (base protocol) throughout; AXI-specific info can be carried via `tlm_extension` without changing socket types.

### Data & Control Flow

```
                DATA PATH                          CONTROL PATH
CPU.instr_socket --> Cache --> Interconnect --> Memory
CPU.data_socket  --> Cache --> Interconnect --> Memory
                                         |--> DMA.target      (MMIO @ 0x10000000)
                                         |--> Display.target  (MMIO @ 0x10001000)

DMA.initiator     --> Interconnect --> Memory                 (data transfer)
Display.initiator --> Interconnect --> Memory                 (framebuffer read)
```

### CPU (`cpu/rv32-lt/`)

- **`core.h/cpp`** — `CPU` SC_MODULE with `instr_socket` (I-fetch) and `data_socket` (load/store). Runs `SC_THREAD` (`CPU_thread`): fetch `->` execute `->` incPC/jump `->` `wait(10, SC_NS)`.
- **`base_isa.h`** — `Instruction` decodes 32-bit raw instruction into opcode, funct3/7, rd/rs1/rs2, immediates. `Executor` uses function pointer callbacks (`mem_read`, `mem_write`, `reg_read`, `reg_write`, `get_pc`, `set_pc`, `inc_pc`, `dump`) — no direct dependency on Registers or any socket. CPU provides static callback methods.
- **`registers.h`** — 32-entry register file with ABI documentation. `x0` hardwired to zero. PC starts at `0x80000000`, SP at `0x800000` (top of 8MB).

### Cache (`cache/`)

- Configurable via `CacheConfig { size, line_size, associativity }`. Write-through, write-allocate on read miss. LRU replacement per set. Hit/miss counters.
- `multi_passthrough_target_socket` on CPU side (both instr and data bind here), `simple_initiator_socket` on bus side.
- Unified topology (default): one Cache for I+D. Separated: two Cache instances for I-Cache + D-Cache (swap via commented block in `main.cpp`).

### Interconnect (`interconnect/`)

- `multi_passthrough_target_socket` — all initiators (Cache, DMA, Display) enter here.
- Per-target `simple_initiator_socket` members: `mem_socket`, `dma_mmio_socket`, `display_mmio_socket`.
- `b_transport` iterates `regions[]` vector, forwards to matching socket.

### Memory (`mem/`)

- 8MB byte array. `simple_target_socket`. `loadHex()` parses Intel HEX (types 00/01/02/04) and returns entry PC. Byte-copy reads/writes, handles any `data_length`.

### DMA (`periph/dma/`)

- `target_socket` for MMIO: SRC_ADDR (0x00/0x04), DST_ADDR (0x08/0x0C), SIZE (0x10), CTRL (0x14, bit0=start).
- `initiator_socket` for data movement. `SC_THREAD` polls CTRL, does word-by-word `b_transport` READ+WRITE transfer.

### Display (`periph/display/`)

- `target_socket` for MMIO: FB_ADDR (0x00/0x04), WIDTH (0x08), HEIGHT (0x0C), CTRL (0x10, bit0=enable).
- `initiator_socket` for framebuffer read. `SC_THREAD` reads pixels, outputs ASCII (`#` or `.`) to stdout every 100us.

### Address Map

| Range | Size | Target |
|---|---|---|
| `0x00000000` | 8MB | Memory (low alias) |
| `0x80000000` | 8MB | Memory |
| `0x10000000` | 4KB | DMA MMIO |
| `0x10001000` | 4KB | Display MMIO |

### Tests

Four hand-written Intel HEX test programs in `tests/`:
- `test_alu.hex` — ADDI, ADD, SRLI, XOR, ECALL
- `test_mem.hex` — SW, LW, ECALL
- `test_loop.hex` — ADDI, ADD, ADDI, BNE loop, ECALL
- `test_call.hex` — ADDI, JAL, JALR (ret), ECALL

Each test ends with ECALL `->` `sc_stop()` `->` register dump. Tests pass if exit code is 0.
