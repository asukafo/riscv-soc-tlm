#ifndef __SOC_COMMON_H__
#define __SOC_COMMON_H__

#include <cstdint>
#include <vector>

#include "tlm.h"
#include "tlm_utils/instance_specific_extensions.h"

namespace riscv_soc_tlm
{

// ═══════════════════════════════════════════════════════════════════════
// SoC Memory Manager — Clause 14.6, 14.7
//
// Object pool for tlm_generic_payload.  SystemC is cooperative
// single-threaded (SC_THREAD is a coroutine, not an OS thread), so
// no std::mutex is needed here.
// ═══════════════════════════════════════════════════════════════════════
class SoCMM : public tlm::tlm_mm_interface
{
public:
    SoCMM() { pool.reserve(64); }

    /// Return a recycled or newly-allocated transaction object.
    tlm::tlm_generic_payload* allocate()
    {
        if (pool.empty())
        {
            return new tlm::tlm_generic_payload(this);
        }
        tlm::tlm_generic_payload* gp = pool.back();
        pool.pop_back();
        return gp;
    }

    /// Clause 14.7: reset() releases auto_extensions automatically.
    void free(tlm::tlm_generic_payload* gp) override
    {
        gp->reset();                               // auto-extensions freed here
        gp->set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        pool.push_back(gp);
    }

    size_t pool_size() const { return pool.size(); }

private:
    std::vector<tlm::tlm_generic_payload*> pool;
};


// ═══════════════════════════════════════════════════════════════════════
// AXI4 Extension — Clause 14.12
//
// Carries AXI4-specific attributes (burst type, length, size, ID).
// AT path: attach with trans->set_auto_extension(new AXI4Ext()).
// LT path: create locally and extend manually.
// ═══════════════════════════════════════════════════════════════════════
struct AXI4Ext : tlm::tlm_extension<AXI4Ext>
{
    uint8_t  burst_type = 1;   // 0=FIXED  1=INCR  2=WRAP
    uint8_t  burst_len  = 0;   // actual beats = burst_len + 1
    uint8_t  burst_size = 2;   // 2^burst_size bytes per beat
    uint8_t  axprot     = 0;   // protection bits
    uint16_t axid       = 0;   // transaction ID

    tlm::tlm_extension_base* clone() const override
    {
        return new AXI4Ext(*this);
    }

    void copy_from(tlm::tlm_extension_base const& e) override
    {
        *this = static_cast<AXI4Ext const&>(e);
    }
};


// ═══════════════════════════════════════════════════════════════════════
// Hop Context — Clause 14.16 (instance_specific_extension)
//
// Carries original absolute address and master ID across one
// interconnect hop.  When the Interconnect translates an absolute
// address to a slave-relative offset, it saves the original address
// here.  On the response path, it restores the absolute address so
// that the upstream initiator sees the correct address.
// ═══════════════════════════════════════════════════════════════════════
struct HopCtx : tlm_utils::instance_specific_extension<HopCtx>
{
    uint64_t orig_addr = 0;
    int      master_id = 0;
};

/// Convenience accessor for HopCtx — use in both b_transport and
/// nb_transport paths.
static tlm_utils::instance_specific_extension_accessor hop_accessor;

}  // namespace riscv_soc_tlm

#endif
