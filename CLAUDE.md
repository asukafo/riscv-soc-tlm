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

## Architecture

This is a **minimal RV32I loosely-timed (LT) instruction set simulator** built on **SystemC + TLM-2.0**. All code lives in the `riscv_soc_tlm` namespace.

### Top-level wiring (`main.cpp`)

`sc_main` instantiates one `Memory`, one `Interconnect`, and one `CPU`. The CPU's unified `mem_if.socket` binds to the `Interconnect::target_socket`. The interconnect routes transactions by address: `[0x00000000, 8MB)` and `[0x80000000, 8MB)` both map to `Memory::socket`. The memory loads a HEX file via `loadHex()`, which returns the entry PC; that value is passed to the CPU constructor.

### CPU (`cpu/rv32-lt/`)

- **`cpu.h/cpp`** — The `CPU` SC_MODULE. Runs a single `SC_THREAD` (`CPU_thread`) that loops forever: fetch instruction via `mem_if.fetchInstruction(pc)`, execute, increment PC (or let branch/jump override), then `wait(10, SC_NS)`.
- **`base_isa.h`** — Two key classes:
  - `Instruction` — Decodes the 32-bit raw instruction into opcode, funct3/7, rd/rs1/rs2, and immediate fields (I, S, B, U, J types per the RISC-V spec).
  - `Executor` — Holds pointers to `Registers` and `MemoryInterface`. The `execute()` method dispatches via opcode switch to inline handler methods (LUI, JAL, BEQ, LW, SW, ADDI, ADD, ECALL, etc.). Returns `true` if PC was explicitly set (branch/jump), `false` otherwise.
- **`registers.h`** — 32-entry register file (`x0`–`x31`), with ABI aliases (`zero`, `ra`, `sp`, `a0`, etc.). `x0` is hardwired to zero on writes. PC starts at `0x80000000`. `dump()` prints all regs + PC in hex.
- **`memory_interface.h/cpp`** — Wraps a single `simple_initiator_socket` for all CPU memory access. Methods: `readDataMem(addr, size)` / `writeDataMem(addr, data, size)` for load/store, and `fetchInstruction(addr)` for instruction fetch — each issuing a TLM `b_transport`.

### Memory (`mem/`)

- **`memory.h/cpp`** — 8MB byte-array memory, base address `0x80000000`. Provides a `simple_target_socket`. The `b_transport` callback handles READ/WRITE commands by indexing into the byte array. `loadHex()` parses Intel HEX format (record types 00/01/02/04) and returns the entry PC (address of the first data record).

### Interconnect (`interconnect/`)

- **`interconnect.h/cpp`** — Address-routing bus module. Provides a `multi_passthrough_target_socket` for all initiators (CPU, future DMA engines) and per-target `simple_initiator_socket` members (currently `mem_socket` for Memory). The `map(base, size, socket)` method registers address regions; `b_transport` iterates regions and forwards matching transactions. Unmatched addresses get `TLM_ADDRESS_ERROR_RESPONSE`.

### TLM-2.0 sockets

CPU connects to Interconnect via a single `MemoryInterface::socket`. The Interconnect routes to Memory via `mem_socket`. All communication uses `b_transport` (loosely-timed coding style), blocking transport with zero delay. Protocol strategy: base TLM-2.0 protocol types; future AXI-specific info (burst, cache, prot) will be carried via `tlm_extension` on the generic payload without changing socket types.

### Tests

Four hand-written Intel HEX test programs in `tests/`:
- `test_alu.hex` — ADDI, ADD, SRLI, XOR, then ECALL
- `test_mem.hex` — SW, LW, then ECALL
- `test_loop.hex` — ADDI, ADD, ADDI with BNE loop, then ECALL
- `test_call.hex` — ADDI, JAL, JALR (ret), then ECALL

Each test ends with an ECALL instruction that triggers `sc_stop()` and a register dump. Tests pass if the simulator exits cleanly (return code 0).
