#ifndef __INTERCONNECT_H__
#define __INTERCONNECT_H__

#include <cstdint>
#include <vector>

#include "systemc"
#include "tlm.h"
#include "tlm_utils/multi_passthrough_target_socket.h"
#include "tlm_utils/multi_passthrough_initiator_socket.h"
#include "tlm_utils/instance_specific_extensions.h"
#include "tlm/soc_common.h"

namespace riscv_soc_tlm
{

class Interconnect : public sc_core::sc_module
{
public:
    // BUSWIDTH must match the default socket width (32) for binding compatibility
    static constexpr int MAX_INITIATORS = 32;
    static constexpr int MAX_TARGETS   = 32;

    // Upstream: multiple initiators bind here
    tlm_utils::multi_passthrough_target_socket<Interconnect, MAX_INITIATORS> targ_socket;

    // Downstream: connects to multiple targets
    tlm_utils::multi_passthrough_initiator_socket<Interconnect, MAX_TARGETS> init_socket;

    Interconnect(sc_core::sc_module_name name);

    /// Allocate a new downstream port. Returns the port index to bind to.
    int add_target();

    /// Map an absolute address range to a downstream port.
    void map(uint64_t base, uint64_t size, int port);

private:
    struct AddressRegion
    {
        uint64_t base;
        uint64_t size;
        int      port;    // index into init_socket
    };
    std::vector<AddressRegion> regions;
    int next_port = 0;

    // ── Forward path (upstream → downstream) ──────────────────────────
    void          b_transport(int id, tlm::tlm_generic_payload& trans,
                             sc_core::sc_time& delay);

    tlm::tlm_sync_enum nb_transport_fw(int id, tlm::tlm_generic_payload& trans,
                                        tlm::tlm_phase& phase,
                                        sc_core::sc_time& delay);

    bool          get_direct_mem_ptr(int id, tlm::tlm_generic_payload& trans,
                                     tlm::tlm_dmi& dmi_data);

    unsigned int  transport_dbg(int id, tlm::tlm_generic_payload& trans);

    // ── Backward path (downstream → upstream) ─────────────────────────
    tlm::tlm_sync_enum nb_transport_bw(int port, tlm::tlm_generic_payload& trans,
                                        tlm::tlm_phase& phase,
                                        sc_core::sc_time& delay);

    void invalidate_direct_mem_ptr(int port, sc_dt::uint64 start,
                                   sc_dt::uint64 end);

    // ── Helpers ───────────────────────────────────────────────────────
    int find_port(uint64_t addr) const;

    // Clause 14.16: hop context accessor for address save/restore
    tlm_utils::instance_specific_extension_accessor accessor;
};

}  // namespace riscv_soc_tlm

#endif
