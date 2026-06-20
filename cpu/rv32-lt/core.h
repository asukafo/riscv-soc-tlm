#ifndef __CPU_H__
#define __CPU_H__

#include <cstdint>
#include <vector>

#include "cpu/rv32-lt/base_isa.h"
#include "cpu/rv32-lt/registers.h"
#include "systemc"
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/tlm_quantumkeeper.h"

namespace riscv_soc_tlm
{

class CLINT;  // forward declaration

class CPU : public sc_core::sc_module
{
public:
    tlm_utils::simple_initiator_socket<CPU> instr_socket;
    tlm_utils::simple_initiator_socket<CPU> data_socket;
    Registers regs;

    // Clause 16.4: quantum keeper for temporal decoupling
    tlm_utils::tlm_quantumkeeper qk;

    CPU(sc_core::sc_module_name name, uint32_t start_pc);

    void setCLINT(CLINT* clint) { m_clint = clint; }

private:
    // ── DMI cache entry ──────────────────────────────────────────────
    struct DmiCache
    {
        uint64_t       base;
        uint64_t       end;
        unsigned char* ptr;
        sc_core::sc_time read_latency;
        sc_core::sc_time write_latency;
        bool           can_read;
        bool           can_write;
    };

    Executor executor;

    void CPU_thread();
    uint32_t fetchInstruction();
    void checkInterrupts();

    // DMI cache management — Clause 11.3
    std::vector<DmiCache> dmi_cache;
    DmiCache* find_dmi(uint64_t addr);
    void add_dmi(uint64_t addr, tlm_utils::simple_initiator_socket<CPU>& sock);
    void invalidate_direct_mem_ptr(sc_dt::uint64 start, sc_dt::uint64 end);

    // MMIO ranges force quantum sync so peripherals (DMA) can make progress
    static bool is_mmio(uint64_t addr);

    // Callbacks for Executor — memory access
    static uint32_t memRead(void* ctx, uint64_t addr, int size);
    static void memWrite(void* ctx, uint64_t addr, uint32_t data, int size);

    // Callbacks for Executor — register access
    static uint32_t regRead(void* ctx, uint32_t reg_num);
    static void regWrite(void* ctx, uint32_t reg_num, uint32_t value);
    static uint32_t regGetPC(void* ctx);
    static void regSetPC(void* ctx, uint32_t pc);
    static void regIncPC(void* ctx);
    static void regDump(void* ctx);

    // Callbacks for Executor — CSR access
    static uint32_t csrRead(void* ctx, uint32_t addr);
    static void csrWrite(void* ctx, uint32_t addr, uint32_t value);

    CLINT* m_clint;
};

}  // namespace riscv_soc_tlm

#endif
