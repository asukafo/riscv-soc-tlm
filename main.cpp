#include <iostream>
#include <string>

#include "cache/cache.h"
#include "cpu/rv32-lt/core.h"
#include "interconnect/interconnect.h"
#include "mem/memory.h"
#include "periph/dma/dma.h"
#include "periph/display/display.h"
#include "periph/fpu/fpu.h"

using namespace riscv_soc_tlm;

int sc_main(int argc, char* argv[])
{
    std::string filename = "firmware.hex";
    if (argc > 1)
    {
        filename = argv[1];
    }

    std::cout << "Loading " << filename << std::endl;

    Memory memory("memory");
    uint32_t start_pc;
    if (filename.size() >= 4 && filename.substr(filename.size() - 4) == ".elf")
    {
        start_pc = memory.loadELF(filename);
    }
    else
    {
        start_pc = memory.loadHex(filename);
    }

    Interconnect interconnect("interconnect");

    CPU cpu("cpu", start_pc);

    // ── Cache configuration ────────────────────────────────────────────
    CacheConfig cache_cfg;
    cache_cfg.size = 4096;
    cache_cfg.line_size = 16;
    cache_cfg.associativity = 4;

    // ── Topology 1: Unified I/D Cache (default) ────────────────────────
    Cache unified_cache("unified_cache", cache_cfg);

    cpu.instr_socket.bind(unified_cache.target_socket);
    cpu.data_socket.bind(unified_cache.target_socket);
    unified_cache.initiator_socket.bind(interconnect.target_socket);

    // ── DMA ────────────────────────────────────────────────────────────
    DMA dma("dma");
    dma.initiator_socket.bind(interconnect.target_socket);
    interconnect.dma_mmio_socket.bind(dma.target_socket);

    // ── Display ────────────────────────────────────────────────────────
    Display display("display");
    display.initiator_socket.bind(interconnect.target_socket);
    interconnect.display_mmio_socket.bind(display.target_socket);

    // ── FPU ────────────────────────────────────────────────────────────
    FPU fpu("fpu");
    interconnect.fpu_mmio_socket.bind(fpu.target_socket);

    // ── Topology 2: Separated I-Cache + D-Cache ───────────────────────
    // Comment out Topology 1 and uncomment this block to switch:
    //
    // Cache icache("icache", cache_cfg);
    // Cache dcache("dcache", cache_cfg);
    //
    // cpu.instr_socket.bind(icache.target_socket);
    // cpu.data_socket.bind(dcache.target_socket);
    // icache.initiator_socket.bind(interconnect.target_socket);
    // dcache.initiator_socket.bind(interconnect.target_socket);

    // ── Address map ────────────────────────────────────────────────────
    interconnect.map(0x00000000, Memory::SIZE, interconnect.mem_socket);
    interconnect.map(0x80000000, Memory::SIZE, interconnect.mem_socket);
    interconnect.map(0x10000000, 0x1000, interconnect.dma_mmio_socket);
    interconnect.map(0x10001000, 0x1000, interconnect.display_mmio_socket);
    interconnect.map(0x10002000, 0x1000, interconnect.fpu_mmio_socket);

    interconnect.mem_socket.bind(memory.socket);

    std::cout << "Starting simulation..." << std::endl;
    sc_core::sc_start();

    std::cout << "Simulation finished." << std::endl;
    std::cout << "\n=== Unified Cache Stats ===" << std::endl;
    std::cout << "Hits: " << unified_cache.hits()
              << "  Misses: " << unified_cache.misses() << std::endl;

    return 0;
}
