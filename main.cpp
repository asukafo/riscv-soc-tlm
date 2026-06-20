#include <iostream>
#include <string>

#include "cache/cache.h"
#include "cpu/rv32-lt/core.h"
#include "interconnect/interconnect.h"
#include "mem/memory.h"
#include "periph/dma/dma.h"
#include "periph/display/display.h"
#include "periph/fpu/fpu.h"
#include "periph/clint/clint.h"
#include "tlm/soc_common.h"
#include "tlm_core/tlm_2/tlm_quantum/tlm_global_quantum.h"

using namespace riscv_soc_tlm;

int sc_main(int argc, char* argv[])
{
    std::string filename = "firmware.hex";
    if (argc > 1)
    {
        filename = argv[1];
    }

    std::cout << "Loading " << filename << std::endl;

    // ── Clause 16.4: set global quantum for LT temporal decoupling ───
    // Use 100ns quantum for responsive AT/LT interleaving
    tlm::tlm_global_quantum::instance().set(sc_core::sc_time(100, sc_core::SC_NS));

    // ── Clause 14.6: create shared memory manager for AT transactions ─
    SoCMM soc_mm;

    // ── Create modules ────────────────────────────────────────────────
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

    // ── Cache configuration ──────────────────────────────────────────
    CacheConfig cache_cfg;
    cache_cfg.size = 4096;
    cache_cfg.line_size = 16;
    cache_cfg.associativity = 4;

    // ── Topology 1: Unified I/D Cache (default) ──────────────────────
    Cache unified_cache("unified_cache", cache_cfg);

    cpu.instr_socket.bind(unified_cache.target_socket);
    cpu.data_socket.bind(unified_cache.target_socket);
    unified_cache.initiator_socket.bind(interconnect.targ_socket);

    // MMIO ranges bypass cache (peripherals are not cacheable)
    unified_cache.add_mmio_bypass(0x10000000, 0x10002FFF);  // DMA + Display + FPU
    unified_cache.add_mmio_bypass(0x02000000, 0x0200FFFF);  // CLINT

    // ── DMA (MMIO via LT, data movement via AT) ──────────────────────
    DMA dma("dma");
    dma.setMM(&soc_mm);   // Clause 14.6: memory manager for AT transactions
    dma.initiator_socket.bind(interconnect.targ_socket);

    // ── Display (MMIO via LT, framebuffer reads via AT) ─────────────
    Display display("display");
    display.setMM(&soc_mm);  // multi-initiator AT verification
    display.initiator_socket.bind(interconnect.targ_socket);

    // ── FPU ──────────────────────────────────────────────────────────
    FPU fpu("fpu");

    // ── CLINT ────────────────────────────────────────────────────────
    CLINT clint("clint");
    cpu.setCLINT(&clint);

    // ═════════════════════════════════════════════════════════════════
    // Interconnect: allocate downstream ports and map address regions
    //
    // add_target() returns a port index for the init_socket.
    // Multiple address ranges can map to the same port (e.g. both
    // low and high aliases for Memory).
    // ═════════════════════════════════════════════════════════════════

    int mem_port   = interconnect.add_target();   // port 0
    int dma_port   = interconnect.add_target();   // port 1
    int disp_port  = interconnect.add_target();   // port 2
    int fpu_port   = interconnect.add_target();   // port 3
    int clint_port = interconnect.add_target();   // port 4

    // Map address ranges to ports (multiple regions per port OK)
    interconnect.map(0x00000000, Memory::SIZE, mem_port);    // memory low alias
    interconnect.map(0x80000000, Memory::SIZE, mem_port);    // memory primary
    interconnect.map(0x10000000, 0x1000, dma_port);          // DMA MMIO
    interconnect.map(0x10001000, 0x1000, disp_port);         // Display MMIO
    interconnect.map(0x10002000, 0x1000, fpu_port);          // FPU MMIO
    interconnect.map(0x02000000, 0x10000, clint_port);       // CLINT

    // ── Bind downstream targets to init_socket ports ─────────────────
    interconnect.init_socket.bind(memory.socket);        // port 0
    interconnect.init_socket.bind(dma.target_socket);    // port 1
    interconnect.init_socket.bind(display.target_socket);// port 2
    interconnect.init_socket.bind(fpu.target_socket);    // port 3
    interconnect.init_socket.bind(clint.target_socket);  // port 4

    // ── Topology 2: Separated I-Cache + D-Cache ─────────────────────
    // Comment out Topology 1 and uncomment this block to switch:
    //
    // Cache icache("icache", cache_cfg);
    // Cache dcache("dcache", cache_cfg);
    //
    // cpu.instr_socket.bind(icache.target_socket);
    // cpu.data_socket.bind(dcache.target_socket);
    // icache.initiator_socket.bind(interconnect.targ_socket);
    // dcache.initiator_socket.bind(interconnect.targ_socket);

    std::cout << "Starting simulation..." << std::endl;
    sc_core::sc_start();

    std::cout << "\nSimulation finished." << std::endl;
    std::cout << "\n=== Unified Cache Stats ===" << std::endl;
    std::cout << "Hits: " << unified_cache.hits()
              << "  Misses: " << unified_cache.misses()
              << "  MMIO bypass: " << unified_cache.mmio_bypass() << std::endl;

    // Clause 14.6: verify SoCMM pool — non-empty means transactions
    // were properly reclaimed
    std::cout << "\n=== SoCMM Pool ===" << std::endl;
    std::cout << "Transactions in pool: " << soc_mm.pool_size() << std::endl;
    if (soc_mm.pool_size() > 0)
        std::cout << "OK: AT transaction pool is non-empty (transactions recycled correctly)"
                  << std::endl;

    // ── DMA test result verification ─────────────────────────────────
    // DMA test writes pass/fail to address 0x200:
    //   0 = pass, 1 = data mismatch, 2 = DMA timeout
    // Use transport_dbg (Clause 11.4) for post-simulation memory peek
    {
        uint32_t dma_result = 0;
        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(0x200);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&dma_result));
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        (*interconnect.init_socket[0]).transport_dbg(trans);
        std::cout << "\n=== DMA Test Result ===" << std::endl;
        std::cout << "Value @ 0x200 = " << dma_result << std::endl;
        if (dma_result == 0)
            std::cout << "PASS: DMA AT transfer data verified correctly" << std::endl;
        else if (dma_result == 1)
            std::cout << "FAIL: DMA AT transfer data mismatch" << std::endl;
        else if (dma_result == 2)
            std::cout << "FAIL: DMA timeout (CPU poll exhausted)" << std::endl;
        else
            std::cout << "INFO: No DMA test result (test not run)" << std::endl;
    }

    return 0;
}
