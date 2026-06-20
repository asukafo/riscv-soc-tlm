#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <cstdint>
#include <string>

#include "systemc"
#include "tlm.h"
#include "tlm_utils/peq_with_cb_and_phase.h"
#include "tlm_utils/simple_target_socket.h"

namespace riscv_soc_tlm
{

class Memory : public sc_core::sc_module
{
public:
    static constexpr uint32_t SIZE = 8 * 1024 * 1024;  // 8MB

    tlm_utils::simple_target_socket<Memory> socket;

    Memory(sc_core::sc_module_name name);

    uint32_t loadHex(const std::string& filename);
    uint32_t loadELF(const std::string& filename);

    // Access latencies (configurable)
    sc_core::sc_time read_latency;
    sc_core::sc_time write_latency;

    // DRAM refresh timing (P3.15)
    sc_core::sc_time tREFI;  // refresh interval (default 7.8 µs)
    sc_core::sc_time tRFC;   // refresh cycle time (default 70 ns)

private:
    // ── DRAM refresh (P3.15) ───────────────────────────────────────
    void refresh_thread();
    bool m_in_refresh = false;
    sc_core::sc_time m_refresh_start;
    sc_core::sc_time refresh_penalty() const;  // remaining tRFC

    // ── LT path (blocking) ──────────────────────────────────────────
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);

    // ── AT path (non-blocking) — Clause 15 ──────────────────────────
    tlm::tlm_sync_enum nb_transport_fw(tlm::tlm_generic_payload& trans,
                                       tlm::tlm_phase& phase,
                                       sc_core::sc_time& delay);

    // PEQ callback — SC_METHOD, cannot call wait() — Clause 16.3
    void peq_cb(tlm::tlm_generic_payload& trans, const tlm::tlm_phase& phase);
    tlm_utils::peq_with_cb_and_phase<Memory> peq;

    // ── DMI — Clause 11.3 ───────────────────────────────────────────
    bool get_direct_mem_ptr(tlm::tlm_generic_payload& trans,
                            tlm::tlm_dmi& dmi_data);

    // ── Debug transport — Clause 11.4 ───────────────────────────────
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);

    // ── Helpers ─────────────────────────────────────────────────────
    uint32_t toOffset(uint64_t addr) const
    {
        int64_t offset = (int64_t)(addr - base_addr);
        if (offset >= 0 && offset < (int64_t)SIZE)
            return (uint32_t)offset;
        if (addr < SIZE)
            return (uint32_t)addr;
        return SIZE;
    }

    // Clause 14.9/14.10: streaming-width-aware access with byte-enable
    void accessMem(uint64_t addr, unsigned char* ptr, uint32_t len,
                   tlm::tlm_command cmd, uint32_t streaming_width,
                   unsigned char* be_ptr, uint32_t be_len);

    uint8_t mem[SIZE];
    uint64_t base_addr;
};

}  // namespace riscv_soc_tlm

#endif
