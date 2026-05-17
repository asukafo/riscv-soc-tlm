#include <iostream>
#include <string>

#include "cache/cache.h"
#include "cpu/rv32-lt/core.h"
#include "interconnect/interconnect.h"
#include "mem/memory.h"

using namespace riscv_soc_tlm;

int sc_main(int argc, char* argv[])
{
    std::string hexfile = "firmware.hex";
    if (argc > 1)
    {
        hexfile = argv[1];
    }

    std::cout << "Loading " << hexfile << std::endl;

    Memory memory("memory");
    uint32_t start_pc = memory.loadHex(hexfile);

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
    cpu.mem_if.socket.bind(unified_cache.target_socket);
    unified_cache.initiator_socket.bind(interconnect.target_socket);

    // ── Topology 2: Separated I-Cache + D-Cache ───────────────────────
    // Comment out Topology 1 and uncomment this block to switch:
    //
    // Cache icache("icache", cache_cfg);
    // Cache dcache("dcache", cache_cfg);
    //
    // cpu.instr_socket.bind(icache.target_socket);
    // cpu.mem_if.socket.bind(dcache.target_socket);
    // icache.initiator_socket.bind(interconnect.target_socket);
    // dcache.initiator_socket.bind(interconnect.target_socket);

    interconnect.map(0x00000000, Memory::SIZE, interconnect.mem_socket);
    interconnect.map(0x80000000, Memory::SIZE, interconnect.mem_socket);

    interconnect.mem_socket.bind(memory.socket);

    std::cout << "Starting simulation..." << std::endl;
    sc_core::sc_start();

    std::cout << "Simulation finished." << std::endl;
    std::cout << "\n=== Unified Cache Stats ===" << std::endl;
    std::cout << "Hits: " << unified_cache.hits()
              << "  Misses: " << unified_cache.misses() << std::endl;

    return 0;
}
